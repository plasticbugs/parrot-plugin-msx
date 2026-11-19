// sound.c -- the MSX's sound on the console's SN76489 (sound.h).
#include "sound.h"

#include <string.h>

#define AY_FRAME2 59659         // AY clocks in two frames (1789772.5 / 30)

// the AY's 16 levels as SN76489 attenuations (2 dB steps): the AY's
// measured output, 10 * log10 of it, rounded; level 3 and below kept just
// audible where the AY still is
static const uint8_t AY_ATT[16] = {15, 15, 14, 14, 14, 12, 11, 9, 8, 6, 5, 3, 2, 2, 1, 0};
// the SCC's linear volume likewise, a step down: it sits a little under the
// PSG on a real MSX
static const uint8_t SCC_ATT[16] = {15, 13, 10, 8, 7, 6, 5, 4, 4, 3, 3, 2, 2, 2, 1, 1};

void sound_reset(sound_t *s, bool scc) {
    memset(s, 0, sizeof *s);
    for (int c = 0; c < 3; c++) {
        s->n[c] = 0xFFFF;
        s->voice[c] = -1;
    }
    memset(s->att, 15, sizeof s->att);  // the frame kernel starts the chip silent
    s->noise = 0xFF;
    s->env_t = 0xFFFFFFFFu;
    s->scc = scc;
}

int sound_env_level(uint8_t shape, uint16_t period, uint32_t t) {
    const uint32_t step = t / (16u * (period ? period : 1));     // 16 steps a ramp
    const uint32_t ramp = step / 16, pos = step % 16;
    const bool cont = shape & 8, att = shape & 4, alt = shape & 2, hold = shape & 1;
    if (ramp == 0) return att ? (int)pos : 15 - (int)pos;
    if (!cont) return 0;
    if (hold) return att ^ alt ? 15 : 0;
    const bool up = att ^ (alt && (ramp & 1));
    return up ? (int)pos : 15 - (int)pos;
}

static uint16_t sn_divider(uint32_t n) {
    while (n > 1023) n >>= 1;           // under the SN's lowest note: up an octave
    return (uint16_t)(n ? n : 1);
}

typedef struct {
    int8_t id;
    uint8_t att;
    uint16_t n;
} voice_t;

int sound_frame(sound_t *s, const msx_t *m, uint8_t out[16]) {
    const uint8_t *r = m->psg;
    // the envelope: where it is halfway through this frame
    if (m->env_restarts != s->env_restarts) {
        s->env_restarts = m->env_restarts;
        s->env_t = 0;
    }
    const uint16_t ep = (uint16_t)(r[11] | r[12] << 8);
    const uint8_t shape = r[13] & 15;
    const bool env_fast = (shape & 8) && !(shape & 1) && 256u * (ep ? ep : 1) < AY_FRAME2;
    int env;
    if (s->env_t == 0xFFFFFFFFu) env = 0;                 // never started
    else if (env_fast) env = 10;                           // a buzz: its average, as heard
    else env = sound_env_level(shape, ep, s->env_t + AY_FRAME2 / 4);
    if (s->env_t != 0xFFFFFFFFu) {
        s->frac ^= 1;
        const uint32_t step = AY_FRAME2 / 2 + s->frac;     // 29829.5 a frame
        s->env_t = s->env_t > 0x7FFFFFFFu ? s->env_t : s->env_t + step;
    }

    // the voices that sound
    voice_t v[8];
    int nv = 0, noise_lvl = 0;
    for (int i = 0; i < 3; i++) {
        const bool tone = !(r[7] >> i & 1), noise = !(r[7] >> (i + 3) & 1);
        const bool use_env = r[8 + i] & 0x10;
        const int lvl = use_env ? env : r[8 + i] & 15;
        if (noise && lvl > noise_lvl) noise_lvl = lvl;
        const uint16_t tp = (uint16_t)(r[2 * i] | (r[2 * i + 1] & 15) << 8);
        if (tone && tp >= 4 && AY_ATT[lvl] < 15) {
            v[nv++] = (voice_t){(int8_t)i, AY_ATT[lvl], sn_divider(tp)};
        } else if (!tone && use_env && env_fast) {        // the envelope as the wave
            const uint32_t n = (shape & 2 ? 32u : 16u) * (ep ? ep : 1);
            v[nv++] = (voice_t){(int8_t)i, AY_ATT[12], sn_divider(n)};
        }
    }
    if (s->scc) {
        const uint8_t *c = m->scc;
        for (int j = 0; j < 5; j++) {
            const uint8_t vol = c[0x8A + j] & 15;
            const uint16_t p = (uint16_t)(c[0x80 + 2 * j] | (c[0x81 + 2 * j] & 15) << 8);
            if ((c[0x8F] >> j & 1) && vol && p >= 8) v[nv++] = (voice_t){(int8_t)(3 + j), SCC_ATT[vol], sn_divider(p + 1u)};
        }
    }

    // the loudest three, a voice already playing first among equals
    for (int a = 1; a < nv; a++) {
        const voice_t x = v[a];
        const bool xon = s->voice[0] == x.id || s->voice[1] == x.id || s->voice[2] == x.id;
        int b = a;
        for (; b > 0; b--) {
            const voice_t y = v[b - 1];
            const bool yon = s->voice[0] == y.id || s->voice[1] == y.id || s->voice[2] == y.id;
            if (y.att < x.att || (y.att == x.att && (yon || !xon))) break;
            v[b] = y;
        }
        v[b] = x;
    }
    const bool noise_on = AY_ATT[noise_lvl] < 15;
    int keep = nv < 3 ? nv : 3;
    const bool exact_noise = noise_on && keep < 3;       // the third tone is free to clock the noise
    const int chans = exact_noise ? 2 : 3;

    // each voice on the tone it had, if it can
    int8_t want[3] = {-1, -1, -1};
    bool placed[8] = {false};
    for (int k = 0; k < keep; k++)
        for (int c = 0; c < chans; c++)
            if (s->voice[c] == v[k].id && want[c] < 0) {
                want[c] = (int8_t)k;
                placed[k] = true;
                break;
            }
    for (int k = 0; k < keep; k++) {
        if (placed[k]) continue;
        for (int c = 0; c < chans; c++)
            if (want[c] < 0) {
                want[c] = (int8_t)k;
                placed[k] = true;
                break;
            }
    }

    // what the chip should hold
    uint16_t n[3];
    uint8_t att[4];
    for (int c = 0; c < 3; c++) {
        if (want[c] >= 0) {
            n[c] = v[want[c]].n;
            att[c] = v[want[c]].att;
            s->voice[c] = v[want[c]].id;
        } else {
            n[c] = s->n[c];
            att[c] = 15;
            s->voice[c] = -1;
        }
    }
    uint8_t noise = s->noise;
    att[3] = 15;
    if (noise_on) {
        const uint8_t np = r[6] & 31;
        att[3] = AY_ATT[noise_lvl];
        if (exact_noise) {
            noise = 0x04 | 3;                                // white, clocked by tone 3
            n[2] = (uint16_t)(np ? np : 1);
            att[2] = 15;
        } else {
            noise = (uint8_t)(0x04 | (np < 23 ? 0 : 1));     // 6991 Hz (NP 16) or 3496 Hz (NP 32)
        }
    }

    // the bytes that change it
    int k = 0;
    for (int c = 0; c < 3; c++) {
        if (att[c] < 15 || (c == 2 && noise_on && exact_noise)) {
            if (n[c] != s->n[c]) {
                out[k++] = (uint8_t)(0x80 | c << 5 | (n[c] & 15));
                out[k++] = (uint8_t)(n[c] >> 4 & 0x3F);
                s->n[c] = n[c];
            }
        }
        if (att[c] != s->att[c]) {
            out[k++] = (uint8_t)(0x90 | c << 5 | att[c]);
            s->att[c] = att[c];
        }
    }
    if (noise_on && noise != s->noise) {                    // (a write restarts the noise: only on a change)
        out[k++] = (uint8_t)(0xE0 | noise);
        s->noise = noise;
    }
    if (att[3] != s->att[3]) {
        out[k++] = (uint8_t)(0xF0 | att[3]);
        s->att[3] = att[3];
    }
    return k;
}
