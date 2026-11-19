// sound.h -- the MSX's sound on the ColecoVision's SN76489, once a frame.
//
// The MSX has an AY-3-8910 (three tones, a noise mixed into any of them,
// and a volume envelope), and a Konami SCC cartridge adds five wave-table
// voices. The console has three square-wave tones and a noise. Each frame
// this looks at the chips' registers and works out the nearest the SN76489
// can do:
//
//   tones   the AY's tone period is the SN's divider as it is (the AY runs
//           at half the clock and divides by 16, the SN by 32), and the
//           SCC's period + 1 is too. Below the SN's lowest note (a period
//           over 1023) it goes up an octave at a time.
//   voices  the three loudest of the AY's tones and the SCC's voices get
//           the SN's three tones; a voice keeps its tone while it sounds.
//   noise   one noise for the AY's three, as loud as the loudest channel
//           that has it on. With the SN's third tone free its pitch is
//           exact (the noise clocked by that tone); if not, the nearest
//           of the SN's fixed rates.
//   volume  the AY's 16 levels (about 3 dB apart) on the SN's 2 dB steps;
//           the SCC's linear volume likewise.
//   envelope worked out at each frame: slow ones step as they would, fast
//           repeating ones (a "buzz") count as their average. A buzz on a
//           channel with its tone off is the AY playing the envelope as a
//           wave: that becomes a tone at the envelope's pitch.
//
// Sample playback (a game writing the volume thousands of times a second)
// can't be followed at 60 writes a second, and is left out.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "msx.h"

typedef struct {
    // the SN76489 as the console will have it once the bytes so far have run
    uint16_t n[3];              // tone dividers (0xFFFF: not known)
    uint8_t att[4];             // attenuations, tones then noise (15: off)
    uint8_t noise;              // the noise register (0xFF: not known)
    int8_t voice[3];            // what each tone plays: 0-2 the AY's, 3-7 the SCC's, -1 nothing
    // the AY's envelope
    uint32_t env_restarts;      // msx.env_restarts when it last started
    uint32_t env_t;             // AY clocks since it started (saturates)
    bool scc;                   // the SCC's voices count (a Konami SCC cartridge)
    uint32_t frac;              // the clock's fraction, for 29829.5 a frame
} sound_t;

void sound_reset(sound_t *s, bool scc);

// The bytes for the SN76489 (port $FF) that bring the console's chip to
// the MSX's sound now, into out (16 at most). Returns how many.
int sound_frame(sound_t *s, const msx_t *m, uint8_t out[16]);

// for tests: the AY's envelope level (0-15) a time after it started
int sound_env_level(uint8_t shape, uint16_t period, uint32_t t);
