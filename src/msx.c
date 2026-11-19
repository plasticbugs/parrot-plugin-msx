// msx.c -- an MSX1 for cartridges (msx.h).
//
// The mapper code follows MSXemu_pico2's msx_core.c (MobileFF, MIT) and
// openMSX's descriptions of the mappers; the joystick port and the mouse
// follow openMSX's behaviour.
#include "msx.h"

#include <string.h>

#include "cpu.h"

msx_t msx;
uint8_t *msx_wpage[8];

const char *const msx_mapper_name[MAP_N] = {
    "PLAIN", "ASCII8", "ASCII16", "KONAMI", "KONAMI SCC", "R-TYPE", "8K", "16K",
};

static uint8_t ram[0x10000] __attribute__((aligned(4)));
static uint8_t ff_page[0x2000];                 // what an empty slot reads
static uint8_t scc_page[0x2000];                // $8000-$9FFF with the SCC on: ROM, then its registers
static const uint8_t *bios_page[8];

#define MOUSE_TIMEOUT_T 10738                   // 3 ms between pin 8 changes: a new read starts

static const uint8_t PSG_MASK[16] = {0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0xFF,
                                     0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF};

// ---- the cartridge's pages ----------------------------------------------------------

static inline const uint8_t *rom_at(uint32_t off) { return msx.rom + (off & (msx.rom_size - 1)); }

static void scc_page_build(void) {
    memcpy(scc_page, rom_at((uint32_t)msx.bank[2] * 0x2000), 0x1800);
    for (int k = 0; k < 8; k++) {
        uint8_t *p = scc_page + 0x1800 + 256 * k;
        memcpy(p, msx.scc, 0x80);                // the waves read back; the rest doesn't
        memset(p + 0x80, 0xFF, 0x80);
        memcpy(p + 0xA0, msx.scc + 0x60, 0x20);  // $98A0: the fifth voice's wave (the fourth's)
    }
}

static const uint8_t *cart_page(int p) {
    if (!msx.rom) return ff_page;
    switch (msx.mapper) {
    case MAP_PLAIN: {
        const uint32_t off = (uint32_t)(p * 0x2000 - (int)msx.base) & 0xFFFF;
        const bool pow2 = (msx.rom_real & (msx.rom_real - 1)) == 0;
        if (pow2 && msx.rom_real <= 0x8000) return rom_at(off);     // mirrored through the 64K
        return off < msx.rom_real ? msx.rom + off : ff_page;
    }
    case MAP_ASCII8: case MAP_KONAMI: case MAP_KONAMI_SCC: case MAP_GENERIC8:
        if (p < 2 || p > 5) return ff_page;
        if (msx.sram && (msx.sram_on >> (p - 2) & 1)) return msx.sram;
        if (p == 4 && msx.scc_on) return scc_page;
        return rom_at((uint32_t)msx.bank[p - 2] * 0x2000);
    case MAP_ASCII16: case MAP_GENERIC16: case MAP_RTYPE: {
        if (p < 2 || p > 5) return ff_page;
        const int r = (p - 2) >> 1;
        if (msx.sram && (msx.sram_on >> r & 1)) return msx.sram;
        return rom_at((uint32_t)msx.bank[r] * 0x4000 + (uint32_t)(p & 1) * 0x2000);
    }
    default:
        return ff_page;
    }
}

static void map_page(int p) {
    const int slot = msx.slots >> (p >> 1 << 1) & 3;
    switch (slot) {
    case 0: cpu_readmap[p] = (uint8_t *)bios_page[p]; msx_wpage[p] = NULL; break;
    case 1: cpu_readmap[p] = (uint8_t *)cart_page(p); msx_wpage[p] = NULL; break;
    case 2: cpu_readmap[p] = ff_page; msx_wpage[p] = NULL; break;
    default: cpu_readmap[p] = ram + p * 0x2000; msx_wpage[p] = ram + p * 0x2000; break;
    }
}

static void map_all(void) {
    for (int p = 0; p < 8; p++) map_page(p);
}

static void map_cart(void) {
    for (int p = 2; p < 6; p++)
        if ((msx.slots >> (p >> 1 << 1) & 3) == 1) map_page(p);
}

static void scc_write(uint16_t a, uint8_t v) {
    const uint8_t off = (uint8_t)a;
    msx.scc_writes++;
    if (off >= 0xE0) { msx.scc[0xE0] = v; return; }      // the test register
    if (off >= 0xA0) return;
    if (off >= 0x90 && off < 0xA0) { msx.scc[off - 0x10] = v; return; }   // $9890: $9880 again
    msx.scc[off] = v;
    if (off < 0x80) {
        for (int k = 0; k < 8; k++) scc_page[0x1800 + 256 * k + off] = v;
        if (off >= 0x60) for (int k = 0; k < 8; k++) scc_page[0x1800 + 256 * k + 0x40 + off] = v;
    }
}

static void cart_write(uint16_t a, uint8_t v) {
    switch (msx.mapper) {
    case MAP_ASCII8:
        if (a >= 0x6000 && a < 0x8000) {
            const int r = a >> 11 & 3;
            msx.bank[r] = v;
            msx.bank_writes++;
            if (msx.sram) {                     // a bank past the ROM's end: the SRAM
                const uint32_t pages = msx.rom_size / 0x2000;
                if (v >= pages && r >= 2) msx.sram_on |= (uint8_t)(1 << r);
                else msx.sram_on &= (uint8_t)~(1 << r);
            }
            map_cart();
        } else if (msx.sram && a >= 0x8000 && a < 0xC000 && (msx.sram_on >> ((a >> 13) - 2) & 1)) {
            const uint32_t m = (uint32_t)msx.sram_size * 1024 - 1;
            for (uint32_t i = a & m; i < 0x2000; i += m + 1) msx.sram[i] = v;
            msx.sram_writes++;
        }
        break;
    case MAP_ASCII16:
        if ((a & 0xF800) == 0x6000 || (a & 0xF800) == 0x7000) {
            const int r = a >> 12 & 1;
            msx.bank[r] = v;
            msx.bank_writes++;
            if (msx.sram) {                     // bit 4 of the bank: the SRAM, written at $8000-$BFFF
                if (v & 0x10) msx.sram_on |= (uint8_t)(1 << r);
                else msx.sram_on &= (uint8_t)~(1 << r);
            }
            map_cart();
        } else if (msx.sram && a >= 0x8000 && a < 0xC000 && (msx.sram_on & 2)) {
            const uint32_t m = (uint32_t)msx.sram_size * 1024 - 1;
            for (uint32_t i = a & m; i < 0x2000; i += m + 1) msx.sram[i] = v;
            msx.sram_writes++;
        }
        break;
    case MAP_KONAMI:
        if (a >= 0x6000 && a < 0xC000) {
            msx.bank[(a >> 13) - 2] = v;
            msx.bank_writes++;
            map_cart();
        }
        break;
    case MAP_KONAMI_SCC:
        if (msx.scc_on && a >= 0x9800 && a < 0xA000) { scc_write(a, v); break; }
        if ((a & 0x1800) == 0x1000 && a >= 0x4000 && a < 0xC000) {   // $5000, $7000, $9000, $B000 (2K each)
            const int r = (a >> 13) - 2;
            msx.bank[r] = v;
            msx.bank_writes++;
            if (r == 2) {
                msx.scc_on = (v & 0x3F) == 0x3F;
                if (msx.scc_on) scc_page_build();
            }
            map_cart();
        }
        break;
    case MAP_RTYPE:
        if (a >= 0x4000 && a < 0x8000) {
            msx.bank[1] = v & (v & 0x10 ? 0x17 : 0x1F);
            msx.bank_writes++;
            map_cart();
        }
        break;
    case MAP_GENERIC8:
        if (a >= 0x4000 && a < 0xC000) {
            msx.bank[(a >> 13) - 2] = v;
            msx.bank_writes++;
            map_cart();
        }
        break;
    case MAP_GENERIC16:
        if (a >= 0x4000 && a < 0xC000) {
            msx.bank[(a >> 14) - 1] = v;
            msx.bank_writes++;
            map_cart();
        }
        break;
    default:
        break;
    }
}

void msx_write_slow(uint16_t a, uint8_t v) {
    if ((msx.slots >> (a >> 14 << 1) & 3) == 1) cart_write(a, v);
}

// ---- the joystick ports and the mouse ------------------------------------------------

static bool mouse_active(void) { return msx.mouse_in && msx.frames - msx.mouse_frame < 30; }

static void mouse_strobe(void) {
    const uint32_t now = cpu_cycles();
    msx.mouse_phase = now - msx.mouse_t > MOUSE_TIMEOUT_T ? 0 : (msx.mouse_phase + 1) & 3;
    msx.mouse_t = now;
    msx.mouse_frame = msx.frames;
    if (msx.mouse_phase == 0) {                 // a new read: what it moved, as the MSX counts it
        int32_t x = -msx.mouse_dx, y = -msx.mouse_dy;
        x = x < -128 ? -128 : x > 127 ? 127 : x;
        y = y < -128 ? -128 : y > 127 ? 127 : y;
        msx.mouse_x = (int8_t)x;
        msx.mouse_y = (int8_t)y;
        msx.mouse_dx += x;
        msx.mouse_dy += y;
    }
}

static uint8_t joy_port(int port) {
    uint8_t v;
    if (port == 0 && mouse_active()) {
        static const uint8_t shift[4] = {4, 0, 4, 0};
        const int8_t c = msx.mouse_phase < 2 ? msx.mouse_x : msx.mouse_y;
        v = (uint8_t)(((uint8_t)c >> shift[msx.mouse_phase]) & 0x0F);
        v |= (uint8_t)((~msx.mouse_buttons & 3) << 4);
    } else {
        v = (uint8_t)(~msx.joy[port] & 0x3F);
    }
    v &= (uint8_t)((msx.psg[15] >> (2 * port) & 3) << 4 | 0xCF);   // pins 6 and 7 read only while they're out high
    return v | 0x80;                            // bit 6 clear: an international keyboard; bit 7 set: no tape (as openMSX)
}

// ---- ports ------------------------------------------------------------------------

uint8_t msx_in(uint8_t port) {
    switch (port) {
    case 0x98: return tms_read_data(&msx.vdp);
    case 0x99: {
        const uint8_t s = tms_read_status(&msx.vdp);
        cpu_irq(false);
        return s;
    }
    case 0xA2:
        if (msx.psg_addr == 14) return joy_port(msx.psg[15] >> 6 & 1);
        return msx.psg[msx.psg_addr];
    case 0xA8: return msx.slots;
    case 0xA9: {
        const int row = msx.ppi_c & 0x0F;
        return row < 11 ? msx.keys[row] : 0xFF;
    }
    case 0xAA: return msx.ppi_c;
    default: return 0xFF;
    }
}

void msx_out(uint8_t port, uint8_t v) {
    switch (port) {
    case 0x98: tms_write_data(&msx.vdp, v); break;
    case 0x99:
        tms_write_ctrl(&msx.vdp, v);
        cpu_irq(tms_irq(&msx.vdp));             // (R1's interrupt bit may have changed)
        break;
    case 0xA0: msx.psg_addr = v & 0x0F; break;
    case 0xA1: {
        const uint8_t r = msx.psg_addr;
        const uint8_t old = msx.psg[r];
        msx.psg[r] = v & PSG_MASK[r];
        if (r == 13) msx.env_restarts++;
        if (r == 15 && ((old ^ v) & 0x10)) mouse_strobe();   // port 1's pin 8
        break;
    }
    case 0xA8:
        msx.slots = v;
        map_all();
        break;
    case 0xAA: msx.ppi_c = v; break;
    case 0xAB:
        if (!(v & 0x80)) {                      // set or clear one bit of port C
            const uint8_t bit = (uint8_t)(1 << (v >> 1 & 7));
            msx.ppi_c = v & 1 ? (uint8_t)(msx.ppi_c | bit) : (uint8_t)(msx.ppi_c & ~bit);
        }
        break;
    default: break;
    }
}

// ---- the machine ------------------------------------------------------------------

void msx_init(const uint8_t *bios, uint32_t bios_size, const uint8_t *logo, uint32_t logo_size) {
    memset(ff_page, 0xFF, sizeof ff_page);
    for (int p = 0; p < 8; p++) {
        const uint32_t a = (uint32_t)p * 0x2000;
        if (a + 0x2000 <= bios_size) bios_page[p] = bios + a;
        else if (logo && a >= 0x8000 && a + 0x2000 <= 0x8000 + logo_size) bios_page[p] = logo + (a - 0x8000);
        else bios_page[p] = ff_page;
    }
}

uint32_t msx_plain_base(const uint8_t *rom, uint32_t size) {
    if (size >= 4 && rom[0] == 'A' && rom[1] == 'B') {
        const uint16_t init = (uint16_t)(rom[2] | rom[3] << 8), text = (uint16_t)(rom[8] | rom[9] << 8);
        uint16_t at = init ? init : text;
        if (!at) return 0x4000;
        at &= 0xC000;
        return at ? at : 0x4000;
    }
    if (size > 0x4000 && rom[0x4000] == 'A' && rom[0x4001] == 'B') return 0x0000;   // it starts at page 0
    if (size > 0x8000 && rom[0x8000] == 'A' && rom[0x8001] == 'B') return 0x0000;
    return 0x4000;
}

void msx_cart(const uint8_t *rom, uint32_t size, uint32_t real_size, msx_mapper_t m, uint8_t *sram8k, int sram_kb) {
    msx.rom = rom;
    msx.rom_size = size;
    msx.rom_real = real_size;
    msx.mapper = m;
    msx.sram = sram8k;
    msx.sram_size = sram8k ? (uint8_t)(sram_kb == 2 ? 2 : 8) : 0;
    msx.base = m == MAP_PLAIN ? msx_plain_base(rom, real_size) : 0;
}

void msx_reset(void) {
    const uint8_t *rom = msx.rom;
    const uint32_t size = msx.rom_size, real = msx.rom_real, base = msx.base;
    const msx_mapper_t m = msx.mapper;
    uint8_t *sram = msx.sram;
    const uint8_t sram_size = msx.sram_size;
    const bool mouse_in = msx.mouse_in;
    memset(&msx, 0, sizeof msx);
    msx.rom = rom;
    msx.rom_size = size;
    msx.rom_real = real;
    msx.base = base;
    msx.mapper = m;
    msx.sram = sram;
    msx.sram_size = sram_size;
    msx.mouse_in = mouse_in;
    tms_reset(&msx.vdp);
    memset(msx.keys, 0xFF, sizeof msx.keys);
    msx.psg[7] = 0xBF;
    msx.psg[15] = 0x0F;
    memset(ram, 0xFF, sizeof ram);
    if (m == MAP_KONAMI || m == MAP_KONAMI_SCC || m == MAP_GENERIC8) {
        for (int i = 0; i < 4; i++) msx.bank[i] = (uint8_t)i;
    } else if (m == MAP_RTYPE) {
        msx.bank[0] = 0x17;
        msx.bank[1] = 0;
    } else if (m == MAP_GENERIC16) {
        msx.bank[0] = 0;
        msx.bank[1] = 1;
    }
    map_all();
    cpu_reset();
}

void msx_frame(void) {
    tms_vblank(&msx.vdp);
    cpu_irq(tms_irq(&msx.vdp));
    cpu_exec(MSX_FRAME_T);
    msx.frames++;
}

void msx_key(int row, int bit, bool down) {
    if (row < 0 || row > 15) return;
    if (down) msx.keys[row] &= (uint8_t)~(1 << bit);
    else msx.keys[row] |= (uint8_t)(1 << bit);
}

void msx_keys_up(void) { memset(msx.keys, 0xFF, sizeof msx.keys); }
