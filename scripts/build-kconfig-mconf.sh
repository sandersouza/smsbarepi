#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SRC_DIR="$ROOT_DIR/tools/kconfig-linux"
GEN_DIR="$SRC_DIR/generated"
OUT_BIN="$ROOT_DIR/build/tools/kconfig-linux/mconf"

mkdir -p "$GEN_DIR" "$(dirname "$OUT_BIN")"

HOSTCC_BIN="${HOSTCC:-cc}"
HOSTPKG_CONFIG_BIN="${HOSTPKG_CONFIG:-pkg-config}"

BISON_OUT_C="$GEN_DIR/parser.tab.c"
BISON_OUT_H="$GEN_DIR/parser.tab.h"
FLEX_OUT_C="$GEN_DIR/lexer.lex.c"

bison -t -d -o "$BISON_OUT_C" "$SRC_DIR/parser.y"
flex -o "$FLEX_OUT_C" "$SRC_DIR/lexer.l"

MCONF_CFLAGS_FILE="$GEN_DIR/mconf-cflags"
MCONF_LIBS_FILE="$GEN_DIR/mconf-libs"
HOSTCC="$HOSTCC_BIN" HOSTPKG_CONFIG="$HOSTPKG_CONFIG_BIN" "$SRC_DIR/mconf-cfg.sh" "$MCONF_CFLAGS_FILE" "$MCONF_LIBS_FILE"

MCONF_CFLAGS="$(cat "$MCONF_CFLAGS_FILE")"
MCONF_LIBS="$(cat "$MCONF_LIBS_FILE")"

COMMON_SRCS=(
  "$SRC_DIR/confdata.c"
  "$SRC_DIR/expr.c"
  "$FLEX_OUT_C"
  "$SRC_DIR/menu.c"
  "$BISON_OUT_C"
  "$SRC_DIR/preprocess.c"
  "$SRC_DIR/symbol.c"
  "$SRC_DIR/util.c"
  "$SRC_DIR/mnconf-common.c"
)

LXDIALOG_SRCS=(
  "$SRC_DIR/lxdialog/checklist.c"
  "$SRC_DIR/lxdialog/inputbox.c"
  "$SRC_DIR/lxdialog/menubox.c"
  "$SRC_DIR/lxdialog/textbox.c"
  "$SRC_DIR/lxdialog/util.c"
  "$SRC_DIR/lxdialog/yesno.c"
)

CFLAGS="-std=gnu99 -D_GNU_SOURCE -DKBUILD_NO_NLS -I$SRC_DIR -I$SRC_DIR/include -I$GEN_DIR"

$HOSTCC_BIN $CFLAGS $MCONF_CFLAGS \
  "$SRC_DIR/mconf.c" \
  "${COMMON_SRCS[@]}" \
  "${LXDIALOG_SRCS[@]}" \
  $MCONF_LIBS -o "$OUT_BIN"

echo "Built: $OUT_BIN"
