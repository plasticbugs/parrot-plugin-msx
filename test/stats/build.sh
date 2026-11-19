#!/bin/sh
# test/stats/build.sh OUT -- stats.c for the Mac (stats.c says what it does)
set -e
here=$(cd "$(dirname "$0")/../.." && pwd)
[ -f "$here/src/cbios_rom.h" ] || "$here/tools/cbios.sh" "$here/src/cbios_rom.h"
s=$here/src
cc -std=c11 -O2 -Wall -Wextra -I"$s" -o "$1" "$here/test/stats/stats.c" "$s/msx.c" "$s/tms.c" "$s/mirror.c" \
    "$s/sound.c" "$s/cpu.c" "$s/z80core.c" "$s/romtype.c" "$s/sha1.c"
