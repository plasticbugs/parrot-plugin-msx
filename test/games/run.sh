#!/bin/sh
# test/games/run.sh ROMDIR [SECONDS] -- every .rom/.mx1/.mx2 in ROMDIR, each
# run in the Mac console model (test/host) for SECONDS (default 20), with
# SPACE and fire pressed now and then. Out: $TMPDIR/msx-games/NAME/*.png,
# and a line per game: the mapper, the frames, the list budget used.
# The ROMs are yours: nothing here keeps or copies them anywhere else.
set -e
here=$(cd "$(dirname "$0")/../.." && pwd)
roms=$1
secs=${2:-20}
out=${TMPDIR:-/tmp}/msx-games
mkdir -p "$out"
[ -f "$here/src/cbios_rom.h" ] || "$here/tools/cbios.sh" "$here/src/cbios_rom.h"
"$here/test/host/build.sh" "$here/src" "$out/run" 2>"$out/build.log" || { cat "$out/build.log"; exit 1; }
tree=$out/tree
rm -rf "$tree" "$out/card.cdr"
mkdir -p "$tree/MSX"
for f in "$roms"/*.rom "$roms"/*.mx1 "$roms"/*.mx2; do
    [ -f "$f" ] || continue
    b=$(basename "$f"); cp "$f" "$tree/MSX/${b%.*}.mx1"
done
hdiutil create -quiet -srcfolder "$tree" -fs MS-DOS -layout NONE -size 128m -format UDTO -o "$out/card"
end=$((secs * 1000))
script="4000:key=* 4300:key= 6000:joy=fire 6300:joy= 8000:key=* 8300:key= 10000:joy=fire 10300:joy="
script="$script $((end / 2)):snap=half $((end - 500)):snap=end"
for f in "$tree"/MSX/*.mx1; do
    b=$(basename "$f" .mx1)
    o=$out/$(echo "$b" | tr -c 'A-Za-z0-9\n' '_')
    rm -rf "$o"; mkdir -p "$o"
    cp "$out/card.cdr" "$o/card.cdr"
    PC_OUT=$o PC_CARD=$o/card.cdr PC_OPEN="/MSX/$b.mx1" PC_END=$end PC_SCRIPT="$script" "$out/run" >"$o/run.txt" 2>&1 || echo "  (exit $?)" >>"$o/run.txt"
    rm -f "$o/card.cdr"
    mapper=$(grep -o 'note "MSX [A-Z0-9 -]* \(GUESS\|LIST\|TAG\)' "$o/log.txt" | head -1 | sed 's/note "MSX //')
    printf '%-40s %-22s %s\n' "$b" "$mapper" "$(grep -o '[0-9]* lists ([^)]*)' "$o/run.txt" | head -1)"
done
