// tms.c -- the MSX's TMS9918A, as the MSX program sees it (tms.h).
#include "tms.h"

#include <string.h>

void tms_reset(tms_t *t) {
    memset(t, 0, sizeof *t);
    memset(t->dirty, 0xFF, sizeof t->dirty);      // the console's copy isn't known yet: all of it
}

static inline void mark(tms_t *t, uint16_t a) { t->dirty[a >> 11] |= 1u << (a >> 6 & 31); }

void tms_write_data(tms_t *t, uint8_t v) {
    t->second = false;
    t->vram[t->addr] = v;
    mark(t, t->addr);
    t->ahead = v;                               // (the TMS9918A's read-ahead takes what's written)
    t->addr = (t->addr + 1) & 0x3FFF;
}

uint8_t tms_read_data(tms_t *t) {
    t->second = false;
    const uint8_t r = t->ahead;
    t->ahead = t->vram[t->addr];
    t->addr = (t->addr + 1) & 0x3FFF;
    return r;
}

void tms_write_ctrl(tms_t *t, uint8_t v) {
    if (!t->second) {
        t->latch = v;
        t->second = true;
        t->addr = (t->addr & 0x3F00) | v;      // the TMS99x8 takes the low byte at once
        return;
    }
    t->second = false;
    if (v & 0x80) {                             // a register: 3 bits of its number on the TMS9918A
        static const uint8_t MASK[8] = {0x03, 0xFB, 0x0F, 0xFF, 0x07, 0x7F, 0x07, 0xFF};
        const int r = v & 7;
        t->reg[r] = t->latch & MASK[r];
        t->reg_writes++;
        if (r == 1 && !(t->latch & 0x40)) t->blanked = true;
        return;
    }
    t->addr = (uint16_t)((v & 0x3F) << 8 | t->latch);
    if (!(v & 0x40)) {                          // set up for reading: the first byte read ahead
        t->ahead = t->vram[t->addr];
        t->addr = (t->addr + 1) & 0x3FFF;
    }
}

uint8_t tms_read_status(tms_t *t) {
    t->second = false;
    const uint8_t r = t->status;
    t->status &= 0x1F;                          // F, 5S and C clear; the number stays
    return r;
}

void tms_take_dirty(tms_t *t, uint32_t out[8]) {
    for (int i = 0; i < 8; i++) {
        out[i] |= t->dirty[i];
        t->dirty[i] = 0;
    }
}

// a sprite's pixels on one of its rows, as 32 bits from its left (bit 31)
static uint32_t sprite_row(const tms_t *t, const uint8_t *s, int row, bool big, bool mag) {
    const uint16_t spt = (uint16_t)((t->reg[6] & 7) << 11);
    if (mag) row >>= 1;
    uint32_t bits;
    if (big) {
        const uint16_t base = (uint16_t)(spt + (s[2] & 0xFC) * 8);
        bits = (uint32_t)t->vram[(base + row) & 0x3FFF] << 8 | t->vram[(base + 16 + row) & 0x3FFF];
        bits <<= 16;
    } else {
        bits = (uint32_t)t->vram[(spt + s[2] * 8 + row) & 0x3FFF] << 24;
    }
    if (mag) {                                  // each pixel twice
        uint32_t m = 0;
        for (int i = 0; i < 16; i++)
            if (bits & 0x80000000u >> i) m |= 0xC0000000u >> (2 * i);
        bits = m;
    }
    return bits;
}

void tms_vblank(tms_t *t) {
    t->status |= 0x80;
    const uint8_t r1 = t->reg[1];
    if (!(r1 & 0x40) || (r1 & 0x10)) return;    // blanked, or text mode: no sprites looked at
    const bool big = r1 & 2, mag = r1 & 1;
    const int h = (big ? 16 : 8) << mag;
    const uint8_t *sat = &t->vram[(t->reg[5] & 0x7F) << 7];

    static uint8_t cnt[192], on[192][4], fifth[192];
    memset(cnt, 0, sizeof cnt);
    memset(fifth, 0xFF, sizeof fifth);
    int last = 31;
    for (int i = 0; i < 32; i++) {
        const uint8_t *s = sat + 4 * i;
        if (s[0] == 0xD0) { last = i; break; }
        const int y = s[0] > 0xE0 ? s[0] - 255 : s[0] + 1;   // drawn from the line after its Y
        for (int line = y < 0 ? 0 : y; line < y + h && line < 192; line++) {
            if (cnt[line] < 4) on[line][cnt[line]++] = (uint8_t)i;
            else if (fifth[line] == 0xFF) fifth[line] = (uint8_t)i;
        }
    }
    if (!(t->status & 0x40)) {
        int n = last;
        for (int line = 0; line < 192; line++)
            if (fifth[line] != 0xFF) { n = fifth[line] | 0x40; break; }
        t->status = (uint8_t)((t->status & 0xA0) | n);
    }
    if (t->status & 0x20) return;               // a collision already waiting to be read
    for (int line = 0; line < 192; line++) {
        if (cnt[line] < 2) continue;
        int x[4];
        uint32_t m[4];
        for (int k = 0; k < cnt[line]; k++) {
            const uint8_t *s = sat + 4 * on[line][k];
            const int y = s[0] > 0xE0 ? s[0] - 255 : s[0] + 1;
            x[k] = s[1] - (s[3] & 0x80 ? 32 : 0);
            m[k] = sprite_row(t, s, line - y, big, mag);
            if (x[k] < 0) m[k] &= 0xFFFFFFFFu >> -x[k];           // off the left: no collisions there
            if (x[k] > 224) m[k] &= ~(0xFFFFFFFFu >> (256 - x[k]));   // nor off the right
        }
        for (int a = 0; a < cnt[line]; a++)
            for (int b = a + 1; b < cnt[line]; b++) {
                const int d = x[b] - x[a];
                if (d >= 32 || d <= -32 || !m[a] || !m[b]) continue;
                const bool hit = d >= 0 ? (m[a] << d & m[b]) != 0 : (m[b] << -d & m[a]) != 0;
                if (hit) { t->status |= 0x20; return; }
            }
    }
}
