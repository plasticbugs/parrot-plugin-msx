#!/usr/bin/env python3
"""Make src/romlist.h: the ROMs whose mapper the guess (src/romtype.c) can't
find, by the first 4 bytes of their SHA-1.

  tools/romlist.py SOFTWAREDB.XML GUESSES.TSV > src/romlist.h

SOFTWAREDB.XML is openMSX's software database (share/softwaredb.xml in an
openMSX install). GUESSES.TSV is what the guess made of a set of ROMs, one
line each: mapper, size, path (test/romguess.c prints it). The list takes:
  - every MSX1 ROM the database says has SRAM or the R-Type mapper (no guess
    can tell those);
  - every ROM in the set that the guess gets wrong, with all the database's
    other dumps of the same title and type.
"""
import hashlib, re, sys
from collections import defaultdict

OURS = {'PLAIN': 0, 'ASCII8': 1, 'ASCII16': 2, 'KONAMI': 3, 'KONAMISCC': 4, 'RTYPE': 5, '8K': 6, '16K': 7}
# the database's types we run: (our mapper, SRAM in K)
DB = {'Mirrored': ('PLAIN', 0), 'Normal': ('PLAIN', 0), 'page2': ('PLAIN', 0), 'page12': ('PLAIN', 0),
      'page23': ('PLAIN', 0), 'page1': ('PLAIN', 0), 'page012': ('PLAIN', 0), 'page0123': ('PLAIN', 0),
      '16kb': ('PLAIN', 0), '8kB': ('8K', 0), 'GenericKonami': ('8K', 0),
      'ASCII8': ('ASCII8', 0), 'ASCII16': ('ASCII16', 0), 'Konami': ('KONAMI', 0), 'KonamiSCC': ('KONAMISCC', 0),
      'R-Type': ('RTYPE', 0), 'ASCII16SRAM2': ('ASCII16', 2), 'ASCII8SRAM8': ('ASCII8', 8), 'ASCII8SRAM2': ('ASCII8', 2)}
ALWAYS = {'R-Type', 'ASCII16SRAM2', 'ASCII8SRAM8', 'ASCII8SRAM2'}

db, by_title = {}, defaultdict(list)
text = open(sys.argv[1], encoding='utf-8').read()
for sw in re.finditer(r'<software>(.*?)</software>', text, re.S):
    s = sw.group(1)
    if re.search(r'<system>(.*?)</system>', s).group(1) != 'MSX':
        continue
    title = re.search(r'<title>(.*?)</title>', s).group(1)
    for d in re.finditer(r'<(?:megarom|rom)><type>([^<]*)</type><hash>([0-9a-f]{40})</hash>', s):
        db[d.group(2)] = (d.group(1), title)
        by_title[(title, d.group(1))].append(d.group(2))

out = {}
for h, (t, title) in db.items():
    if t in ALWAYS:
        out[h] = (t, title)
for line in open(sys.argv[2]):
    guess, _, path = line.rstrip('\n').split('\t')
    h = hashlib.sha1(open(path, 'rb').read()).hexdigest()
    if h not in db:
        continue
    t, title = db[h]
    if t in DB and DB[t] != (guess, 0):
        for h2 in by_title[(title, t)]:
            out[h2] = (t, title)

prefixes = defaultdict(set)
for h, v in out.items():
    prefixes[h[:8]].add(DB[v[0]])
print('// romlist.h -- made by tools/romlist.py; don\'t edit. The ROMs whose mapper')
print('// romtype.c\'s guess can\'t find, by the first 4 bytes of their SHA-1. From')
print('// openMSX\'s software database (softwaredb.xml: Nicolas Beyaert, the blueMSX')
print('// team, the openMSX team).')
print('#pragma once\n#include <stdint.h>\n')
print('typedef struct {\n    uint8_t sha1[4];\n    uint8_t mapper, sram_kb;   // msx_mapper_t, K of SRAM\n} romlist_t;\n')
print('static const romlist_t ROMLIST[] = {')
n = 0
for h, (t, title) in sorted(out.items(), key=lambda kv: (kv[1][1], kv[0])):
    if len(prefixes[h[:8]]) > 1:
        continue                                    # two types on one prefix: leave it to the guess
    m, k = DB[t]
    b = ', '.join('0x' + h[i:i + 2].upper() for i in range(0, 8, 2))
    print('    {{%s}, %d, %d},   // %s (%s)' % (b, OURS[m], k, title.replace('*/', ''), t))
    n += 1
print('};\n#define ROMLIST_N %d' % n)
