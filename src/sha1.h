// sha1.h -- SHA-1, to look a ROM up in romlist.h while it loads.
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t h[5];
    uint64_t len;
    uint8_t buf[64];
    size_t n;
} sha1_t;

void sha1_init(sha1_t *s);
void sha1_add(sha1_t *s, const void *data, size_t n);
void sha1_done(sha1_t *s, uint8_t out[20]);
