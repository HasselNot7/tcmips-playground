#!/bin/sh
# Build a native host binary of the NetHack port for debugging.
set -e
cd "$(dirname "$0")/.."
SRC=src/games/nethack
OUT=/tmp/opencode/nhhost
mkdir -p $OUT
SRCS=""
for f in $SRC/core/*.c $SRC/tty/*.c $SRC/sys/*.c $SRC/lua/*.c \
         $SRC/tcm_main.c $SRC/tcm_vterm.c $SRC/tcm_termcap.c \
         $SRC/tcm_sys.c $SRC/tcm_kbd.c $SRC/tcm_dirent.c; do
    case "$f" in
        */lua/lua.c|*/lua/luac.c|*/lua/onelua.c) continue ;;
    esac
    SRCS="$SRCS $f"
done
gcc -O1 -g -std=gnu99 -DTCM_HOST -DTCMIPS_PORT -DNO_SIGNAL -DNOMAIL \
    -DNO_TERMCAP_HEADERS -DGCC_UMINUS_NOSTATICUNUSED \
    -I$SRC/include -I$SRC/lua -I$SRC -Iasset/nethack \
    -Wno-implicit-function-declaration -Wno-format-security \
    -o $OUT/nethack $SRCS asset/nethack/nhdata.cpp -lm
ls -la $OUT/nethack && echo BUILD_OK