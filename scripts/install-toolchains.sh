#!/usr/bin/env bash
set -euo pipefail

SCRIPT_NAME="$(basename "$0")"
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DRY_RUN="${DRY_RUN:-0}"

if [ "$DRY_RUN" != "0" ] && [ "$DRY_RUN" != "1" ]; then
  echo "[ERRO] DRY_RUN invalido: $DRY_RUN (use 0 ou 1)"
  exit 2
fi

COMMON_TOOLS=(
  make
  git
  zip
  cpio
  rsync
  fzf
  shasum
)

TOOLCHAIN_PREFIXES=(
  aarch64-none-elf-
  aarch64-elf-
  aarch64-linux-gnu-
)

toolchain_prefix_found() {
  local prefix tool
  for prefix in "${TOOLCHAIN_PREFIXES[@]}"; do
    local missing=0
    for tool in gcc g++ ld objcopy; do
      if ! command -v "${prefix}${tool}" >/dev/null 2>&1; then
        missing=1
        break
      fi
    done
    if [ "$missing" -eq 0 ]; then
      printf '%s\n' "$prefix"
      return 0
    fi
  done
  return 1
}

run_toolchain_check() {
  if [ -x "$ROOT_DIR/scripts/check-toolchain.sh" ]; then
    CROSS="auto" "$ROOT_DIR/scripts/check-toolchain.sh"
    return $?
  fi
  echo "[WARN] Script de validacao nao encontrado: scripts/check-toolchain.sh"
  return 1
}

missing_tools() {
  local tool
  for tool in "$@"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
      printf '%s\n' "$tool"
    fi
  done
}

collect_missing_tools() {
  local out_var="$1"
  shift

  local list=()
  local item
  while IFS= read -r item; do
    [ -n "$item" ] && list+=("$item")
  done < <(missing_tools "$@")

  if [ "${#list[@]}" -eq 0 ]; then
    eval "$out_var=()"
  else
    eval "$out_var=(\"\${list[@]}\")"
  fi
}

print_tools_status() {
  local label="$1"
  shift
  local items=("$@")
  local missing=()
  collect_missing_tools missing "${items[@]}"

  echo
  echo "[$label]"
  if [ "${#missing[@]}" -eq 0 ]; then
    echo "  OK: tudo presente"
  else
    echo "  Faltando:"
    local item
    for item in "${missing[@]}"; do
      [ -n "$item" ] && echo "  - $item"
    done
  fi
}

print_toolchain_status() {
  local active_prefix=""
  echo
  echo "[Toolchain AArch64]"
  if active_prefix="$(toolchain_prefix_found)"; then
    echo "  OK: toolchain detectada no PATH (${active_prefix}*)"
  else
    echo "  Faltando:"
    echo "  - gcc/g++/ld/objcopy para um dos prefixos:"
    local p
    for p in "${TOOLCHAIN_PREFIXES[@]}"; do
      echo "    * ${p}"
    done
  fi
}

linux_install_with_apt() {
  if ! command -v apt-get >/dev/null 2>&1; then
    echo "[ERRO] Linux sem APT detectado. Este script instala apenas com APT."
    echo "[INFO] Use instalacao manual no seu gerenciador de pacotes."
    exit 2
  fi

  local common_packages=(
    make
    git
    zip
    cpio
    rsync
    fzf
    perl
  )

  local toolchain_pkg_sets=(
    "binutils-aarch64-none-elf gcc-aarch64-none-elf"
    "binutils-aarch64-elf gcc-aarch64-elf"
    "binutils-aarch64-linux-gnu gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
  )

  local runner=()
  if [ "${EUID:-$(id -u)}" -eq 0 ]; then
    runner=()
  elif command -v sudo >/dev/null 2>&1; then
    runner=(sudo)
  else
    echo "[ERRO] Precisa de root ou sudo para instalar pacotes via APT."
    echo "[INFO] Rode como root ou instale sudo."
    exit 2
  fi

  if [ "$DRY_RUN" = "1" ]; then
    echo "[DRY-RUN] ${runner[*]} apt-get update"
    echo "[DRY-RUN] ${runner[*]} apt-get install -y ${common_packages[*]}"
    echo "[DRY-RUN] Tentativa de instalacao da toolchain por conjuntos de fallback APT"
    return 0
  fi

  echo "[INFO] Atualizando indice APT..."
  "${runner[@]}" apt-get update

  echo "[INFO] Instalando ferramentas necessarias..."
  "${runner[@]}" apt-get install -y "${common_packages[@]}"

  if toolchain_prefix_found >/dev/null 2>&1; then
    echo "[INFO] Toolchain ja presente no PATH, sem instalacao adicional via APT."
    return 0
  fi

  local set pair pkg all_available
  for set in "${toolchain_pkg_sets[@]}"; do
    all_available=1
    for pkg in $set; do
      if ! apt-cache show "$pkg" 2>/dev/null | grep -q '^Package:'; then
        all_available=0
        break
      fi
    done
    if [ "$all_available" -eq 1 ]; then
      echo "[INFO] Instalando toolchain via APT: $set"
      "${runner[@]}" apt-get install -y $set
      return 0
    fi
  done

  echo "[WARN] Nenhum conjunto de pacotes AArch64 suportado encontrado no APT desta distro."
  echo "[INFO] Instale manualmente uma toolchain com prefixo aarch64-none-elf-, aarch64-elf- ou aarch64-linux-gnu-."
}

macos_install_with_brew() {
  if ! command -v brew >/dev/null 2>&1; then
    echo "[ERRO] Homebrew nao encontrado no macOS."
    echo "[INFO] Instale o Homebrew e execute novamente."
    echo "[INFO] https://brew.sh"
    exit 2
  fi

  local common_formulas=(
    make
    git
    zip
    rsync
    cpio
    fzf
    perl
  )

  if [ "$DRY_RUN" = "1" ]; then
    echo "[DRY-RUN] brew update"
    echo "[DRY-RUN] brew install ${common_formulas[*]}"
  else
    echo "[INFO] Atualizando Homebrew..."
    brew update
    echo "[INFO] Instalando ferramentas gerais via Homebrew..."
    brew install "${common_formulas[@]}" || true
  fi

  if command -v aarch64-none-elf-gcc >/dev/null 2>&1 || command -v aarch64-elf-gcc >/dev/null 2>&1; then
    echo "[INFO] Toolchain AArch64 ja presente no PATH."
    return 0
  fi

  local toolchain_formula=""
  local candidates=(
    aarch64-none-elf-gcc
    aarch64-elf-gcc
    messense/macos-cross-toolchains/aarch64-unknown-elf-gcc
  )
  local c
  for c in "${candidates[@]}"; do
    if brew info --formula "$c" >/dev/null 2>&1; then
      toolchain_formula="$c"
      break
    fi
  done

  if [ -z "$toolchain_formula" ]; then
    echo "[WARN] Nenhuma formula de toolchain AArch64 bare-metal encontrada automaticamente."
    echo "[INFO] Instale manualmente uma toolchain que forneca aarch64-none-elf-* ou aarch64-elf-* e ajuste o PATH/CROSS."
    return 0
  fi

  if [ "$DRY_RUN" = "1" ]; then
    echo "[DRY-RUN] brew install $toolchain_formula"
  else
    echo "[INFO] Instalando toolchain via Homebrew: $toolchain_formula"
    brew install "$toolchain_formula"
  fi
}

suggest_for_unsupported_os() {
  local os="$1"
  echo "[ERRO] SO nao suportado por este instalador: $os"
  echo "[INFO] Suportado: Linux com APT e macOS com Homebrew."
  echo "[INFO] Em outros SOs, instale manualmente:"
  echo "  - make git zip cpio rsync fzf perl"
  echo "  - toolchain aarch64-none-elf-*"
}

main() {
  local os
  os="$(uname -s)"

  echo "[$SCRIPT_NAME] Verificando ambiente..."
  print_tools_status "Ferramentas gerais" "${COMMON_TOOLS[@]}"
  print_toolchain_status

  echo
  echo "[$SCRIPT_NAME] Checando toolchain via script oficial..."
  if run_toolchain_check; then
    local missing_before=()
    collect_missing_tools missing_before "${COMMON_TOOLS[@]}"
    if [ "${#missing_before[@]}" -eq 0 ]; then
      echo "[OK] Ambiente ja estava completo."
      exit 0
    fi
    echo "[INFO] Toolchain OK, mas faltam ferramentas gerais."
  else
    echo "[INFO] Toolchain ausente/incompleta. Tentando instalar..."
  fi

  case "$os" in
    Linux)
      linux_install_with_apt
      ;;
    Darwin)
      macos_install_with_brew
      ;;
    *)
      suggest_for_unsupported_os "$os"
      exit 2
      ;;
  esac

  echo
  echo "[$SCRIPT_NAME] Revalidando ambiente..."
  print_tools_status "Ferramentas gerais" "${COMMON_TOOLS[@]}"
  print_toolchain_status

  echo
  echo "[$SCRIPT_NAME] Rechecando toolchain via script oficial..."
  if ! run_toolchain_check; then
    echo "[WARN] Script oficial de check da toolchain ainda falha."
  fi

  local missing_after=()
  collect_missing_tools missing_after "${COMMON_TOOLS[@]}"
  local toolchain_ok=1
  if ! toolchain_prefix_found >/dev/null 2>&1; then
    toolchain_ok=0
  fi

  if [ "${#missing_after[@]}" -gt 0 ] || [ "$toolchain_ok" -ne 1 ]; then
    echo
    echo "[WARN] Ainda existem ferramentas faltando."
    exit 1
  fi

  echo
  echo "[OK] Ambiente pronto para build."
}

main "$@"
