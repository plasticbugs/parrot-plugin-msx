# What in this repository came from elsewhere

The plugin's own code is in `src/`, `test/carts/`, `test/stats/`,
`test/games/` and `tools/cbios.sh` and `tools/romlist.py`. As a whole the
plugin is under the GNU GPL, version 3 (`LICENSE`), because its Z80 core
came from a GPL-3.0 project. The plugin template it started from (the
build files, `lib/`, `test/host/`, `test/run.sh`, `sim/`, `tools/` and
`docs/`, except `docs/design.md`) and the copy of the cart's SDK in
`sdk/` are by the Parrot Cart contributors, under the MIT license below.
These parts came from other projects and keep their own terms:

| Where | What | License |
|---|---|---|
| `vendor/z80/z80.c`, `z80.h`, `z80daa.h`, `cpuintrf.h`, `osd_cpu.h` | Juergen Buchmueller's portable Z80 (V2.7), from MAME, as SMS Plus and the [pico-smsplus](https://github.com/PicoPlus-devel/pico-smsplus) port (Frank Hoedemakers) have it. The changes made here are marked in `z80.c` and listed in `vendor/z80/VENDORED.md` | his notice at the top of `z80.c`: "released as freeware for non-commercial purposes". pico-smsplus, and so its changes to the file, is GPL-3.0 |
| `vendor/cbios/` | [C-BIOS](https://cbios.sourceforge.net/) 0.29, the sources of its MSX1 main and logo ROMs, unchanged. `tools/cbios.sh` assembles them into the plugin | BSD 2-Clause (`vendor/cbios/cbios.txt`) |
| `src/romlist.h` | 34 ROMs' SHA-1s and cartridge types, from openMSX's software database (`softwaredb.xml`: Nicolas Beyaert, the blueMSX team, the openMSX team), made by `tools/romlist.py` | facts about the ROMs, credited. openMSX is GPL-2.0 |
| `sdk/firmware/revc/sdk/tinyusb/` | [TinyUSB](https://github.com/hathach/tinyusb) (Ha Thach and contributors), with the cart's fixes, listed in its `VENDORED.md` | MIT (`sdk/firmware/revc/sdk/tinyusb/LICENSE`) |
| `sdk/firmware/revc/midihello/tinyusb_fix/hub.c` | TinyUSB 0.18's hub driver with one fix (used only when a plugin builds against the pico-sdk's TinyUSB) | MIT (its header) |
| `sim/third_party/rtmidi/` | [RtMidi](https://github.com/thestk/rtmidi) 6.1.0 (Gary P. Scavone and contributors), unchanged: the simulator's MIDI ports | MIT-style (`sim/third_party/rtmidi/LICENSE`) |
| `cmake/pico_sdk_import.cmake` | from the [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) | BSD 3-Clause |
| `LICENSE`, `tools/licenses/` | license texts, as their publishers give them | verbatim copies, as each text allows |

## What each license asks

- **The Z80.** Juergen Buchmueller must be in the credits (the README
  does that). A changed file must say so (`z80.c` does, under his
  notice, and marks each change), and the notice must stay. It's for
  non-commercial use: the plugin is free, a separate download, never
  sold or bundled with the cart. Commercial use would need his
  permission. The GPL-3.0 doesn't allow extra restrictions like that
  one; the SMS plugin's Z80 is in the same position. If the plugin were
  ever sold, the Z80 would have to be replaced by one under a plain
  license, or he would have to be asked.
- **C-BIOS.** A binary that contains it must carry its copyright notice,
  conditions and disclaimer in its documentation. The README, which the
  .CVP carries and the cart shows, has them in full.
- **GPL-3.0.** Whoever gets the plugin is owed its full source, which is
  this repository. A link to it goes beside each download.

The built plugin also contains the Raspberry Pi Pico SDK's runtime (BSD
3-Clause) and TinyUSB (MIT).

No ROM or BIOS image is in this repository. C-BIOS is built from its
source, and the test carts from theirs.

The 8x8 screen font's letter shapes come from the 1982 arcade game Mr. Do!

## The plugin template's license

```
MIT License

Copyright (c) 2026 Parrot Cart contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
