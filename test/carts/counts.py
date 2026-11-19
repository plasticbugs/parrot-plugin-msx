"""counts.py A.vram B.vram -- timing.asm's counts on two screens (VRAM
dumps: 16K, then the 8 registers) must be within one pass of each other.
The phase of the frame against HALT's 5-cycle steps moves a count by one
on its own, so one is allowed."""
import sys


def counts(path):
    d = open(path, "rb").read()
    base = (d[0x4000 + 2] & 0x0F) << 10
    rows = {}
    for r in range(1, 8):
        line = d[base + r * 32: base + r * 32 + 32].decode("latin-1").split()
        if len(line) == 2:
            rows[line[0]] = int(line[1], 16)
    return rows


a, b = counts(sys.argv[1]), counts(sys.argv[2])
ok = len(a) == 5 and a.keys() == b.keys()
for k in a:
    same = k in b and abs(a[k] - b[k]) <= 1
    ok = ok and same
    print(f"  {k:6} {a[k]:5} {b.get(k, -1):5}{'' if same else '  <- more than one apart'}")
sys.exit(0 if ok else 1)
