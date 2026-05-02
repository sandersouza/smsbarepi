#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "${PROJECT_ROOT}/scripts/sdcard-common.sh"

if [ "${EUID:-$(id -u)}" -eq 0 ]; then
  echo "[ERRO] Este script nao pode ser executado como root."
  exit 1
fi

VOLUME_NAME="${VOLUME_NAME:-RPIBOOT}"
TARGET_VOLUME="${TARGET_VOLUME:-$(sdcard_default_target_volume "$VOLUME_NAME")}"
SD_DEVICE="${SD_DEVICE:-}"
KERNEL_IMG="${KERNEL_IMG:-}"
CONFIG_TXT="${CONFIG_TXT:-${PROJECT_ROOT}/sdcard/config.txt}"
CMDLINE_TXT="${CMDLINE_TXT:-${PROJECT_ROOT}/sdcard/cmdline.txt}"
INITRAMFS_IMG="${INITRAMFS_IMG:-${PROJECT_ROOT}/sdcard/sms.cpio}"
INITRAMFS_SRC_DIR="${INITRAMFS_SRC_DIR:-${PROJECT_ROOT}/roms/initramfs}"
SETTINGS_CFG="${SETTINGS_CFG:-${PROJECT_ROOT}/sdcard/sms.cfg}"
FIRMWARE_DIR="${FIRMWARE_DIR:-${PROJECT_ROOT}/rpi-firmware}"
GAMES_SRC_DIR="${GAMES_SRC_DIR:-${PROJECT_ROOT}/roms/games}"
COPY_GAMES_TO_ROOT="${COPY_GAMES_TO_ROOT:-0}"
DEBUG="${DEBUG:-0}"
SMSBARE_EMBED_INITRAMFS="${SMSBARE_EMBED_INITRAMFS:-0}"
VIDEO_MODE_PRESET="${VIDEO_MODE_PRESET:-auto}"
EJECT_AT_END="${EJECT_AT_END:-0}"

export COPYFILE_DISABLE=1
sdcard_require_prepare_deps

if [ -n "$SD_DEVICE" ] && [ "${SD_DEVICE#/dev/}" = "$SD_DEVICE" ]; then
  SD_DEVICE="/dev/${SD_DEVICE}"
fi

if [ -n "$SD_DEVICE" ]; then
  detected_volume="$(sdcard_discover_mounted_volume_for_disk "$SD_DEVICE" || true)"
  if [ -z "$detected_volume" ]; then
    sdcard_mount_disk_if_needed "$SD_DEVICE"
    detected_volume="$(sdcard_discover_mounted_volume_for_disk "$SD_DEVICE" || true)"
  fi
  if [ -n "$detected_volume" ]; then
    TARGET_VOLUME="$detected_volume"
    echo "[INFO] SD_DEVICE detectado: ${SD_DEVICE} -> ${TARGET_VOLUME}"
  fi
fi

if [ ! -d "$TARGET_VOLUME" ]; then
  echo "[ERRO] Volume nao montado: ${TARGET_VOLUME}"
  if [ -n "$SD_DEVICE" ]; then
    echo "[ERRO] Nao foi possivel resolver volume montado para SD_DEVICE=${SD_DEVICE}."
  fi
  exit 1
fi

is_yes() {
  case "${1:-}" in
    1|y|Y|yes|YES|true|TRUE|sim|SIM|s|S) return 0 ;;
    *) return 1 ;;
  esac
}

resolve_default_kernel_img() {
  local branch=""
  local issue_id=""
  local candidate=""

  if [ -n "$KERNEL_IMG" ]; then
    printf '%s' "$KERNEL_IMG"
    return 0
  fi

  branch="$(git -C "$PROJECT_ROOT" rev-parse --abbrev-ref HEAD 2>/dev/null || true)"
  issue_id="$(printf "%s" "$branch" | sed -n 's/^\([0-9][0-9]*\)-.*/\1/p')"
  if [ -n "$issue_id" ]; then
    candidate="${PROJECT_ROOT}/artifacts/kernel8-issue${issue_id}.img"
    if [ -f "$candidate" ]; then
      printf '%s' "$candidate"
      return 0
    fi
  fi

  for candidate in \
    "${PROJECT_ROOT}/build/src/sms.img" \
    "${PROJECT_ROOT}/build/sms.img" \
    "${PROJECT_ROOT}/artifacts/kernel8-default.img" \
    "${PROJECT_ROOT}/artifacts/kernel8-bootstrap.img" \
    "${PROJECT_ROOT}/build/kernel8.img" \
    "${PROJECT_ROOT}/build/src/kernel8.img" \
    "${PROJECT_ROOT}/src/kernel8.img"
  do
    if [ -f "$candidate" ]; then
      printf '%s' "$candidate"
      return 0
    fi
  done

  printf '%s' "${PROJECT_ROOT}/artifacts/kernel8-default.img"
}

ensure_runtime_root_dirs() {
  mkdir -p "${TARGET_VOLUME}/sms/roms"
  echo "[OK] Diretorio raiz garantido: sms/roms/"
}

resolve_target_devices() {
  if [ -n "$SD_DEVICE" ]; then
    TARGET_DISK_DEVICE="$SD_DEVICE"
  fi

  if sdcard_is_macos; then
    local part_id=""
    local disk_id=""
    part_id="$(diskutil info -plist "$TARGET_VOLUME" 2>/dev/null | plutil -extract DeviceIdentifier raw -o - - 2>/dev/null || true)"
    disk_id="$(diskutil info -plist "$TARGET_VOLUME" 2>/dev/null | plutil -extract ParentWholeDisk raw -o - - 2>/dev/null || true)"
    if [ -n "$part_id" ]; then
      TARGET_PART_DEVICE="/dev/${part_id}"
    fi
    if [ -n "$disk_id" ]; then
      TARGET_DISK_DEVICE="${TARGET_DISK_DEVICE:-/dev/${disk_id}}"
    fi
    return 0
  fi

  if command -v findmnt >/dev/null 2>&1; then
    TARGET_PART_DEVICE="$(findmnt -n -o SOURCE --target "$TARGET_VOLUME" 2>/dev/null | head -n1 || true)"
  fi
  if [ -z "${TARGET_PART_DEVICE:-}" ]; then
    TARGET_PART_DEVICE="$(lsblk -pnro NAME,MOUNTPOINT 2>/dev/null | awk -v target=\"$TARGET_VOLUME\" '$2==target{print $1; exit}' || true)"
  fi
  if [ -n "${TARGET_PART_DEVICE:-}" ] && [ -z "${TARGET_DISK_DEVICE:-}" ]; then
    local parent=""
    parent="$(lsblk -nro PKNAME "$TARGET_PART_DEVICE" 2>/dev/null | head -n1 | tr -d '\n' || true)"
    if [ -n "$parent" ]; then
      TARGET_DISK_DEVICE="/dev/${parent}"
    fi
  fi
}

unmount_target_volume() {
  if [ -n "${TARGET_DISK_DEVICE:-}" ]; then
    sdcard_unmount_disk "$TARGET_DISK_DEVICE"
    return 0
  fi

  if [ -z "${TARGET_PART_DEVICE:-}" ]; then
    return 1
  fi

  if sdcard_is_macos; then
    diskutil unmount "$TARGET_VOLUME" >/dev/null 2>&1 || true
    return 0
  fi

  if command -v udisksctl >/dev/null 2>&1; then
    udisksctl unmount -b "$TARGET_PART_DEVICE" >/dev/null 2>&1 || true
  else
    sudo umount "$TARGET_PART_DEVICE" >/dev/null 2>&1 || true
  fi
}

set_or_append_config_key() {
  local file="$1"
  local key="$2"
  local value="$3"
  if grep -Eq "^[[:space:]]*${key}=" "$file"; then
    sed -E "s|^[[:space:]]*${key}=.*|${key}=${value}|" "$file" > "${file}.new"
    mv "${file}.new" "$file"
  else
    printf '%s=%s\n' "$key" "$value" >> "$file"
  fi
}

drop_config_key_if_present() {
  local file="$1"
  local key="$2"
  if grep -Eq "^[[:space:]]*${key}=" "$file"; then
    sed -E "/^[[:space:]]*${key}=.*/d" "$file" > "${file}.new"
    mv "${file}.new" "$file"
  fi
}

drop_initramfs_line_if_present() {
  local file="$1"
  if grep -Eq '^[[:space:]]*initramfs[[:space:]]+' "$file"; then
    sed -E '/^[[:space:]]*initramfs[[:space:]]+.*/d' "$file" > "${file}.new"
    mv "${file}.new" "$file"
  fi
}

apply_video_mode_preset() {
  local file="$1"
  local preset="$2"
  case "$preset" in
    auto|"")
      drop_config_key_if_present "$file" "hdmi_group"
      drop_config_key_if_present "$file" "hdmi_mode"
      drop_config_key_if_present "$file" "framebuffer_width"
      drop_config_key_if_present "$file" "framebuffer_height"
      ;;
    1080|1080p)
      set_or_append_config_key "$file" "hdmi_group" "1"
      set_or_append_config_key "$file" "hdmi_mode" "16"
      set_or_append_config_key "$file" "framebuffer_width" "1920"
      set_or_append_config_key "$file" "framebuffer_height" "1080"
      ;;
    720|720p|16:9)
      set_or_append_config_key "$file" "hdmi_group" "1"
      set_or_append_config_key "$file" "hdmi_mode" "4"
      set_or_append_config_key "$file" "framebuffer_width" "1280"
      set_or_append_config_key "$file" "framebuffer_height" "720"
      ;;
    768|1024x768|4:3)
      set_or_append_config_key "$file" "hdmi_group" "2"
      set_or_append_config_key "$file" "hdmi_mode" "16"
      set_or_append_config_key "$file" "framebuffer_width" "1024"
      set_or_append_config_key "$file" "framebuffer_height" "768"
      ;;
    *)
      echo "[ERRO] VIDEO_MODE_PRESET invalido: ${preset}"
      exit 1
      ;;
  esac
}

ensure_settings_cfg() {
  if [ -f "$SETTINGS_CFG" ]; then
    return 0
  fi

  mkdir -p "$(dirname "$SETTINGS_CFG")"
  cat > "$SETTINGS_CFG" <<'EOF'
backend_id=sms.smsplus
language=1
overscan=0
scanline=0
color_artifacts=0
machine=0
processor=0
rammapper_kb=8
joy_map_up=16385
joy_map_down=20481
joy_map_left=16384
joy_map_right=20480
joy_map_button1=8192
joy_map_button2=8193
fm_music=1
debugger=0
audio_gain=100
#END
EOF
}

copy_if_changed() {
  local src="$1"
  local dst="$2"
  local label="$3"

  if [ ! -f "$src" ]; then
    echo "[WARN] Ausente: ${src}"
    return 0
  fi

  if [ -f "$dst" ] && cmp -s "$src" "$dst"; then
    echo "[SKIP] ${label} (sem alteracoes)"
    return 0
  fi

  cp "$src" "$dst"
  echo "[COPY] ${label}"
}

copy_games_incremental() {
  if ! is_yes "$COPY_GAMES_TO_ROOT"; then
    return 0
  fi
  sdcard_copy_sms_rom_payload "$GAMES_SRC_DIR" "${TARGET_VOLUME}/sms"
}

cleanup_macos_junk() {
  find "$TARGET_VOLUME" \
    \( -name '.fseventsd' -o -name '.Spotlight-V100' -o -name '.Trashes' -o -name '.Trash' -o -name '.TemporaryItems' -o -name '.DocumentRevisions-V100' -o -name '.AppleDouble' \) -prune \
    -o -name '._*' -type f -delete \
    -o -name '.DS_Store' -type f -delete \
    >/dev/null 2>&1 || true
}

if is_yes "$SMSBARE_EMBED_INITRAMFS"; then
  echo "[INFO] Initramfs embutido/dispensado ativo; nao gerando sms.cpio para update incremental."
elif [ -d "$INITRAMFS_SRC_DIR" ] && [ -x "${PROJECT_ROOT}/scripts/make-initramfs.sh" ]; then
  "${PROJECT_ROOT}/scripts/make-initramfs.sh" "$INITRAMFS_SRC_DIR" "$INITRAMFS_IMG"
fi

KERNEL_IMG="$(resolve_default_kernel_img)"
echo "[INFO] KERNEL_IMG efetivo: ${KERNEL_IMG}"

ensure_settings_cfg
ensure_runtime_root_dirs

if [ ! -f "$KERNEL_IMG" ]; then
  echo "[ERRO] Kernel obrigatorio nao encontrado: ${KERNEL_IMG}"
  echo "[ERRO] Rode 'make' ou informe KERNEL_IMG=/caminho/sms.img antes de atualizar o SD."
  exit 1
fi

config_src="$CONFIG_TXT"
config_tmp=""
if [ -f "$CONFIG_TXT" ]; then
  if [ "$DEBUG" = "1" ] || [ "$VIDEO_MODE_PRESET" != "auto" ] || is_yes "$SMSBARE_EMBED_INITRAMFS"; then
    config_tmp="$(mktemp)"
    cp "$CONFIG_TXT" "$config_tmp"
    config_src="$config_tmp"
  fi
  if is_yes "$SMSBARE_EMBED_INITRAMFS"; then
    drop_initramfs_line_if_present "$config_src"
  fi
  if [ "$DEBUG" = "1" ]; then
    if grep -Eq '^[[:space:]]*disable_splash=' "$config_src"; then
      sed -E 's/^[[:space:]]*disable_splash=.*/disable_splash=1/' "$config_src" > "${config_src}.new"
      mv "${config_src}.new" "$config_src"
    else
      printf '\ndisable_splash=1\n' >> "$config_src"
    fi
  fi
  if [ "$VIDEO_MODE_PRESET" != "auto" ]; then
    apply_video_mode_preset "$config_src" "$VIDEO_MODE_PRESET"
  fi
fi

copy_if_changed "$KERNEL_IMG" "${TARGET_VOLUME}/sms.img" "sms.img"
copy_if_changed "$config_src" "${TARGET_VOLUME}/config.txt" "config.txt"
copy_if_changed "$CMDLINE_TXT" "${TARGET_VOLUME}/cmdline.txt" "cmdline.txt"
if is_yes "$SMSBARE_EMBED_INITRAMFS"; then
  echo "[SKIP] sms.cpio (initramfs embutido/dispensado)"
else
  copy_if_changed "$INITRAMFS_IMG" "${TARGET_VOLUME}/sms.cpio" "sms.cpio"
fi
copy_if_changed "$SETTINGS_CFG" "${TARGET_VOLUME}/sms.cfg" "sms.cfg"

for fw in bootcode.bin start.elf fixup.dat bcm2710-rpi-3-b.dtb; do
  copy_if_changed "${FIRMWARE_DIR}/${fw}" "${TARGET_VOLUME}/${fw}" "$fw"
done

copy_games_incremental
cleanup_macos_junk

TARGET_PART_DEVICE=""
TARGET_DISK_DEVICE=""
resolve_target_devices

sync
sleep 1

if is_yes "$EJECT_AT_END"; then
  if [ -n "${TARGET_DISK_DEVICE:-}" ]; then
    sdcard_safe_eject_disk "$TARGET_DISK_DEVICE"
    echo "[OK] SD atualizado, sincronizado e ejetado: ${TARGET_DISK_DEVICE}"
  else
    echo "[WARN] Nao foi possivel determinar o disco para ejecao; tentando apenas desmontar o volume."
    if unmount_target_volume; then
      echo "[OK] SD atualizado, sincronizado e desmontado: ${TARGET_VOLUME}"
    else
      echo "[WARN] Nao foi possivel desmontar automaticamente o volume: ${TARGET_VOLUME}"
    fi
  fi
else
  if unmount_target_volume; then
    echo "[OK] SD atualizado, sincronizado e desmontado: ${TARGET_VOLUME}"
  else
    echo "[WARN] SD atualizado e sincronizado, mas nao foi possivel desmontar automaticamente: ${TARGET_VOLUME}"
  fi
fi

if [ -n "$config_tmp" ] && [ -f "$config_tmp" ]; then
  rm -f "$config_tmp"
fi
