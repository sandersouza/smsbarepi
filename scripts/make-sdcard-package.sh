#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "${PROJECT_ROOT}/scripts/sdcard-common.sh"

PACKAGE_KERNEL_IMG="${PACKAGE_KERNEL_IMG:-}"
PACKAGE_CONFIG_TXT="${PACKAGE_CONFIG_TXT:-${PROJECT_ROOT}/sdcard/config.txt}"
PACKAGE_CMDLINE_TXT="${PACKAGE_CMDLINE_TXT:-${PROJECT_ROOT}/sdcard/cmdline.txt}"
PACKAGE_INITRAMFS_IMG="${PACKAGE_INITRAMFS_IMG:-${PROJECT_ROOT}/sdcard/sms.cpio}"
PACKAGE_EMBED_INITRAMFS="${PACKAGE_EMBED_INITRAMFS:-0}"
PACKAGE_SETTINGS_CFG="${PACKAGE_SETTINGS_CFG:-${PROJECT_ROOT}/sdcard/sms.cfg}"
PACKAGE_FIRMWARE_DIR="${PACKAGE_FIRMWARE_DIR:-${PROJECT_ROOT}/rpi-firmware/}"
PACKAGE_COPY_GAMES_TO_ROOT="${PACKAGE_COPY_GAMES_TO_ROOT:-0}"
PACKAGE_GAMES_SRC_DIR="${PACKAGE_GAMES_SRC_DIR:-${PROJECT_ROOT}/roms/games}"

PACKAGE_STAGE_DIR="${PACKAGE_STAGE_DIR:-${PROJECT_ROOT}/artifacts/.temp-sdcard-package}"
PACKAGE_TIMESTAMP="${PACKAGE_TIMESTAMP:-$(date +%Y%m%d%H%M)}"
PACKAGE_ZIP="${PACKAGE_ZIP:-${PROJECT_ROOT}/artifacts/sdcard-package-${PACKAGE_TIMESTAMP}.zip}"

required_fw_files="bootcode.bin start.elf fixup.dat bcm2710-rpi-3-b.dtb"

# Normaliza caminhos para absolutos quando necessário (evita falha após 'cd' no stage).
case "$PACKAGE_STAGE_DIR" in
  /*) ;;
  *) PACKAGE_STAGE_DIR="${PROJECT_ROOT}/${PACKAGE_STAGE_DIR}" ;;
esac
case "$PACKAGE_ZIP" in
  /*) ;;
  *) PACKAGE_ZIP="${PROJECT_ROOT}/${PACKAGE_ZIP}" ;;
esac

# Evita gravacao de AppleDouble/extended attributes no stage ZIP.
export COPYFILE_DISABLE=1

ensure_settings_cfg() {
  if [ -f "$PACKAGE_SETTINGS_CFG" ]; then
    return 0
  fi

  echo "[INFO] Arquivo de configuracao nao encontrado; gerando default: ${PACKAGE_SETTINGS_CFG}"
  mkdir -p "$(dirname "$PACKAGE_SETTINGS_CFG")"
  cat > "$PACKAGE_SETTINGS_CFG" <<'EOF'
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

resolve_default_package_kernel_img() {
  local branch=""
  local issue_id=""
  local candidate=""

  if [ -n "$PACKAGE_KERNEL_IMG" ]; then
    printf '%s' "$PACKAGE_KERNEL_IMG"
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

require_file() {
  local f="$1"
  if [ ! -f "$f" ]; then
    echo "[ERRO] Arquivo obrigatorio ausente: $f"
    exit 1
  fi
}

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "[ERRO] Comando obrigatorio nao encontrado: $1"
    exit 1
  fi
}

require_cmd zip
PACKAGE_KERNEL_IMG="$(resolve_default_package_kernel_img)"
echo "[INFO] PACKAGE_KERNEL_IMG efetivo: ${PACKAGE_KERNEL_IMG}"
require_file "$PACKAGE_KERNEL_IMG"
require_file "$PACKAGE_CONFIG_TXT"
require_file "$PACKAGE_CMDLINE_TXT"
if [ "$PACKAGE_EMBED_INITRAMFS" != "1" ]; then
  require_file "$PACKAGE_INITRAMFS_IMG"
fi
ensure_settings_cfg
require_file "$PACKAGE_SETTINGS_CFG"

for fw in $required_fw_files; do
  require_file "${PACKAGE_FIRMWARE_DIR}/${fw}"
done

rm -rf "$PACKAGE_STAGE_DIR"
mkdir -p "$PACKAGE_STAGE_DIR"
mkdir -p "${PACKAGE_STAGE_DIR}/sms/roms"
mkdir -p "$(dirname "$PACKAGE_ZIP")"

cp "$PACKAGE_KERNEL_IMG" "${PACKAGE_STAGE_DIR}/sms.img"
cp "$PACKAGE_CONFIG_TXT" "${PACKAGE_STAGE_DIR}/config.txt"
cp "$PACKAGE_CMDLINE_TXT" "${PACKAGE_STAGE_DIR}/cmdline.txt"
if [ "$PACKAGE_EMBED_INITRAMFS" != "1" ]; then
  cp "$PACKAGE_INITRAMFS_IMG" "${PACKAGE_STAGE_DIR}/sms.cpio"
fi
cp "$PACKAGE_SETTINGS_CFG" "${PACKAGE_STAGE_DIR}/sms.cfg"

for fw in $required_fw_files; do
  cp "${PACKAGE_FIRMWARE_DIR}/${fw}" "${PACKAGE_STAGE_DIR}/${fw}"
done

if [ "$PACKAGE_COPY_GAMES_TO_ROOT" = "1" ]; then
  sdcard_copy_sms_rom_payload "$PACKAGE_GAMES_SRC_DIR" "${PACKAGE_STAGE_DIR}/sms"
fi

rm -f "$PACKAGE_ZIP"
(
  cd "$PACKAGE_STAGE_DIR"
  zip -qry "$PACKAGE_ZIP" .
  rm -rf "$PACKAGE_STAGE_DIR"
)

echo "[OK] Pacote SD criado: $PACKAGE_ZIP"
