# C-BIOS, vendored

- What: C-BIOS 0.29, a free MSX BIOS that starts cartridges
  (http://cbios.sourceforge.net/). From the release's source archive,
  `cbios-0.29a.zip`.
- Files: the sources the MSX1 main ROM and logo ROM are built from
  (`src/main_msx1.asm`, `src/logo_msx1.asm` and what they include),
  `derived/asm/version.asm`, `version.txt`, and the license and notes,
  `cbios.txt`. The MSX2, disk, BASIC and music sources are left out. No
  file is changed.
- Built by `tools/cbios.sh` with sjasmplus. The two ROMs come out byte for
  byte as C-BIOS 0.29's own (SHA-1 `2f997e8a…` and `9fbbe400…`, which the
  script checks), and become `cbios_rom.h`. No ROM image is in the
  repository.
- License: BSD 2-clause, in `cbios.txt`. A binary that contains C-BIOS
  must carry its copyright notice, conditions and disclaimer in its
  documentation: the plugin's README (which the .CVP carries) does.
