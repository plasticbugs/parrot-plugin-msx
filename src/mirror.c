// mirror.c -- the MSX's VRAM and registers on the console (mirror.h).
#include "mirror.h"

#include <string.h>

// what each command costs in Z80 cycles (gbkernel.asm's header), and the
// limits the template keeps to
#define FRAME_T     (262 * 228)
#define BLANK_T     (70 * 228)
#define BLANK_LIMIT (BLANK_T - 2000)    // the first part ends inside the vertical blank
#define LIST_LIMIT  (FRAME_T - 4000)    // and the list well inside the frame
#define START_T     480                 // the kernel's own start of a frame
#define RUN_T       106
#define VB_BYTE_T   23
#define ACT_BYTE_T  31
#define PSG_T       75
#define PSG_BYTE_T  52
#define REG_T       114
#define END_VB_T    54
#define END_T       52
#define CATCH_UP    1400                // bytes a list surely carries: less than that to go needs no hold
#define REG_WAIT    4                   // frames a base register waits for its table at most

typedef struct {
    uint8_t *b;
    size_t n;
    uint32_t t;
    uint32_t bytes;
    bool vblank;
} list_t;

static inline bool is_pending(const mirror_t *m, uint32_t blk) { return m->pending[blk >> 5] >> (blk & 31) & 1; }
static inline void clear_pending(mirror_t *m, uint32_t blk) { m->pending[blk >> 5] &= ~(1u << (blk & 31)); }

void mirror_reset(mirror_t *m) {
    memset(m, 0, sizeof *m);
    m->vram[0x1B00] = 0xD0;             // gbkernel.asm's vdp_init: all 0 but this
    static const uint8_t R[8] = {0x02, 0xE0, 0x06, 0xFF, 0x03, 0x36, 0x07, 0x01};
    memcpy(m->reg, R, sizeof R);
    memset(m->pending, 0xFF, sizeof m->pending);
}

static void l_end_vblank(list_t *l) {
    if (!l->vblank) return;
    l->b[l->n++] = 0;
    l->t += END_VB_T;
    l->vblank = false;
}

// how many bytes a run can carry now (0: none)
static int room(const list_t *l) {
    int32_t left;
    if (l->vblank) {
        left = BLANK_LIMIT - (int32_t)l->t - RUN_T - END_VB_T;
        return left < VB_BYTE_T ? 0 : left / VB_BYTE_T;
    }
    left = LIST_LIMIT - (int32_t)l->t - RUN_T - END_T;
    return left < ACT_BYTE_T ? 0 : left / ACT_BYTE_T;
}

static void l_run(list_t *l, mirror_t *m, const uint8_t *src, uint32_t a, uint32_t n) {
    l->b[l->n++] = 1;
    l->b[l->n++] = (uint8_t)a;
    l->b[l->n++] = (uint8_t)((a >> 8 & 0x3F) | 0x40);
    l->b[l->n++] = (uint8_t)n;
    memcpy(l->b + l->n, src + a, n);
    memcpy(m->vram + a, src + a, n);    // so it will be, once the list has run
    l->n += n;
    l->t += RUN_T + n * (l->vblank ? VB_BYTE_T : ACT_BYTE_T);
    l->bytes += n;
}

// The bytes in [a0, a1) that differ, as runs, while the list has time;
// false when it has run out. A run takes a few right bytes in its way
// rather than pay for a new run.
static bool send(list_t *l, mirror_t *m, const uint8_t *want, uint32_t a0, uint32_t a1) {
    uint32_t a = a0;
    bool ok = true;
    while (a < a1) {
        const uint32_t blk = a >> 6;
        if (!is_pending(m, blk)) {
            a = (blk + 1) << 6;
            continue;
        }
        if (want[a] == m->vram[a]) {
            a++;
            continue;
        }
        const uint32_t gap = l->vblank ? 4 : 3;
        uint32_t end = a + 1, p = a + 1;
        while (p < a1 && p - a < 255) {
            if (want[p] != m->vram[p]) end = ++p;
            else if (++p - end > gap) break;
        }
        uint32_t n = end - a;
        int fit = room(l);
        if (!fit && l->vblank) {
            l_end_vblank(l);
            fit = room(l);
        }
        if (!fit) {
            ok = false;
            break;
        }
        if (n > (uint32_t)fit) n = (uint32_t)fit;
        l_run(l, m, want, a, n);
        a += n;
    }
    // the blocks gone through that now match are done
    const uint32_t last = (a < a1 ? a : a1 - 1) >> 6;
    for (uint32_t blk = a0 >> 6; blk <= last && blk < 256; blk++)
        if (is_pending(m, blk) && !memcmp(want + blk * 64, m->vram + blk * 64, 64)) clear_pending(m, blk);
    return ok;
}

// where table r (2-6) is for the registers g, and its size (0: not used)
static void table(const uint8_t g[8], int r, uint32_t *base, uint32_t *size) {
    const bool text = g[1] & 0x10, multi = g[1] & 0x08, g2 = g[0] & 0x02;
    *base = 0;
    *size = 0;
    switch (r) {
    case 2: *base = (uint32_t)(g[2] & 0x0F) << 10; *size = text ? 960 : 768; break;
    case 3:
        if (g2) *base = (uint32_t)(g[3] & 0x80) << 6, *size = 0x1800;
        else if (!text && !multi) *base = (uint32_t)g[3] << 6, *size = 32;
        break;
    case 4:
        if (g2) *base = (uint32_t)(g[4] & 0x04) << 11, *size = 0x1800;
        else *base = (uint32_t)(g[4] & 7) << 11, *size = 0x800;
        break;
    case 5: *base = (uint32_t)(g[5] & 0x7F) << 7; *size = text ? 0 : 128; break;
    case 6: *base = (uint32_t)(g[6] & 7) << 11; *size = text ? 0 : 0x800; break;
    default: break;
    }
}

static bool region_pending(const mirror_t *m, uint32_t base, uint32_t size) {
    for (uint32_t blk = base >> 6; blk < (base + size + 63) >> 6 && blk < 256; blk++)
        if (is_pending(m, blk)) return true;
    return false;
}

size_t mirror_frame(mirror_t *m, tms_t *t, const uint8_t *psg, int npsg, uint8_t list[MIRROR_LIST_MAX]) {
    list_t l = {list, 0, START_T, 0, true};
    if (npsg > 0) {
        l.b[l.n++] = 2;
        l.b[l.n++] = (uint8_t)npsg;
        memcpy(l.b + l.n, psg, (size_t)npsg);
        l.n += (size_t)npsg;
        l.t += PSG_T + PSG_BYTE_T * (uint32_t)npsg;
    }

    // what changed, and how much of it there is
    uint32_t dirty[8] = {0};
    tms_take_dirty(t, dirty);
    uint32_t backlog = 0;
    for (int i = 0; i < 8; i++) m->pending[i] |= dirty[i];
    for (uint32_t blk = 0; blk < 256; blk++) {
        if (!is_pending(m, blk)) continue;
        const uint8_t *w = t->vram + blk * 64, *c = m->vram + blk * 64;
        uint32_t d = 0;
        for (int i = 0; i < 64; i++) d += w[i] != c[i];
        if (!d) clear_pending(m, blk);
        backlog += d;
    }

    // the registers the console should have
    const uint8_t *r = t->reg;
    uint8_t want[8] = {(uint8_t)(r[0] & 0x02), (uint8_t)((r[1] & 0x5B) | 0xA0), (uint8_t)(r[2] & 0x0F), r[3],
                       (uint8_t)(r[4] & 0x07), (uint8_t)(r[5] & 0x7F), (uint8_t)(r[6] & 0x07), r[7]};
    const bool msx_on = r[1] & 0x40;
    if (!msx_on || t->blanked) m->was_off = true;
    t->blanked = false;
    bool hold = false;
    if (!msx_on) {
        m->held = 0;
    } else if (m->was_off) {
        if (backlog > CATCH_UP && m->held < HOLD_FRAMES) {
            hold = true;                // the screen stays off until the console has the picture
            m->held++;
        } else {
            m->was_off = false;
            m->held = 0;
        }
    }
    if (hold) want[1] &= (uint8_t)~0x40;
    for (int k = 2; k <= 6; k++) {      // a table that moves waits for its new place to arrive
        if (want[k] == m->reg[k] || hold || !(want[1] & 0x40)) {
            m->reg_wait[k] = 0;
            continue;
        }
        uint32_t base, size;
        table(want, k, &base, &size);
        if (size && region_pending(m, base, size) && m->reg_wait[k] < REG_WAIT) {
            m->reg_wait[k]++;
            want[k] = m->reg[k];
        } else {
            m->reg_wait[k] = 0;
        }
    }
    for (int k = 0; k < 8; k++) {
        if (want[k] == m->reg[k]) continue;
        l.b[l.n++] = 3;
        l.b[l.n++] = want[k];
        l.b[l.n++] = (uint8_t)(0x80 | k);
        l.t += REG_T;
        m->reg[k] = want[k];
    }

    // VRAM, most visible first: the sprite attributes and the names, then
    // the tables the picture is drawn from, then the rest
    uint8_t g[8];
    memcpy(g, r, 8);
    static const int ORDER[5] = {5, 2, 4, 3, 6};
    uint8_t seen[256 / 8] = {0};
    bool ok = true;
    for (int i = 0; i < 5 && ok; i++) {
        uint32_t base, size;
        table(g, ORDER[i], &base, &size);
        if (!size) continue;
        ok = send(&l, m, t->vram, base, base + size);
        for (uint32_t blk = base >> 6; blk < (base + size + 63) >> 6 && blk < 256; blk++) seen[blk >> 3] |= (uint8_t)(1 << (blk & 7));
    }
    for (uint32_t k = 0; k < 256 && ok; k++) {
        const uint32_t blk = (m->cursor + k) & 255;
        if (seen[blk >> 3] >> (blk & 7) & 1 || !is_pending(m, blk)) continue;
        ok = send(&l, m, t->vram, blk * 64, blk * 64 + 64);
        if (!ok) m->cursor = blk;
    }

    l_end_vblank(&l);
    l.b[l.n++] = 0;
    l.t += END_T;

    backlog = 0;                        // what is still to go
    for (uint32_t blk = 0; blk < 256; blk++) {
        if (!is_pending(m, blk)) continue;
        const uint8_t *w = t->vram + blk * 64, *c = m->vram + blk * 64;
        for (int i = 0; i < 64; i++) backlog += w[i] != c[i];
    }
    m->bytes = l.bytes;
    m->cost = l.t;
    m->backlog = backlog;
    if (l.t > m->max_cost) m->max_cost = l.t;
    if (m->backlog > m->max_backlog) m->max_backlog = m->backlog;
    if (m->backlog) m->frames_behind++;
    return l.n;
}
