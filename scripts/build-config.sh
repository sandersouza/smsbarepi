#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

if [ "$PWD" != "/Users/sandersouza/Developer/smsbarepi" ]; then
  echo "[WARN] PWD atual: $PWD" >&2
  echo "[WARN] Recomenda-se usar /Users/sandersouza/Developer/smsbarepi" >&2
fi

NON_INTERACTIVE="${NON_INTERACTIVE:-0}"

choose() {
  local prompt="$1"
  local allow_back="$2"
  shift 2
  local options=("$@")

  if [ "$NON_INTERACTIVE" = "1" ]; then
    echo "${options[0]}"
    return 0
  fi

  if command -v fzf >/dev/null 2>&1; then
    local selected=""
    set +e
    local header="Setas + Enter"
    if [ "$allow_back" = "1" ]; then
      header="Setas + Enter | ESC = voltar"
    fi
    selected="$(printf '%s\n' "${options[@]}" | fzf --height=12 --reverse --border --prompt "${prompt} > " --header "$header")"
    local status=$?
    set -e
    if [ "$status" -eq 0 ] && [ -n "$selected" ]; then
      echo "$selected"
      return 0
    fi
    if [ "$status" -eq 130 ] && [ "$allow_back" = "1" ]; then
      return 2
    fi
    return 1
  fi

  local i=1
  echo >&2
  echo "$prompt" >&2
  for opt in "${options[@]}"; do
    echo "  [$i] $opt" >&2
    i=$((i+1))
  done
  if [ "$allow_back" = "1" ]; then
    echo "  [0] Voltar" >&2
  fi

  local sel
  while true; do
    printf "> " >&2
    read -r sel
    if [ "$allow_back" = "1" ] && { [ "$sel" = "0" ] || [ "$sel" = "b" ] || [ "$sel" = "B" ]; }; then
      return 2
    fi
    if [[ "$sel" =~ ^[0-9]+$ ]] && [ "$sel" -ge 1 ] && [ "$sel" -le "${#options[@]}" ]; then
      echo "${options[$((sel-1))]}"
      return 0
    fi
    echo "Selecao invalida." >&2
  done
}

debug_choice="ON"

if [ "$NON_INTERACTIVE" != "1" ]; then
  step=1
  while true; do
    case "$step" in
      1)
        set +e
        choice="$(choose "Debug visual (overlay)" 0 "ON" "OFF")"
        status=$?
        set -e
        if [ "$status" -ne 0 ]; then
          echo "[ABORTADO] Configuracao cancelada."
          exit 1
        fi
        debug_choice="$choice"
        step=2
        ;;
      2) break ;;
    esac
  done
else
  debug_choice="ON"
fi

debug_value="1"
if [ "$debug_choice" = "OFF" ]; then
  debug_value="0"
fi

cmd=(
  make default
  DEBUG="$debug_value"
)

echo
echo "Configuracao selecionada:"
echo "  Debug compilado: $debug_choice"
echo "  Boot mode: controlado em runtime pelo menu (NORM/FAST/GAME)"
echo

echo "Comando:"
printf '  %q' "${cmd[@]}"
echo

echo
echo "Executando build..."
"${cmd[@]}"

echo
echo "Build finalizado: artifacts/kernel8-default.img"
