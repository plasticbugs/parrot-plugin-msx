# How the MSX plugin works

The plugin runs an MSX1 with a cartridge in it on the cart's RP2350, and
puts the MSX's picture and sound on the ColecoVision. It plays cartridge
ROMs only. There's no BASIC, and no disks or tapes.

The two machines are close. Both have a Z80 at 3.58 MHz and a TMS9918A
video chip with 16K of VRAM. The MSX has an AY-3-8910 for sound and the
ColecoVision has an SN76489. So the cart runs the MSX's Z80, memory and
sound chip, and copies the MSX's VRAM and video registers to the
console's own TMS9918A, which draws the picture.

## The parts

| File | What it does |
|---|---|
| `src/main.c` | Loads the ROM and the BIOS, starts the MSX, runs it a frame at a time, reads the controls, writes the saves |
| `src/msx.c` | The machine: slots, memory, mappers, SRAM, the SCC, the PSG's registers, the keyboard matrix, the joystick ports and the mouse |
| `src/cpu.c`, `src/z80core.c` | The Z80, `vendor/z80` |
| `src/tms.c` | The MSX's video chip as the Z80 sees it: VRAM, registers, the status byte (frame flag, fifth sprite, collision) |
| `src/mirror.c` | The MSX's VRAM and registers copied to the console, a frame kernel list each frame |
| `src/sound.c` | The AY (and the SCC) turned into SN76489 writes, once a frame |
| `src/keymap.c` | A USB keyboard and the controllers' keys on the MSX keyboard |
| `src/romtype.c`, `src/romlist.h` | Which mapper a ROM uses |
| `src/sha1.c` | The ROM's SHA-1, for the mapper list |
| `vendor/cbios` | C-BIOS 0.29, assembled at build time by `tools/cbios.sh` |

## The Z80 core

The plugin uses Juergen Buchmueller's portable Z80 from MAME (V2.7), in
the copy the SMS plugin took from pico-smsplus. I picked it by measuring
three cores. Each ran the same MSX (this plugin's memory, mapper and VDP
code) on a model of the RP2350's Cortex-M33, for frames 400 to 460 of
four games. These are model cycles per frame, median and most:

| Game | MAME Z80 | fMSX's Z80 (Marat Fayzullin) | superzazu/z80 |
|---|---|---|---|
| Road Fighter | 44,017 / 67,123 | 275,448 / 285,008 | 687,307 / 704,043 |
| Super Laydock | 60,588 / 128,859 | 92,469 / 164,225 | 1,474,520 / 1,474,621 |
| Nemesis 2 | 89,070 / 543,731 | 297,877 / 574,307 | 722,627 / 1,177,534 |
| R-Type | 476,164 / 480,693 | 444,184 / 445,835 | 997,652 / 1,005,709 |

The MAME core was fastest on three of the four. It runs idle loops
(`JR $`, `DJNZ $`, `DEC BC` delay loops) in one step, and it already runs
the SMS plugin on this cart. fMSX's Z80 is a little faster on R-Type,
which keeps the CPU busy, but its license needs Marat Fayzullin's
permission before a changed copy is given out. superzazu/z80 is MIT, and
it was two to three times slower than fMSX's on the busy games.
blueMSX's core wasn't measured. It comes inside a whole emulator, with
its own board and device code, and this plugin has to fit in the
RP2350's 520K of SRAM with its data.

### Its license, and what follows from it

The notice at the top of `vendor/z80/z80.c` says:

- it's freeware for non-commercial use;
- it can be given out, changed or not, if Juergen Buchmueller is in the
  credits;
- each changed file must say it has been changed;
- the notice must stay in the source;
- commercial use needs his permission.

This plugin is free. It's never sold, and it isn't bundled with the
cart. Each owner downloads it separately, which is the non-commercial
use the notice allows. The README credits him. `z80.c` says it was
changed and marks each change (`vendor/z80/VENDORED.md` lists them), and
his notice is as it was.

This copy came from pico-smsplus, which is under the GPL-3.0, and that
port changed `z80.c` (it runs from RAM, and the interrupt can be
deferred). So the plugin as a whole is under the GPL-3.0, as the SMS
plugin is, and its full source is published with it.

The GPL doesn't allow extra restrictions, and the Z80's non-commercial
term is one. The SMS plugin is in the same position. As long as the
plugins are free, the two don't clash in practice. If any commercial use
comes up, the Z80 would have to be replaced (superzazu/z80 is MIT), or
Juergen Buchmueller asked.

### Changes for the MSX

An MSX adds one wait state to every opcode fetch, which makes its Z80
about 10% slower than a ColecoVision's. `Z80_M1_WAIT` adds it everywhere
the core counts time: the opcode dispatch, the repeats of the block
instructions, the idle-loop shortcuts and HALT. Before the shortcuts
counted it, LDIR ran fast and Penguin Adventure crashed at its start. An
interrupt came before the game had set up what its handler reads, and
the stack overflowed.

The core also charged one pass twice when a block instruction (LDIR and
the rest) was repeating as its time slice ran out. It's charged once
now.

With both changes, `test/carts/timing.asm` counts the same time left in
a frame after LDIR, a DEC BC loop, DJNZ $ and OTIR as openMSX does, give
or take one pass of a 39-cycle loop.

## The machine

Slot 0 is the BIOS (32K), with C-BIOS's logo ROM at 0x8000. Slot 1 is
the cartridge, slot 2 is empty, and slot 3 is 64K of RAM. Port 0xA8
selects them. There are no expanded slots.

C-BIOS 0.29 is built in. It's assembled from `vendor/cbios` when the
plugin is built, and the result is checked against C-BIOS's own ROMs by
SHA-1. If there's a 32K file at `/MSX/MSX.ROM` on the card, the plugin
uses that instead, without the logo ROM. That's for a real MSX1 BIOS,
which the player provides.

The ROM is read from the card into the PSRAM, padded to a power of two
with 0xFF. It can be up to 4 MB, and a player's BIOS goes in the upper
half of the PSRAM. The Z80 reads through a table of 8K pages, so a
mapper switch only changes a pointer.

The mappers are plain ROMs from 8K to 64K (placed where their header
says they start), ASCII8, ASCII16, Konami, Konami SCC, R-Type, and the
generic 8K and 16K ones. ASCII8 and ASCII16 can have 2K or 8K of SRAM,
as Hydlide II, Xanadu and Deep Dungeon do. The SRAM goes to
`<game>.sav` beside the ROM once it has stayed the same for a second,
and when RESET is pressed.

The SCC's registers are at 0x9800-0x98FF while bank 0x3F is selected at
0x9000. Its five voices go to the sound conversion.

`src/tms.c` keeps 16K of VRAM, the 8 registers, the address latch and
the read-ahead byte. It also makes the status byte. The frame flag is
set at each frame, and the fifth-sprite flag, its sprite number and the
collision flag are worked out from the sprite table the way the chip
does it. `test/carts/video.asm` shows the status the BIOS read, and it
matches openMSX's.

The PSG has its 16 registers. Register 14 reads the joystick port that
register 15 selects. In it, bit 6 is clear (an international keyboard)
and bit 7 is set (no tape), as openMSX has them.

A frame is 59,736 Z80 cycles (262 lines of 228). The VDP interrupt
comes at the start of each frame.

## Mapper detection

`romtype_find()` asks three sources in turn, and the last one that
answers wins.

1. `src/romlist.h` holds 34 ROMs by SHA-1, made by `tools/romlist.py`
   from openMSX's software database. It has the R-Type and SRAM games,
   which no guess can tell apart, and the ROMs the guess got wrong on
   the No-Intro MSX set.
2. The guess. A ROM of 32K or less is plain. A bigger one is scored by
   the addresses its code writes to with `LD (nnnn),A`, as openMSX does
   it. 0x5000, 0x9000 and 0xB000 point to Konami SCC, 0x4000, 0x8000 and
   0xA000 to Konami, 0x6800 and 0x7800 to ASCII8, and 0x77FF to ASCII16.
   0x6000 and 0x7000 count for several. On the No-Intro MSX set, the
   guess alone got 96.5% right, going by openMSX's database.
3. A tag the player adds to the file's name: `[ASCII8]`, `[ASCII16]`,
   `[KONAMI]`, `[KONAMISCC]` (or `[SCC]`), `[RTYPE]`, `[PLAIN]`, `[8K]`
   or `[16K]`.

The menu's note shows what was picked and why, like `MSX KONAMI SCC
GUESS`.

## The picture: mirroring VRAM

The console's video chip is the same as the MSX's, so the picture comes
from copying VRAM. The frame kernel (`gbkernel.asm`) runs one command
list per TV frame. The first part of a list runs during the vertical
blank, and the second while the picture is drawn. Writes are cheaper in
the blank. A run of n VRAM bytes costs 106 + 23n cycles there, and
106 + 31n later. The template keeps the blank part under 13,960 cycles
and the whole list under 55,736, which allows about 1,900 bytes of VRAM
a frame. An MSX game can rewrite all 16K in a frame, so `src/mirror.c`
keeps a copy of what the console's VRAM will hold and sends only what
differs, in this order:

1. The PSG bytes for this frame (see below).
2. The registers that changed. R1 always keeps the console's frame
   interrupt (0x20) and 16K (0x80) on.
3. The sprite attribute table, then the name table, in the blank, so
   sprites and tiles move with the TV frame.
4. The pattern table, the colour table and the sprite patterns.
5. The rest of VRAM, a little each frame, from where the last frame left
   off.

`src/mirror.c` tracks VRAM in 64-byte blocks. A block is marked when the
MSX writes to it, and cleared once the console has the same bytes. A run
carries up to 4 unchanged bytes (3 after the blank) to save starting a
new run, which costs 106 cycles. A run that doesn't fit in the blank
goes on in the second part.

Two rules keep a half-copied picture off the screen.

- When a register moves a table (R2 to R6), the console keeps the old
  value until the new table's bytes have arrived, for up to 4 frames.
- Most games turn the screen off while they fill VRAM. If a game turns
  it back on while more than 1,400 bytes are still to go, the console's
  screen stays off until they've arrived, for up to 20 frames.

### What it costs

`test/stats` runs a ROM for 3,600 frames (a minute), pressing SPACE and
fire now and then. It plays each list into a copy of the console's
VRAM, and checks that every frame without a backlog leaves the console's
VRAM the same as the MSX's. All 26 games I tried pass that check, and
their longest lists fit the limit. Here are some of them:

| Game | Mapper | VRAM bytes a list | Frames behind | Most in a row | Screen held off |
|---|---|---|---|---|---|
| Antarctic Adventure | plain | 22 | 1 | 1 | 0 |
| Boulder Dash | plain | 29 | 11 | 5 | 5 |
| Dragon Quest | ASCII8 | 33 | 11 | 3 | 0 |
| Galaga | plain | 96 | 1 | 1 | 0 |
| Golvellius | ASCII16 | 16 | 1 | 1 | 0 |
| Hydlide II | ASCII16 + SRAM | 17 | 7 | 3 | 0 |
| Knightmare | plain | 92 | 5 | 4 | 0 |
| Knightmare II: Maze of Galious | Konami | 36 | 5 | 4 | 0 |
| Nemesis 2 | Konami SCC | 30 | 5 | 4 | 0 |
| Penguin Adventure | Konami | 26 | 5 | 4 | 0 |
| R-Type | R-Type | 99 | 13 | 7 | 0 |
| Super Cobra | plain | 13 | 9 | 8 | 0 |
| Zanac | plain | 96 | 1 | 1 | 0 |

"VRAM bytes a list" is the average. "Frames behind" counts the frames,
out of 3,600, that ended with VRAM still to send. Nearly all of them
come at scene changes, when a game rewrites its tables with the screen
on. In normal play a game needs far less than a list can carry. The
most left over after any frame was 259 bytes, in Eggerland Mystery.

`tools/mame-replay.sh` plays the lists that the console model received
to the SDK's real kernel in headless MAME. I did that for 30 seconds
each of R-Type, Boulder Dash and Nemesis 2. Every list ran, none took
longer than its TV frame, and the last picture was the model's, pixel
for pixel.

### Speed on the RP2350

A model of the Cortex-M33 (an instruction-set simulator, with the XIP
cache in front of the PSRAM) ran whole frames: the MSX, the sound
conversion and the mirror. These are frames 600 to 1,200 of each game,
in cycles. At 150 MHz, a TV frame is 2.5 million cycles.

| Game | Median | 90% of frames | Most |
|---|---|---|---|
| Nemesis 2 | 426,375 | 491,911 | 565,005 |
| R-Type | 428,779 | 442,359 | 658,695 |
| Salamander | 127,819 | 473,171 | 583,209 |
| Knightmare II: Maze of Galious | 476,396 | 477,487 | 479,334 |
| Zanac | 426,836 | 479,317 | 483,315 |
| Knightmare | 205,702 | 269,994 | 506,781 |
| Galaga | 227,478 | 353,747 | 426,274 |
| Super Laydock | 92,076 | 161,771 | 161,773 |
| Road Fighter | 154,306 | 157,532 | 363,336 |
| Penguin Adventure | 95,690 | 378,113 | 522,122 |

The worst frame takes about a quarter of a TV frame. The model leaves
out USB, card reads and the SDK's own work, and its PSRAM and SRAM
timing is an estimate. Only the console can confirm it, and the menu's
note after RESET reports what it saw (see Pacing).

## The sound

`src/sound.c` looks at the AY's registers (and the SCC's) once a frame,
and writes what the SN76489 needs to sound the same.

- The AY's tone period works as the SN's divider unchanged. The AY runs
  at half the clock and divides by 16, and the SN divides by 32. An SCC
  voice's period plus one works the same way. A note below the SN's
  lowest (a divider over 1,023) goes up an octave at a time until it
  fits.
- The SN's three tones go to the three loudest of the AY's tones and the
  SCC's voices. A voice keeps its tone while it sounds.
- The AY's three channels share one noise, as loud as the loudest
  channel that has it on. When the SN's third tone is free, it clocks
  the noise at the AY's exact rate. Otherwise the noise uses the nearest
  of the SN's fixed rates.
- The AY's 16 levels (about 3 dB apart) are mapped onto the SN's 2 dB
  steps, and the SCC's linear volume a step lower.
- The envelope is worked out halfway through each frame. Slow envelopes
  step as they would. A fast repeating one (a buzz) counts as its
  average. A buzz on a channel with its tone off means the AY is using
  the envelope as a wave, so it becomes a tone at the envelope's pitch.

Sample playback (a game writing the volume thousands of times a second)
can't be followed at 60 writes a second, so it's left out.
`test/carts/sound.asm` checks each rule's bytes in the console model.

## The controls

| From | On the MSX |
|---|---|
| The console's joystick and fire buttons | Joystick 1 (port 1). Left fire is trigger A, right fire is trigger B |
| The console's keypad | `*` is SPACE, `#` is F1 (pause in Konami's games), and the digits are the number keys |
| USB pad 1 | The same as the console's controller. D-pad or left stick, bottom button trigger A, right button trigger B, top button SPACE, left button F1, + SPACE, − F1 |
| USB pad 2 | Joystick 2 (port 2). D-pad or left stick, bottom button trigger A, right button trigger B |
| A USB keyboard | The MSX keyboard (US layout), key for key. Left Alt is GRAPH, right Alt is CODE, Pause is STOP, End is SELECT, F6-F10 are SHIFT and F1-F5, and the keypad is the MSX keypad |
| A USB mouse | An MSX mouse in port 1. It answers when the game reads a mouse there (port 1's pin 8 changing), and for 30 frames after that. The rest of the time, port 1 is the joystick |

A key held on two inputs (the keypad's `*` and the keyboard's space
bar, say) stays down until both let go.

## Pacing

The MSX runs one frame for each frame the console shows. The console's
frames come from the frame kernel's interrupt, so the MSX runs at the
console's 59.92 Hz. The main loop builds a frame's list only when a new
console frame has started (`cvf_frames()`), then offers it until the
kernel takes it. The note after RESET shows the frames run each second,
how many console frames went by without a new MSX frame (late), and the
longest time one MSX frame took on the cart, like `MSX 60/s 0 late
4.2ms`. Anything near 16.7 ms is too slow.

## Tests on the Mac

| Command | What it checks |
|---|---|
| `test/carts/run.sh` | The test carts in `test/carts`, built with sjasmplus. `video`, four `mapper` builds and `input` must give openMSX's picture, pixel for pixel (openMSX runs headless, machine C-BIOS_MSX1). Each mapper build must be guessed right. `timing` must count what openMSX counts, give or take one. `sound`, the Konami SCC mapper build and `input` run in the console model, which checks the SN76489 bytes |
| `test/games/run.sh DIR` | Each ROM in a folder of your own runs in the console model, with a line of results and a picture for each |
| `test/stats/build.sh` | Builds `test/stats/stats.c`, which measures the lists (the tables above) and checks the mirror |
| `test/run.sh` | The kit's checks: the plugin with no file, and with `input.asm` and USB input, in the console model, then the lists replayed on the real kernel in headless MAME (`MAME_ROMPATH` must have `coleco.zip`) |
| `sim/test.sh` | The same two in the simulator, headless |

No ROM or BIOS image is in the repository. The test carts and C-BIOS
are built from their source, and the games for `test/games/run.sh` are
your own.

## What only the console can show

1. Copy `MSX.CVP` to `/PLUGINS`, and a few `.mx1` files to `/MSX`, and
   pick one. The C-BIOS logo shows, then the game. After RESET, the
   menu's note shows the mapper, then `MSX 60/s 0 late ...`.
2. Play Nemesis 2, R-Type and Knightmare II for a few minutes each, then
   press RESET. The note should say 60/s, and 0 late or close to it. The
   time at the end (the longest frame) should be well under 16.7 ms. The
   M33 model says about 4.4 ms at worst.
3. There should be no tearing or flicker beyond what the game has on an
   MSX. At a scene change the screen may stay black for a few frames,
   which is expected. Watch for half-drawn screens and wrong colours
   after one.
4. On a real TMS9918A, a fifth sprite on a line drops out. Games that
   cycle their sprites to show them all should flicker as they do on an
   MSX.
5. Konami's SCC games (Nemesis 2, Salamander) should play their music,
   and AY games their tunes and noise drums. Listen for bass notes in
   the wrong octave, and for buzzes.
6. On the controller, check the stick, both fire buttons, and keypad `*`
   (SPACE) and `#` (F1, pause in Konami's games).
7. With a powered hub, check a pad as player 1 and a second pad as
   player 2. On a keyboard, type a name in a game, and try F1 to F5 and
   GRAPH (left Alt). Try a mouse with a game that uses one, or with
   `test/carts/input.asm`.
8. In Hydlide II, save in the game, press RESET and start it again. The
   save should still be there, and `Hydlide II.sav` should be on the
   card beside the ROM.
9. Put a 32K MSX1 BIOS at `/MSX/MSX.ROM`. The game should start without
   the C-BIOS logo.
10. A ROM of 1 MB or more (a MegaROM) should load in a few seconds and
    play. This checks the PSRAM and the XIP cache.

## Known gaps

- MSX2 games (`.mx2` files that need the MSX2's video chip) won't run.
  The plugin opens `.mx2` files in case they're really MSX1 games.
- No BASIC, disks, tapes or MSX-MUSIC.
- Cross Blaim, Harry Fox: Yuki no Maou Hen and Konami's Game Master 2
  have mappers of their own, and some games use ASCII8 with 32K of SRAM.
  None of those is here, so those games fail.
- The VDP interrupt comes at the start of each emulated frame, not at
  line 192, and the status byte doesn't change during a frame. Split
  screen effects that poll the status mid-frame won't work.
- Sample playback through the PSG is left out.
- The mouse has no scaling. A USB mouse moves the MSX mouse one step for
  each count.
