#!/bin/sh
# sim/test.sh [NAME...] -- the simulator, checked headless: the kit's
# examples and the template's plugin, each built with sim/run.sh and run
# with --headless and a script of inputs, as test/run.sh runs them on the
# models alone. No window opens and nothing plays.
#
# For each: the log must say what test/run.sh's does; the picture must be a
# 256x192 screenshot; and where test/run.sh has run first (its pictures in
# $TMPDIR/parrotcart-test), the simulator's picture and log must be the
# same as its. Beyond that, what only the simulator does: the Sound
# example's scale, in tune in the WAV; the SD card as a folder, the save
# landing in it as a file; a file opened with --open.
#
#   sim/test.sh                 everything
#   sim/test.sh msx             just that
#
# The names: msx (opened without a file) and msx-usb (test/carts/input.asm
# from a card folder, with a keyboard, a pad and a mouse). Pictures and
# logs: build-sim/test/.
set -e
# (on Windows, in Git Bash or MSYS2: C:/... paths throughout, and the shell
# told not to turn the card's "/NOTES/..." into a path on this computer)
kit=$(cd "$(dirname "$0")/.." && { pwd -W 2>/dev/null || pwd; })
export MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*'
out=$kit/build-sim/test
host=${TMPDIR:-/tmp}/parrotcart-test
mkdir -p "$out"
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy  # (the simulator insists, headless, anyway)
py=
for p in python3 python; do "$p" -c 'import sys' >/dev/null 2>&1 && { py=$p; break; }; done
[ -n "$py" ] || { echo "sim/test.sh: needs Python 3"; exit 2; }
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) windows=1 ;; *) windows=0 ;; esac
status=0
ONLY="$*"
wanted() { [ -z "$ONLY" ] && return 0; case " $ONLY " in *" $1 "*) return 0 ;; esac; return 1; }
fail() { echo "  FAIL: $*"; status=1; }
compared=0

# run NAME DIR [simulator options] -- the plugin in DIR, headless; o= its output
run() {
    name=$1; dir=$2; shift 2
    o=$out/$name
    rm -rf "$o"; mkdir -p "$o"
    echo "$name"
    skip=
    [ -d "$dir" ] || { echo "  (not here: $dir)"; skip=1; return 0; }
    if "$kit/sim/run.sh" "$dir" --headless --out "$o" "$@" >"$o/run.txt" 2>&1; then :; else
        fail "the simulator stopped with an error:"; sed 's/^/    /' "$o/run.txt" | tail -15
    fi
    # (warnings fail the check, as in test/run.sh; not MSVC's, about code written for GCC)
    if grep -q 'compiler warnings' "$o/run.txt"; then
        if [ $windows = 1 ]; then grep 'compiler warnings' "$o/run.txt"; else fail "$(grep 'compiler warnings' "$o/run.txt")"; fi
    fi
    grep '^  [0-9]* ms' "$o/run.txt" || true
    [ -f "$o/screen.png" ] && { "$py" -I "$kit/tools/png256.py" check "$o/screen.png" >/dev/null || fail "screen.png isn't a 256x192 screenshot"; }
    same_as "$name"
}

# midi_log FILE LINE... -- what --midi-log wrote: these lines, no others
midi_log() {
    [ -z "$skip" ] || return 0
    f=$1; shift
    printf '%s\n' "$@" > "$f.want"
    cmp -s "$f" "$f.want" || { fail "$f isn't what the plugin should have sent:"; diff "$f.want" "$f" | head -8 | sed 's/^/    /'; }
}

# same_as NAME [picture] -- the last run's picture and log against test/run.sh's
# NAME, if it has run (the picture only: a card folder isn't macOS's image, so
# its sector reads, and the log's times, differ)
same_as() {
    [ -z "$skip" ] && [ -f "$host/$1/screen.png" ] || return 0
    compared=$((compared + 1))
    if "$py" -I "$kit/tools/png256.py" same "$o/screen.png" "$host/$1/screen.png" >"$o/same.txt" 2>&1; then
        echo "  test/host: the same picture"
    else fail "the picture isn't test/host's $1: $(cat "$o/same.txt")"; fi
    if [ "$2" != picture ] && [ -f "$host/$1/log.txt" ] && ! cmp -s "$o/log.txt" "$host/$1/log.txt"; then
        fail "the log isn't test/host's $1:"; diff "$host/$1/log.txt" "$o/log.txt" | head -8 | sed 's/^/    /'
    fi
}

# expect PATTERN... -- in the last run's log (grep -E)
expect() {
    [ -z "$skip" ] || return 0
    for want in "$@"; do
        grep -Eq -- "$want" "$o/log.txt" || fail "nothing in $o/log.txt matches: $want"
    done
}

# save_check FILE MAGIC RUNS -- a save file a plugin wrote in a card folder:
# 512 bytes, starting with MAGIC, a run count at byte 8 (as test/run.sh's
# card_check reads it on a card image)
save_check() {
    got=$("$py" -I -c 'import struct,sys; d=open(sys.argv[1],"rb").read(); print(len(d), d[:len(sys.argv[2])].decode(), struct.unpack_from("<I", d, 8)[0])' "$1" "$2" 2>&1) || true
    if [ "$got" = "512 $2 $3" ]; then echo "  the folder: $(basename "$1") is 512 bytes, run $3"
    else fail "$1: $got (wanted run $3)"; fi
}

# a card, as a folder: test/run.sh's tree
tree() {
    rm -rf "$1"
    mkdir -p "$1/NOTES" "$1/PLUGINS"
    printf 'Hello from the card.\nThis is the first file.\n' > "$1/A first file.txt"
    i=1; : > "$1/NOTES/Long story.txt"
    while [ $i -le 60 ]; do echo "Line $i of a story long enough to scroll, and to wrap around the screen's 30 columns." >> "$1/NOTES/Long story.txt"; i=$((i + 1)); done
}

# ---- the plugin in src/ (tools/new-plugin.py rewrites this part) ----
# (C-BIOS for the Mac builds: src/cbios_rom.h, from vendor/cbios)
[ -f "$kit/src/cbios_rom.h" ] || "$kit/tools/cbios.sh" "$kit/src/cbios_rom.h"
if wanted msx; then
    run msx "$kit/src" --ms 3000 --script "1500:joy=fire 1700:joy="
    expect 'note "MSX: no file opened"' 'back to the menu'
fi
if wanted msx-usb; then
    # test/carts/input.asm from a card folder (named apart from test/run.sh's
    # msx-usb, whose card is an image: the pictures are compared, not the logs)
    t=$out/msx-card-folder; rm -rf "$t"; mkdir -p "$t/MSX"
    (cd "$kit/test/carts" && sjasmplus --nologo --msg=err --raw="$t/MSX/INPUT.mx1" input.asm >/dev/null)
    run msx-card "$kit/src" --card "$t" --open /MSX/INPUT.mx1 --ms 6000 \
        --script "3500:keydown=04 3500:mods=02 3500:joy=up+fire 3600:pad=south+left 4000:snap=keys 4200:keyup=04 4200:mods=0 4200:joy= 4200:pad=off 4500:mouse=40,20,1 5000:mouse=-10,5,0 5500:snap=mouse"
    expect 'usb: the port is a host' 'note "MSX PLAIN GUESS"' 'back to the menu'
    same_as msx-usb picture
fi
# ---- the examples ----
# (left out with examples/: tools/new-plugin.py --examples keeps them)

[ $compared = 0 ] && echo "(no test/host pictures to compare with: test/run.sh hasn't run here)"
[ $status = 0 ] && echo "all simulator checks passed (pictures: $out/*/screen.png)"
exit $status
