// stats.c -- the MSX, the mirror and the sound without the cart: a ROM run
// for some frames on the Mac, each frame's list played to a model of the
// console's VRAM, and what that cost. Also built for the Cortex-M33 model
// (M33 defined: test/stats/m33.sh), which times each frame's work.
//
//   stats ROM FRAMES [MAPPER] [OUT_DIR]   (MAPPER: a tag, e.g. KONAMISCC; - to guess)
//
// STATS_KEYS="time row mask down|up ..." (time in seconds, as
// test/carts/omsx-dump.tcl's OMSX_KEYS) presses those keyboard matrix bits
// instead of the SPACE and fire now and then.
//
// Prints a line per second of frames and a summary: VRAM bytes a list,
// the list's cost against its limit, the backlog (bytes the console is
// still missing) and how long it lasted, and whether the console's VRAM
// came out the MSX's. SPACE and fire are pressed now and then.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbios_rom.h"
#include "mirror.h"
#include "msx.h"
#include "romtype.h"
#include "sha1.h"
#include "sound.h"

#ifdef M33
#include "sim.h"
#endif

static uint8_t con_vram[0x4000], con_reg[8];
static uint8_t bios[0xC000];
static mirror_t mirror;
static sound_t snd;
static uint8_t list[MIRROR_LIST_MAX];
uint32_t sim_sum;

// a list as the kernel runs it: false if it's malformed
static int play(const uint8_t *b, size_t n, int *psg_bytes) {
    size_t i = 0;
    int part = 0;
    while (i < n) {
        const uint8_t op = b[i++];
        if (op == 0) {
            if (++part == 2) return i == n;
            continue;
        }
        if (op == 1) {
            const uint16_t a = (uint16_t)(b[i] | (b[i + 1] & 0x3F) << 8);
            const uint8_t k = b[i + 2];
            for (int j = 0; j < k; j++) con_vram[(a + j) & 0x3FFF] = b[i + 3 + j];
            i += 3u + k;
        } else if (op == 2) {
            *psg_bytes += b[i];
            i += 1u + b[i];
        } else if (op == 3 && part == 0) {
            con_reg[b[i + 1] & 7] = b[i];
            i += 2;
        } else {
            return 0;
        }
    }
    return 0;
}

static void press(int f) {
#ifndef M33
    const char *k = getenv("STATS_KEYS");
    if (k) {
        double t;
        int row, mask, n;
        char how[8];
        while (sscanf(k, "%lf %d %d %7s%n", &t, &row, &mask, how, &n) == 4) {
            if ((int)(t * 59.922 + 0.5) == f)   // (the TMS9918A's 59.922 frames a second)
                for (int b = 0; b < 8; b++)
                    if (mask >> b & 1) msx_key(row, b, how[0] == 'd');
            k += n;
        }
        return;
    }
#endif
    const int s = f % 240;
    msx_key(8, 0, s >= 100 && s < 110);         // SPACE
    msx.joy[0] = (uint8_t)(s >= 160 && s < 170 ? JOY_A : 0);
}

#ifdef M33
int main(void) {
    const uint8_t *rom = sim_rom + 0x10000;
    uint32_t real, map;
    memcpy(&real, sim_rom + 0xC000, 4);
    memcpy(&map, sim_rom + 0xC004, 4);
    const int frames = (int)sim_frames;
    uint32_t size = 0x2000;
    while (size < real) size <<= 1;
    const romtype_t rt = {(msx_mapper_t)(map & 0xFF), (int)(map >> 8), "M33"};
    memcpy(bios, cbios_main, sizeof cbios_main);
    memcpy(bios + 0x8000, cbios_logo, sizeof cbios_logo);
    static uint8_t sram[0x2000];
#else
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "stats ROM FRAMES [MAPPER] [OUT_DIR]\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    const uint32_t real = (uint32_t)ftell(f);
    rewind(f);
    uint32_t size = 0x2000;
    while (size < real) size <<= 1;
    uint8_t *rom = malloc(size);
    memset(rom, 0xFF, size);
    if (fread(rom, 1, real, f) != real) return 2;
    fclose(f);
    const int frames = atoi(argv[2]);
    sha1_t h;
    uint8_t sum[20];
    sha1_init(&h);
    sha1_add(&h, rom, real);
    sha1_done(&h, sum);
    char tag[40] = "";
    if (argc > 3 && strcmp(argv[3], "-")) snprintf(tag, sizeof tag, "[%s]", argv[3]);
    const romtype_t rt = romtype_find(tag, sum, rom, real);
    memcpy(bios, cbios_main, sizeof cbios_main);
    memcpy(bios + 0x8000, cbios_logo, sizeof cbios_logo);
    static uint8_t sram[0x2000];
    printf("%s: %u bytes, %s (%s)%s\n", argv[1], real, msx_mapper_name[rt.mapper], rt.why, rt.sram_kb ? ", SRAM" : "");
#endif
    memset(sram, 0xFF, sizeof sram);
    msx_init(bios, 0x8000, bios + 0x8000, 0x4000);
    msx_cart(rom, size, real, rt.mapper, rt.sram_kb ? sram : NULL, rt.sram_kb);
    msx_reset();
    sound_reset(&snd, rt.mapper == MAP_KONAMI_SCC);
    mirror_reset(&mirror);
    memcpy(con_vram, mirror.vram, sizeof con_vram);
    memcpy(con_reg, mirror.reg, sizeof con_reg);

    uint64_t bytes = 0, psg = 0;
    uint32_t behind = 0, run = 0, longest = 0, held = 0, bad = 0, mismatch = 0, full = 0;
    for (int fr = 0; fr < frames; fr++) {
        press(fr);
#ifdef M33
        sim_begin(fr);
#endif
        msx_frame();
        uint8_t p[16];
        const int np = sound_frame(&snd, &msx, p);
        const size_t n = mirror_frame(&mirror, &msx.vdp, p, np, list);
#ifdef M33
        sim_end(fr);
#endif
        int pb = 0;
        if (!play(list, n, &pb)) bad++;
        psg += (uint64_t)pb;
        bytes += mirror.bytes;
        if (mirror.cost > 55736 - 200) full++;
        if (mirror.backlog) {
            behind++;
            if (++run > longest) longest = run;
        } else {
            run = 0;
            if (memcmp(con_vram, msx.vdp.vram, sizeof con_vram)) mismatch++;
        }
        if (!(con_reg[1] & 0x40) && (msx.vdp.reg[1] & 0x40)) held++;
#ifndef M33
        if ((fr + 1) % 600 == 0)
            printf("  frame %5d: %4u bytes, %5u T, backlog %5u, regs %02X %02X %02X %02X %02X %02X %02X %02X\n", fr + 1,
                   mirror.bytes, mirror.cost, mirror.backlog, con_reg[0], con_reg[1], con_reg[2], con_reg[3], con_reg[4],
                   con_reg[5], con_reg[6], con_reg[7]);
#endif
    }
    uint32_t s = 0;
    for (int i = 0; i < 0x4000; i++) s = s * 31 + con_vram[i];
    sim_sum = s;
#ifndef M33
    printf("  %d frames: %.0f VRAM bytes a list on average; %u lists full; longest %u T (limit 55736); %llu PSG bytes;\n"
           "  behind %u frames (%.1f%%), %u in a row at most, %u bytes at most; screen held off %u frames;\n"
           "  bad lists %u; caught-up frames with the console's VRAM not the MSX's: %u\n",
           frames, (double)bytes / frames, full, mirror.max_cost, (unsigned long long)psg, behind, 100.0 * behind / frames,
           longest, mirror.max_backlog, held, bad, mismatch);
    if (argc > 4) {
        char path[512];
        snprintf(path, sizeof path, "%s/console.vram", argv[4]);
        FILE *o = fopen(path, "wb");
        if (o) {
            fwrite(con_vram, 1, sizeof con_vram, o);
            fwrite(con_reg, 1, 8, o);
            fclose(o);
        }
    }
    return bad || mismatch ? 1 : 0;
#else
    return 0;
#endif
}
