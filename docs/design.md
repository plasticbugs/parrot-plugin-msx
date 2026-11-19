# How the MSX plugin works

The plugin runs an MSX1 with a cartridge in it on the cart's RP2350, and
puts the MSX's picture and sound on the ColecoVision. It plays cartridge
ROMs only: no BASIC, no disks, no tapes.

The two machines are close. Both have a Z80 at 3.58 MHz and a TMS9918A
video chip with 16K of VRAM. The MSX has an AY-3-8910 for sound and the
ColecoVision has an SN76489. So the cart runs the MSX's Z80, memory and
sound chip, and copies the MSX's VRAM and video registers to the
console's own TMS9918A. Nothing is drawn on the cart.

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
the copy the SMS plugin took from pico-smsplus. It was picked over the
others by measuring them. Each core ran the same MSX (this plugin's
memory, mapper and VDP code) on a model of the RP2350's Cortex-M33, with
four games, frames 400 to 460, in model cycles per frame:

| Game | MAME Z80 | fMSX's Z80 (Marat Fayzullin) | superzazu/z80 |
|---|---|---|---|
| Road Fighter | 44,017 median, 67,123 max | 275,448, 285,008 | 687,307, 704,043 |
| Super Laydock | 60,588, 128,859 | 92,469, 164,225 | 1,474,520, 1,474,621 |
| Nemesis 2 | 89,070, 543,731 | 297,877, 574,307 | 722,627, 1,177,534 |
| R-Type | 476,164, 480,693 | 444,184, 445,835 | 997,652, 1,005,709 |

The MAME core is the fastest of the three on most games, because it
skips idle loops (`JR $`, `DJNZ $`, `DEC BC` delay loops) in one step. It
already runs the SMS plugin on this cart. fMSX's Z80 is as fast on busy
games, but its license needs Marat Fayzullin's permission before a
changed copy is given out. superzazu/z80 is MIT, but it is about twice as
slow. blueMSX wasn't measured: it's a whole emulator built around its
own board and device code, much bigger than this plugin, which has to
fit in the RP2350's 520K of SRAM with its data.

### Its license, and what follows from it

The notice at the top of `vendor/z80/z80.c` says:

- it's freeware for non-commercial use;
- it can be given out, changed or not, if Juergen Buchmueller is in the
  credits;
- each changed file must say it has been changed;
- the notice must stay in the source;
- commercial use needs his permission.

This plugin is free, and it's never sold or bundled with the cart. It's
a separate download that each owner picks, which is the non-commercial
use the notice allows. The README credits him. `z80.c` says what was
changed and marks each change (`vendor/z80/VENDORED.md` lists them), and
the notice is untouched.

This copy came from pico-smsplus, which is under the GPL-3.0, and that
port changed `z80.c` (the code that runs from RAM, and the deferred
interrupt). So the plugin as a whole is under the GPL-3.0, as the SMS
plugin is, and its full source is published with it.

The GPL doesn't allow extra restrictions, and the Z80's non-commercial
term is one. The SMS plugin is in the same position. While the plugins
are free, nothing is in conflict. If any commercial use comes up, the
Z80 would have to be replaced (superzazu/z80 is MIT), or Juergen
Buchmueller asked.

### Changes for the MSX

- **The M1 wait state.** An MSX adds one wait state to every opcode
  fetch, which makes the Z80 about 10% slower than a ColecoVision's.
  `Z80_M1_WAIT` adds it everywhere the core counts time: the opcode
  dispatch, the block instructions' repeats, the idle-loop shortcuts and
  HALT. Without the shortcuts counting it, LDIR ran fast, and Penguin
  Adventure's start crashed (its interrupt came in the middle of its own
  setup).
- **A double charge.** When a block instruction (LDIR and the rest) was
  repeating as the time slice ran out, one pass was charged twice. Now
  it isn't.

With both, `test/carts/timing.asm` counts the same time left in a frame
after LDIR, a DEC BC loop, DJNZ $ and OTIR as openMSX does, give or take
one pass of a 39-cycle loop.

## The machine

- **Slots.** Slot 0 is the BIOS (32K) and C-BIOS's logo ROM at
  0x8000. Slot 1 is the cartridge, slot 2 is empty, and slot 3 is 64K of
  RAM. Port 0xA8 selects them. There are no expanded slots.
- **The BIOS.** C-BIOS 0.29 is built in. It's assembled from
  `vendor/cbios` when the plugin is built, and checked against C-BIOS's
  own ROMs by SHA-1. If a 32K file is at `/MSX/MSX.ROM` on the card, the
  plugin uses it instead (a real MSX1 BIOS, which the player provides),
  without the logo ROM.
- **The cartridge.** The ROM is read from the card into the PSRAM,
  padded to a power of two with 0xFF. It can be up to 4 MB, and a
  player's BIOS goes in the PSRAM's upper half. The Z80 reads through a
  table of 8K pages, so a mapper switch only changes a pointer.
- **Mappers.** Plain ROMs from 8K to 64K (placed where their header
  says they start), ASCII8, ASCII16, Konami, Konami SCC, R-Type, and the generic 8K
  and 16K ones. ASCII8 and ASCII16 can have SRAM (2K or 8K), as Hydlide
  II and Harry Fox do. The SRAM is saved to `<game>.sav` beside the ROM
  once it has stayed the same for a second, and when RESET is pressed.
- **The SCC.** Its registers are read and written at 0x9800-0x98FF when
  bank 0x3F is selected at 0x9000. Its five voices go to the sound
  conversion.
- **Video.** `src/tms.c` keeps 16K of VRAM, the 8 registers, the
  address latch and the read-ahead byte. It also makes the status byte:
  the frame flag at each frame, and the fifth-sprite flag, its sprite
  number and the collision flag, worked out from the sprite table the way
  the chip does it. `test/carts/video.asm` shows the status the BIOS read,
  and it matches openMSX's.
- **The PSG.** Its 16 registers. Register 14 reads the joystick port
  that register 15 selects. Bit 6 is clear (an international keyboard)
  and bit 7 is set (no tape), as openMSX has them.
- **Timing.** A frame is 59,736 Z80 cycles (262 lines of 228). The VDP
  interrupt comes at the start of each frame.

## Mapper detection

`romtype_find()` looks in three places, and the last one that answers
wins:

1. `src/romlist.h`, 34 ROMs that the guess gets wrong, by SHA-1. It's
   made by `tools/romlist.py` from openMSX's software database.
2. The guess. A ROM of 32K or less is plain. A bigger one is scored by
   the addresses its code writes to with `LD (nnnn),A`, as openMSX does
   it. 0x5000, 0x9000 and 0xB000 point to Konami SCC; 0x4000, 0x8000 and
   0xA000 to Konami; 0x6800 and 0x7800 to ASCII8; 0x77FF to ASCII16; and
   0x6000 and 0x7000 count for several. On the No-Intro MSX set, the
   guess and the list together get 96.5% right.
3. A tag in the file's name, which the player can add: `[ASCII8]`,
   `[ASCII16]`, `[KONAMI]`, `[KONAMISCC]` (or `[SCC]`), `[RTYPE]`,
   `[PLAIN]`, `[8K]`, `[16K]`.

The menu's note shows what was picked and why, like `MSX KONAMI SCC
GUESS`.

## The picture: mirroring VRAM

The console's video chip is the same as the MSX's, so the picture comes
from copying VRAM. The frame kernel runs one command list per TV frame
(`gbkernel.asm`). The list has two parts. The first runs during the
vertical blank, and the second while the picture is drawn. Writes are
cheaper in the blank: a run of n VRAM bytes costs 106 + 23n cycles there,
and 106 + 31n later. The template keeps the blank part under 13,960
cycles and the whole list under 55,736, which is about 1,800 bytes of
VRAM a frame at most. An MSX game can rewrite all 16K in a frame, so
`src/mirror.c` keeps a copy of what the console's VRAM will hold and
sends only what differs, most visible first:

1. The PSG bytes for this frame (see below).
2. The registers that changed. R1 always keeps the console's frame
   interrupt (0x20) and 16K (0x80) on.
3. The sprite attribute table, then the name table, in the blank, so
   sprites and tiles move with the TV frame.
4. The pattern table, the colour table and the sprite patterns.
5. The rest of VRAM, a little each frame, from where the last frame left
   off.

The tracking is done in 64-byte blocks. A block is marked when the MSX
writes to it, and cleared once the console has the same bytes. Runs take
a few unchanged bytes along (4 in the blank, 3 later) rather than paying
for a new run. A run that doesn't fit in the blank goes on in the second
part.

Two rules keep a half-copied picture off the screen:

- **Table moves.** When a register moves a table (R2 to R6), the
  console keeps the old value until the new table's bytes have arrived,
  for up to 4 frames.
- **Screen back on.** Most games turn the screen off while they fill
  VRAM. If a game turns it back on while more than 1,400 bytes are still
  to go, the console's screen stays off until they've arrived, for up to
  20 frames.

### What it costs

`test/stats` runs a ROM for 3,600 frames (a minute). SPACE and fire are
pressed now and then. It plays each list into a copy of the console's
VRAM, and checks that every frame without a backlog leaves the console's
VRAM the same as the MSX's. All 26 games tried pass that check, and
their longest list fits the limit.

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

"Frames behind" means frames that ended with VRAM still to send, out of
3,600. Nearly all of them come at scene changes, when a game rewrites its
tables with the screen on. A game's normal play needs well under a
list's room. The most behind at once, after a frame, was 259 bytes
(Eggerland Mystery).

`tools/mame-replay.sh` plays the lists the console model received to
the SDK's real kernel in headless MAME. For R-Type, Boulder Dash and
Nemesis 2, 30 seconds each, every list ran, none took longer than its
TV frame, and the last picture was the model's, pixel for pixel.

### Speed on the RP2350

A model of the Cortex-M33 (an instruction-set simulator with the XIP
cache in front of the PSRAM) ran whole frames: the MSX, the sound
conversion and the mirror. These are frames 600 to 1,200 of each game, in
cycles. At 150 MHz, a TV frame is 2.5 million cycles.

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

The worst frame is about a quarter of the time a TV frame gives. The
model leaves out USB, card reads and the SDK's own work, and its PSRAM
and SRAM timing is an estimate. Only the console can confirm it, and the
menu's note after RESET reports it (see below).

## The sound

`src/sound.c` looks at the AY's registers (and the SCC's) once a frame,
and writes what the SN76489 needs to sound the same:

- **Tones.** The AY's tone period is the SN's divider as it is: the AY
  runs at half the clock and divides by 16, and the SN divides by 32. An
  SCC voice's period plus one is too. Below the SN's lowest note (a
  divider over 1,023), it goes up an octave at a time.
- **Voices.** The SN's three tones go to the three loudest of the AY's
  tones and the SCC's voices. A voice keeps its tone while it sounds.
- **Noise.** One noise for the AY's three channels, as loud as the
  loudest channel that has it on. When the SN's third tone is free, the
  noise is clocked by it, at the AY's exact rate. Otherwise it uses the
  nearest of the SN's fixed rates.
- **Volume.** The AY's 16 levels (about 3 dB apart) are mapped onto the
  SN's 2 dB steps, and the SCC's linear volume a step lower.
- **Envelope.** It's worked out halfway through each frame. Slow
  envelopes step as they would. Fast repeating ones (a buzz) count as
  their average. A buzz on a channel with its tone off is the AY using
  the envelope as a wave, and it becomes a tone at the envelope's pitch.

Sample playback (a game writing the volume thousands of times a second)
can't be followed at 60 writes a second, and is left out.
`test/carts/sound.asm` checks each rule's bytes in the console model.

## The controls

| From | On the MSX |
|---|---|
| The console's joystick and fire buttons | Joystick 1 (port 1): left fire is trigger A, right fire is trigger B |
| The console's keypad | `*` is SPACE, `#` is F1 (pause in Konami's games), and the digits are the number keys |
| USB pad 1 | Plays along with the console's controller: d-pad or left stick, bottom button trigger A, right button trigger B, top button SPACE, left button F1, + SPACE, − F1 |
| USB pad 2 | Joystick 2 (port 2): d-pad or left stick, bottom button trigger A, right button trigger B |
| A USB keyboard | The MSX keyboard (US layout), key for key. Left Alt is GRAPH, right Alt is CODE, Pause is STOP, End is SELECT, F1-F5 are F1-F5, F6-F10 are SHIFT and F1-F5, and the keypad is the MSX keypad |
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
| `test/carts/run.sh` | The test carts in `test/carts`, built with sjasmplus. `video`, four `mapper` builds and `input` must give openMSX's picture, pixel for pixel (openMSX runs headless, machine C-BIOS_MSX1). Each mapper build must be guessed right. `timing` must count what openMSX counts, give or take one. `sound`, the Konami SCC mapper build and `input` run in the console model, and the SN76489 bytes are checked |
| `test/games/run.sh DIR` | Each ROM in a folder of your own runs in the console model, with a line of results each and pictures |
| `test/stats/build.sh` | `test/stats/stats.c`, which measures the lists (the tables above) and checks the mirror |
| `test/run.sh` | The kit's checks: the plugin with no file, and with `input.asm` and USB input, in the console model, then the lists replayed on the real kernel in headless MAME (`MAME_ROMPATH` must have `coleco.zip`) |
| `sim/test.sh` | The same two in the simulator, headless |

No ROM or BIOS image is in the repository. The test carts are built
from source, and C-BIOS is assembled from source. Games for
`test/games/run.sh` are your own.

## What only the console can show

These are the checks to make on a real ColecoVision with the cart.

1. **Start.** Copy `MSX.CVP` to `/PLUGINS`, and a few `.mx1` files to
   `/MSX`. Pick one. The C-BIOS logo shows, then the game. After RESET,
   the menu's note shows the mapper, then `MSX 60/s 0 late ...`.
2. **Speed.** Play Nemesis 2, R-Type and Knightmare II for a few
   minutes each. Press RESET. The note should say 60/s, and 0 late or
   close to it. The time at the end (the longest frame) should be well
   under 16.7 ms. The M33 model says about 4.5 ms at worst.
3. **Picture.** There should be no tearing or flicker beyond what the
   game has on an MSX. Check scene changes: the screen may stay black for
   a few frames, and that's expected. Watch for half-drawn screens and
   wrong colours after a scene change.
4. **Sprites.** On a real TMS9918A, a fifth sprite on a line drops out.
   Games that cycle sprites to show them all should flicker as they do on
   an MSX.
5. **Sound.** Konami's SCC games (Nemesis 2, Salamander) should play
   their music. AY games should have their tunes and noise drums. Listen
   for wrong octaves on bass notes, and for buzzes.
6. **Controller.** Check the stick, both fire buttons, keypad `*`
   (SPACE) and `#` (F1, pause in Konami's games).
7. **USB.** Use a powered hub. Check a pad as player 1 and a second pad
   as player 2. On a keyboard, typing a name in a game, F1 to F5, and
   GRAPH (left Alt). A mouse with a game that uses one (or
   `test/carts/input.asm`).
8. **Saves.** Hydlide II: save in the game, press RESET, start it
   again, and the save should be there. `Hydlide II.sav` should be on
   the card beside the ROM.
9. **Own BIOS.** Put a 32K MSX1 BIOS at `/MSX/MSX.ROM`. The game should
   start without the C-BIOS logo.
10. **Big ROMs.** A 1 MB or bigger ROM (a MegaROM) should load in a few
    seconds and play. This checks the PSRAM and the XIP cache.

## Known gaps

- MSX2 games (`.mx2` files that need the MSX2's video chip) won't run.
  The plugin opens them in case they're really MSX1 games.
- No BASIC, disks, tapes or MSX-MUSIC.
- More mappers exist (CrossBlaim, Harry Fox's own, Game Master 2, and
  ASCII8 with 32K of SRAM). The games that need them either fail or need
  a tag.
- The VDP interrupt comes at the start of each emulated frame, not at
  line 192, and the status byte doesn't change during a frame. Split
  screen effects that poll the status mid-frame won't work.
- Sample playback through the PSG is left out.
- The mouse has no scaling. A USB mouse moves the MSX mouse one step for
  each count.
