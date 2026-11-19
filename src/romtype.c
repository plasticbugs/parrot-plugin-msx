// romtype.c -- the mapper a cartridge uses (romtype.h).
#include "romtype.h"

#include <ctype.h>
#include <string.h>

#include "romlist.h"

static const struct {
    const char *tag;
    msx_mapper_t m;
} TAGS[] = {
    {"PLAIN", MAP_PLAIN},       {"ASCII8", MAP_ASCII8},       {"ASCII16", MAP_ASCII16},
    {"KONAMISCC", MAP_KONAMI_SCC}, {"SCC", MAP_KONAMI_SCC},   {"KONAMI5", MAP_KONAMI_SCC},
    {"KONAMI", MAP_KONAMI},     {"KONAMI4", MAP_KONAMI},      {"RTYPE", MAP_RTYPE},
    {"R-TYPE", MAP_RTYPE},      {"8K", MAP_GENERIC8},         {"16K", MAP_GENERIC16},
};

bool romtype_tag(const char *name, msx_mapper_t *m) {
    if (!name) return false;
    for (const char *p = strchr(name, '['); p; p = strchr(p + 1, '[')) {
        const char *q = strchr(p, ']');
        if (!q) break;
        char t[16];
        size_t n = 0;
        for (const char *s = p + 1; s < q && n < sizeof t - 1; s++)
            if (*s != ' ' && *s != '_') t[n++] = (char)toupper((unsigned char)*s);
        t[n] = 0;
        for (size_t i = 0; i < sizeof TAGS / sizeof TAGS[0]; i++)
            if (!strcmp(t, TAGS[i].tag)) {
                *m = TAGS[i].m;
                return true;
            }
    }
    return false;
}

msx_mapper_t romtype_guess(const uint8_t *rom, uint32_t size) {
    if (size <= 0x8000) return MAP_PLAIN;
    // up to 64K with a header where a plain ROM has it and no bank writes
    // is plain too; the count below decides
    enum { K4, K5, A8, A16, N };
    uint32_t c[N] = {0};
    for (uint32_t i = 0; i + 2 < size; i++) {
        if (rom[i] != 0x32) continue;
        switch (rom[i + 1] | rom[i + 2] << 8) {
        case 0x5000: case 0x9000: case 0xB000: c[K5]++; break;
        case 0x4000: case 0x8000: case 0xA000: c[K4]++; break;
        case 0x6800: case 0x7800: c[A8]++; break;
        case 0x6000: c[K4]++; c[A8]++; c[A16]++; break;
        case 0x7000: c[K5]++; c[A8]++; c[A16]++; break;
        case 0x77FF: c[A16]++; break;
        default: break;
        }
    }
    if (c[A8]) c[A8]--;                         // a tie between ASCII8 and ASCII16 goes to ASCII16
    static const msx_mapper_t AS[N] = {MAP_KONAMI, MAP_KONAMI_SCC, MAP_ASCII8, MAP_ASCII16};
    int best = -1;
    for (int k = 0; k < N; k++)
        if (c[k] && (best < 0 || c[k] >= c[best])) best = k;
    if (best < 0) return size <= 0x10000 ? MAP_PLAIN : MAP_GENERIC8;
    if (size <= 0x10000 && c[best] < 2) return MAP_PLAIN;   // a stray $32 in 64K of code or data
    return AS[best];
}

romtype_t romtype_find(const char *name, const uint8_t sha1[20], const uint8_t *rom, uint32_t size) {
    romtype_t r = {MAP_PLAIN, 0, "GUESS"};
    if (sha1)
        for (size_t i = 0; i < ROMLIST_N; i++)
            if (!memcmp(ROMLIST[i].sha1, sha1, 4)) {
                r.mapper = (msx_mapper_t)ROMLIST[i].mapper;
                r.sram_kb = ROMLIST[i].sram_kb;
                r.why = "LIST";
                break;
            }
    if (r.why[0] == 'G') r.mapper = romtype_guess(rom, size);
    msx_mapper_t m;
    if (romtype_tag(name, &m)) {
        if (m != r.mapper) r.sram_kb = 0;
        r.mapper = m;
        r.why = "TAG";
    }
    return r;
}
