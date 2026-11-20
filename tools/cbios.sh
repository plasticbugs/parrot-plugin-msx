#!/bin/sh
# tools/cbios.sh OUT.h -- C-BIOS's MSX1 main and logo ROMs, assembled from
# vendor/cbios with sjasmplus, as a C header (cbios_main[], cbios_logo[]).
# The ROMs come out byte for byte as C-BIOS 0.29's own (their SHA-1s are
# checked). CMake runs this; test/run.sh and tools/sim.sh run it for src/.
set -e
here=$(cd "$(dirname "$0")/.." && pwd)
case $1 in /*|[A-Za-z]:*) out=$1 ;; *) out=$PWD/$1 ;; esac   # (D:/... is CMake's on Windows)
tmp=${TMPDIR:-/tmp}/msx-cbios.$$
rm -rf "$tmp"
mkdir -p "$tmp"
cp -R "$here/vendor/cbios/." "$tmp/"
cd "$tmp/src"
sjasmplus --nologo --msg=err -I../derived/asm --raw=../main.rom main_msx1.asm
sjasmplus --nologo --msg=err -I../derived/asm --raw=../logo.rom logo_msx1.asm
cd ..
python3 -I - "$out" main.rom logo.rom <<'PY'
import hashlib, sys
want = {'main.rom': '2f997e8a57528518c82ab3693fdae243dbbcc508', 'logo.rom': '9fbbe400dbaf186aeba42e170d9424b032412c42'}
out = ['// made by tools/cbios.sh from vendor/cbios (C-BIOS 0.29, BSD 2-clause:',
       '// vendor/cbios/cbios.txt); not in git', '#pragma once', '#include <stdint.h>']
for f, name, size in ((sys.argv[2], 'cbios_main', 0x8000), (sys.argv[3], 'cbios_logo', 0x4000)):
    d = open(f, 'rb').read()
    if hashlib.sha1(d).hexdigest() != want[f]:
        sys.exit('cbios.sh: %s came out different from C-BIOS 0.29' % f)
    d = d.ljust(size, b'\xff')
    out.append('static const uint8_t %s[%d] = {' % (name, size))
    for i in range(0, size, 24):
        out.append('    ' + ','.join(str(b) for b in d[i:i + 24]) + ',')
    out.append('};')
open(sys.argv[1], 'w').write('\n'.join(out) + '\n')
PY
rm -rf "$tmp"
