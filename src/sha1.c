// sha1.c -- SHA-1 (FIPS 180-4), plain and small.
#include "sha1.h"

#include <string.h>

static inline uint32_t rol(uint32_t x, int n) { return x << n | x >> (32 - n); }

static void block(sha1_t *s, const uint8_t *p) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | p[4 * i + 1] << 16 | p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) f = (b & c) | (~b & d), k = 0x5A827999;
        else if (i < 40) f = b ^ c ^ d, k = 0x6ED9EBA1;
        else if (i < 60) f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
        else f = b ^ c ^ d, k = 0xCA62C1D6;
        const uint32_t t = rol(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = t;
    }
    s->h[0] += a;
    s->h[1] += b;
    s->h[2] += c;
    s->h[3] += d;
    s->h[4] += e;
}

void sha1_init(sha1_t *s) {
    static const uint32_t H[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    memcpy(s->h, H, sizeof H);
    s->len = 0;
    s->n = 0;
}

void sha1_add(sha1_t *s, const void *data, size_t n) {
    const uint8_t *p = data;
    s->len += n;
    while (n) {
        if (s->n == 0 && n >= 64) {
            block(s, p);
            p += 64;
            n -= 64;
            continue;
        }
        size_t k = 64 - s->n < n ? 64 - s->n : n;
        memcpy(s->buf + s->n, p, k);
        s->n += k;
        p += k;
        n -= k;
        if (s->n == 64) {
            block(s, s->buf);
            s->n = 0;
        }
    }
}

void sha1_done(sha1_t *s, uint8_t out[20]) {
    const uint64_t bits = s->len * 8;
    const uint8_t one = 0x80, zero = 0;
    sha1_add(s, &one, 1);
    while (s->n != 56) sha1_add(s, &zero, 1);
    uint8_t l[8];
    for (int i = 0; i < 8; i++) l[i] = (uint8_t)(bits >> (56 - 8 * i));
    sha1_add(s, l, 8);
    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 4; j++) out[4 * i + j] = (uint8_t)(s->h[i] >> (24 - 8 * j));
}
