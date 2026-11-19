// render.c -- a VRAM dump (16K, then the 8 registers) as a 256x192 PNG,
// drawn by the console model's TMS9918A (test/host/tms9918.c). Prints the
// mode and the status the picture would leave.
//
//   render DUMP OUT.png
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include "tms9918.h"

static const uint8_t RGB[16][3] = {
    {0, 0, 0}, {0, 0, 0}, {33, 200, 66}, {94, 220, 120}, {84, 85, 237}, {125, 118, 252}, {212, 82, 77}, {66, 235, 245},
    {252, 85, 84}, {255, 121, 120}, {212, 193, 84}, {230, 206, 128}, {33, 176, 59}, {201, 91, 186}, {204, 204, 204}, {255, 255, 255}};

static void put32(FILE *f, uint32_t v) {
    const uint8_t b[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
    fwrite(b, 1, 4, f);
}

static void chunk(FILE *f, const char *type, const uint8_t *d, uint32_t n) {
    put32(f, n);
    uLong c = crc32(0, (const Bytef *)type, 4);
    fwrite(type, 1, 4, f);
    if (n) {
        fwrite(d, 1, n, f);
        c = crc32(c, d, n);
    }
    put32(f, (uint32_t)c);
}

int main(int argc, char **argv) {
    static uint8_t dump[0x4008], pic[192][256], raw[192 * 257], z[sizeof raw + 1024];
    if (argc != 3) {
        fprintf(stderr, "render DUMP OUT.png\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f || fread(dump, 1, sizeof dump, f) != sizeof dump) {
        fprintf(stderr, "%s: not a 16K + 8 dump\n", argv[1]);
        return 2;
    }
    fclose(f);
    const uint8_t st = tms_picture(dump, dump + 0x4000, pic);
    for (int y = 0; y < 192; y++) {
        raw[y * 257] = 0;
        memcpy(raw + y * 257 + 1, pic[y], 256);
    }
    uLongf zn = sizeof z;
    compress2(z, &zn, raw, sizeof raw, 9);
    f = fopen(argv[2], "wb");
    if (!f) return 2;
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    const uint8_t ihdr[13] = {0, 0, 1, 0, 0, 0, 0, 192, 8, 3, 0, 0, 0};
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "PLTE", &RGB[0][0], 48);
    chunk(f, "IDAT", z, (uint32_t)zn);
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    printf("%s: %s, status %02X\n", argv[2], tms_mode_name(dump + 0x4000), st);
    return 0;
}
