#!/usr/bin/env bash
set -euo pipefail

CROSS="${CROSS:-auto}"
DRY_RUN="${DRY_RUN:-0}"

if [ "$DRY_RUN" != "0" ] && [ "$DRY_RUN" != "1" ]; then
  echo "[ERRO] DRY_RUN invalido: $DRY_RUN (use 0 ou 1)"
  exit 2
fi

OS_NAME="$(uname -s)"

COMMON_TOOLS=(
  make
  git
  zip
  cpio
  rsync
  fzf
  perl
)

OS_TOOLS=()
case "$OS_NAME" in
  Darwin)
    OS_TOOLS=(diskutil brew)
    ;;
  Linux)
    OS_TOOLS=(apt-get)
    ;;
  *)
    echo "[ERRO] SO nao suportado: $OS_NAME"
    echo "[INFO] Suportado: macOS e Linux"
    exit 2
    ;;
esac

MISSING=0

check_cmd() {
  local cmd="$1"
  local label="$2"

  if [ "$DRY_RUN" = "1" ]; then
    if command -v "$cmd" >/dev/null 2>&1; then
      echo "[DRY-RUN][OK] $label: $cmd"
    else
      echo "[DRY-RUN][MISS] $label: $cmd"
      MISSING=1
    fi
    return 0
  fi

  if command -v "$cmd" >/dev/null 2>&1; then
    echo "[OK] $label: $cmd"
  else
    echo "[ERRO] $label ausente: $cmd"
    MISSING=1
  fi
}

check_toolchain_prefix() {
  local prefix="$1"
  local miss=0
  local tool cmd

  echo "[INFO] Testando prefixo de toolchain: ${prefix}"
  for tool in gcc g++ ld objcopy; do
    cmd="${prefix}${tool}"
    if [ "$DRY_RUN" = "1" ]; then
      if command -v "$cmd" >/dev/null 2>&1; then
        echo "[DRY-RUN][OK] Toolchain: $cmd"
      else
        echo "[DRY-RUN][MISS] Toolchain: $cmd"
        miss=1
      fi
    else
      if command -v "$cmd" >/dev/null 2>&1; then
        echo "[OK] Toolchain: $cmd"
      else
        echo "[ERRO] Toolchain ausente: $cmd"
        miss=1
      fi
    fi
  done

  [ "$miss" -eq 0 ]
}

echo "[INFO] SO detectado: $OS_NAME"

for tool in "${COMMON_TOOLS[@]}"; do
  check_cmd "$tool" "Dependencia"
done

for tool in "${OS_TOOLS[@]}"; do
  check_cmd "$tool" "Dependencia do SO"
done

if [ "$CROSS" = "auto" ]; then
  if check_toolchain_prefix "aarch64-none-elf-"; then
    :
  elif check_toolchain_prefix "aarch64-elf-"; then
    :
  elif check_toolchain_prefix "aarch64-linux-gnu-"; then
    :
  else
    MISSING=1
  fi
else
  if ! check_toolchain_prefix "$CROSS"; then
    MISSING=1
  fi
fi

if [ "$MISSING" -ne 0 ]; then
  if [ "$DRY_RUN" = "1" ]; then
    echo "[DRY-RUN] Dependencias/toolchain faltando (simulacao nao falha)."
    exit 0
  fi
  exit 1
fi
