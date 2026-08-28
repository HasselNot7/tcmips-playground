#!/bin/sh
# Sentinel build for the NetHack port: verifies key edits are still on disk
# (CLion auto-save has been overwriting files with stale editor buffers),
# builds the device target, and confirms the marker strings landed in the
# .tcm image. Aborts loudly on any mismatch.
set -e
cd "$(dirname "$0")/.."
SRC=src/games/nethack

check() { # file, pattern, label
    if ! grep -q "$2" "$1"; then
        echo "SENTINEL FAIL: $3 ($1 missing '$2')" >&2
        exit 1
    fi
}

check $SRC/tcm_main.c      "tcm_embed_init" "blob copy-out"
check $SRC/core/weapon.c   "stale/dangling pointer guard" "weapon_descr guard"
check $SRC/core/botl.c     "stale/dangling uwep pointer" "uwep guard"
check $SRC/tcm_fs.c        "tcm_embed_init" "sys blob copy"
check $SRC/tcm_fs.c        "efiles"         "shadow file table"
check $SRC/tcm_vterm.c     "render_cell"    "VRAM renderer"
check $SRC/tcm_fs.c        "gender:female"  "options string"
check $SRC/lua/lvm.c        "__mips__"       "lua jump table disabled"
check $SRC/core/save.c       "in_self_recover" "savestateinlock guard"
check $SRC/core/sfstruct.c   "TCMIPS_PORT"     "stdio buffering disabled"
check $SRC/tcm_hooks.c       "O_CREAT"         "getlock creates 1lock.0"

export CMAKE=${CMAKE:-/home/hasselnot/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake}
export PATH=/home/hasselnot/.local/share/JetBrains/Toolbox/apps/clion/bin/ninja/linux/x64:$PATH
$CMAKE --build cmake-build-release --target tcmips_nethack -j8 > /tmp/opencode/nh_build.log 2>&1

if ! grep -q "tcm-nethack" cmake-build-release/tcmips_nethack.tcm 2>/dev/null; then
    python3 - << 'EOF'
import sys
img = open('cmake-build-release/tcmips_nethack.tcm','rb').read()
if b'TCMBUILD' not in img:
    print("SENTINEL FAIL: build id missing from image", file=sys.stderr)
    sys.exit(1)
print("image marker OK")
EOF
fi
ls -la cmake-build-release/tcmips_nethack.tcm
echo BUILD_OK