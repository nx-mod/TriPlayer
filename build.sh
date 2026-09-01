#!/bin/bash
# Builds TriPlayer (Application + Overlay + Sysmodule, plus sqlite-nx) inside
# the devkitPro MSYS2 shell.
#   bash build.sh [target] [jobs]
#     target  all (default) | clean

source /etc/profile.d/devkit-env.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR" || exit 1

# Point at the flat-cloned libnx fork, not devkitPro's stock system libnx --
# matches every other project's build.sh in this repo.
export LIBNX="$SCRIPT_DIR/../libnx/nx"

TARGET="${1:-all}"
JOBS="${2:-2}"

echo "DEVKITPRO=$DEVKITPRO"
echo "Target=$TARGET Jobs=$JOBS CWD=$(pwd)"

make $TARGET
STATUS=$?

if [ $STATUS -eq 0 ] && [ "$TARGET" = "all" ]; then
    echo "== Build ok. Output in sdcard/ =="
    find sdcard -type f 2>/dev/null
fi

exit $STATUS
