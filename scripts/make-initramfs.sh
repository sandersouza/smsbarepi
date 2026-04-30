#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 2 ]; then
  echo "Uso: $0 <diretorio_de_entrada> <arquivo_saida.cpio>"
  echo "Exemplo: $0 roms/initramfs sdcard/sms.cpio"
  exit 1
fi

IN_DIR="$1"
OUT_FILE="$2"

if [ ! -d "$IN_DIR" ]; then
  echo "[ERRO] Diretorio nao encontrado: $IN_DIR"
  exit 1
fi

# Resolve caminho de saida para absoluto antes de mudar diretorio.
case "$OUT_FILE" in
  /*) OUT_ABS="$OUT_FILE" ;;
  *) OUT_ABS="$(pwd)/$OUT_FILE" ;;
esac

mkdir -p "$(dirname "$OUT_ABS")"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

shopt -s nullglob

rom_candidates=("$IN_DIR"/bios.sms "$IN_DIR"/BIOS.SMS "$IN_DIR"/*.SMS "$IN_DIR"/*.sms)
if [ "${#rom_candidates[@]}" -eq 0 ]; then
  echo "[ERRO] Nenhuma ROM encontrada em: $IN_DIR (esperado bios.sms)"
  exit 1
fi

cp -R "${rom_candidates[@]}" "$TMP_DIR"/

if [ ! -f "$TMP_DIR/bios.sms" ]; then
  if [ -f "$TMP_DIR/BIOS.SMS" ]; then
    cp -f "$TMP_DIR/BIOS.SMS" "$TMP_DIR/bios.sms"
  fi
fi

if [ ! -f "$TMP_DIR/bios.sms" ]; then
  echo "[ERRO] bios.sms nao encontrado em: $IN_DIR"
  exit 1
fi

(
  cd "$TMP_DIR"
  find . -type f | LC_ALL=C sort | cpio -o -H newc --quiet > "$OUT_ABS"
)

echo "[OK] Initramfs criado: $OUT_ABS"
