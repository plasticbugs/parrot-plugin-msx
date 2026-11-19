#!/bin/sh
# test/carts/run.sh -- the test carts (test/carts/*.asm) on the Mac:
#
#   1. video, the four mappers and input run in the MSX and the mirror
#      (test/stats); the console's picture at set times must be headless
#      openMSX's (machine C-BIOS_MSX1), pixel for pixel, and the mapper
#      guess must be right
#   2. sound, the Konami SCC cart and input run in the console model
#      (test/host), the plugin as it is; the SN76489 bytes it sends are
#      checked, and input's pictures (keys, stick, mouse) are kept to look at
#
# Out: $TMPDIR/msx-carts. Needs sjasmplus, and openMSX for step 1
# (OPENMSX=its binary; the Mac app's by default; without it step 1 only
# checks the guesses). openMSX runs with no window: renderer none, SDL's
# dummy video driver.
set -e
here=$(cd "$(dirname "$0")/../.." && pwd)
c=$here/test/carts
out=${TMPDIR:-/tmp}/msx-carts
rm -rf "$out"; mkdir -p "$out"
omsx=${OPENMSX:-/Applications/openMSX.app/Contents/MacOS/openmsx}
[ -x "$omsx" ] || { echo "(no openMSX at $omsx: no comparisons)"; omsx=; }
status=0
fail() { echo "  FAIL: $*"; status=1; }

# the carts
cd "$c"
sjasmplus --nologo --msg=war --raw="$out/video.rom" video.asm >/dev/null
sjasmplus --nologo --msg=war --raw="$out/sound.rom" sound.asm >/dev/null
sjasmplus --nologo --msg=war --raw="$out/input.rom" input.asm >/dev/null
for m in 1 2 3 4; do sjasmplus --nologo --msg=war -DMAPPER=$m --raw="$out/map$m.rom" mapper.asm >/dev/null; done
cd "$here"
"$here/test/stats/build.sh" "$out/stats"
cc -std=c11 -O2 -Wall -Wextra -I"$here/test/host" -o "$out/render" "$c/render.c" "$here/test/host/tms9918.c" -lz

# compare NAME ROM "TIMES" [ROMTYPE [KEYS]] -- the console's picture against
# openMSX's at each time (seconds); KEYS as omsx-dump.tcl's OMSX_KEYS
compare() {
    name=$1; rom=$2; times=$3; romtype=$4; keys=$5
    o=$out/$name; mkdir -p "$o"
    if [ -n "$omsx" ]; then
        last=$(echo $times | tr ' ' '\n' | sort -g | tail -1)
        rtarg=; [ -n "$romtype" ] && rtarg="-romtype $romtype"
        OMSX_PREFIX=$o/om_ OMSX_TIMES="$times" OMSX_KEYS="$keys" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
            "$omsx" -machine C-BIOS_MSX1 -cart "$rom" $rtarg -script "$c/omsx-dump.tcl" </dev/null >"$o/openmsx.log" 2>&1 &
        pid=$!
        i=0; while [ ! -s "$o/om_$last.vram" ] && [ $i -lt 240 ]; do sleep 0.25; i=$((i + 1)); done
        sleep 0.2; kill $pid 2>/dev/null || true; wait $pid 2>/dev/null || true
    fi
    for t in $times; do
        f=$(awk "BEGIN { print int($t * 59.922) }")
        mkdir -p "$o/$t"
        if [ -n "$keys" ]; then STATS_KEYS=$keys "$out/stats" "$rom" "$f" - "$o/$t" >"$o/$t/stats.txt"
        else "$out/stats" "$rom" "$f" - "$o/$t" >"$o/$t/stats.txt"; fi || fail "$name at $t s: the mirror (stats.txt)"
        "$out/render" "$o/$t/console.vram" "$o/mine_$t.png" >/dev/null
        [ -n "$omsx" ] || continue
        if [ ! -s "$o/om_$t.vram" ]; then fail "$name: no openMSX dump at $t s"; continue; fi
        "$out/render" "$o/om_$t.vram" "$o/openmsx_$t.png" >/dev/null
        if cmp -s "$o/mine_$t.png" "$o/openmsx_$t.png"; then echo "  $name at $t s: the same picture as openMSX"
        else fail "$name at $t s: not openMSX's picture ($o/mine_$t.png, $o/openmsx_$t.png)"; fi
    done
}

echo "1. against openMSX"
compare video "$out/video.rom" "4 6 8 10 12"
for m in 1 2 3 4; do
    rt=$(echo "ASCII8 ASCII16 Konami KonamiSCC" | cut -d' ' -f$m)
    want=$(echo "ASCII8 ASCII16 KONAMI KONAMI_SCC" | cut -d' ' -f$m | tr _ ' ')
    compare map$m "$out/map$m.rom" "6" "$rt"
    grep -q "$want (GUESS)" "$out/map$m/6/stats.txt" && echo "  map$m: guessed $want" || fail "map$m: guessed $(sed -n 1p "$out/map$m/6/stats.txt")"
done
compare input "$out/input.rom" "4.5" "" "4 2 64 down 4 6 1 down 4 8 1 down"

echo "2. in the console model"
"$here/test/host/build.sh" "$here/src" "$out/run" 2>"$out/build.log" || { cat "$out/build.log"; exit 1; }
tree=$out/tree; mkdir -p "$tree/MSX"
cp "$out/sound.rom" "$tree/MSX/SOUND.mx1"; cp "$out/map4.rom" "$tree/MSX/SCC.mx1"; cp "$out/input.rom" "$tree/MSX/INPUT.mx1"
hdiutil create -quiet -srcfolder "$tree" -fs MS-DOS -layout NONE -size 64m -format UDTO -o "$out/card"
# host NAME FILE END_MS SCRIPT PATTERN ...
host() {
    name=$1; file=$2; end=$3; script=$4; shift 4
    o=$out/$name; mkdir -p "$o"
    PC_OUT=$o PC_CARD=$out/card.cdr PC_OPEN=/MSX/$file PC_END=$end PC_SCRIPT=$script "$out/run" >"$o/run.txt" 2>&1 || fail "$name: the model ($o/run.txt)"
    for want in "$@"; do grep -Eq -- "$want" "$o/log.txt" || fail "$name: nothing in $o/log.txt matches: $want"; done
    echo "  $name: $(grep -o '[0-9]* lists ([^)]*)' "$o/run.txt" | head -1)"
}
host sound SOUND.mx1 9000 "8500:snap=end" 'psg 8C 1A 90$' 'psg A6 0D B2$' 'psg CF 3F D8$' 'psg C0 01 DF E7 F8$' \
    'psg 9F BF FF$' 'psg 80 10 92$' 'psg 9F$'
host scc SCC.mx1 7000 "6500:snap=end" 'note "MSX KONAMI SCC GUESS"' 'psg 8C 1A 91$'
host input INPUT.mx1 7000 "4000:keydown=04 4000:mods=02 4000:joy=up+fire 4000:key=* 4500:snap=keys 5000:keyup=04 5000:mods=0 5000:joy= 5000:key= 5200:mouse=40,20,1 5500:snap=mouse 6000:mouse=-10,5,0 6500:snap=mouse2"
echo "  (input's pictures: $out/input/keys.png, mouse.png, mouse2.png)"

[ $status = 0 ] && echo "all cart checks passed"
exit $status
