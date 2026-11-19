// msx.h -- an MSX1 for cartridges: the BIOS, a cartridge, 64K of RAM, the
// VDP (tms.h), the PSG's registers, the PPI and its keyboard matrix, two
// joystick ports and a mouse. No BASIC, disks or tapes.
//
//   slot 0  the BIOS: C-BIOS's main ROM at $0000 and logo ROM at $8000, or
//           the player's own 32K BIOS
//   slot 1  the cartridge, with its mapper
//   slot 2  empty
//   slot 3  64K of RAM
//
// The Z80 is whichever core cpu.h is built with. The PSG makes no sound
// here: aysn.c reads its registers once a frame and turns them into the
// console's SN76489 bytes.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "tms.h"

#define MSX_LINE_T  228                  // Z80 cycles a TV line
#define MSX_FRAME_T (262 * MSX_LINE_T)   // and a TV frame (60 Hz)

typedef enum {
    MAP_PLAIN,          // 8K to 64K, at the address its header asks for
    MAP_ASCII8,
    MAP_ASCII16,
    MAP_KONAMI,         // Konami without SCC ("Konami4")
    MAP_KONAMI_SCC,     // Konami with SCC ("Konami5")
    MAP_RTYPE,          // Irem's R-Type
    MAP_GENERIC8,       // 8K banks, each switched by a write inside it
    MAP_GENERIC16,      // the same with 16K banks
    MAP_N
} msx_mapper_t;

extern const char *const msx_mapper_name[MAP_N];

// the joystick ports' bits, low = pressed (as the PSG's register 14 reads them)
enum { JOY_UP = 1, JOY_DOWN = 2, JOY_LEFT = 4, JOY_RIGHT = 8, JOY_A = 16, JOY_B = 32 };

typedef struct {
    // what the core reads and writes
    tms_t vdp;
    uint8_t psg[16];
    uint8_t psg_addr;
    uint32_t env_restarts;       // writes to R13 (each restarts the envelope)
    uint8_t keys[16];            // the keyboard matrix, rows 0-10, low = pressed
    uint8_t joy[2];              // JOY_* held (high = pressed, unlike the port)
    uint8_t slots;               // the PPI's port A
    uint8_t ppi_c;
    // the mouse: on port 1, while the game toggles its pin 8 (and one is plugged in)
    bool mouse_in;
    int32_t mouse_dx, mouse_dy;  // moved since the game last latched it (MSX units, x right, y down)
    uint8_t mouse_buttons;       // bit 0 left, 1 right
    uint8_t mouse_phase;
    int8_t mouse_x, mouse_y;     // latched for the game's four reads
    uint32_t mouse_t;            // the Z80's cycle count at the last pin 8 change
    uint32_t mouse_frame;        // the frame of the last pin 8 change
    // the cartridge
    msx_mapper_t mapper;
    const uint8_t *rom;
    uint32_t rom_size;           // padded to a power of 2 (the 8K page count, for banks)
    uint32_t rom_real;           // its size as read
    uint32_t base;               // MAP_PLAIN: where its first byte goes
    uint8_t bank[4];             // the banks at $4000, $6000, $8000, $A000 (16K mappers: [0], [1])
    uint8_t *sram;               // SRAM types: 8K (a smaller SRAM is mirrored in it), or NULL
    uint8_t sram_size;           // in K
    uint8_t sram_on;             // bit per 16K region (ASCII16) or 8K bank (ASCII8) showing the SRAM
    uint32_t sram_writes;        // writes to the SRAM so far (for the .sav)
    bool scc_on;
    uint8_t scc[256];            // the SCC's registers as written ($9800-$98FF)
    uint32_t scc_writes;
    // counts
    uint32_t frames;
    uint32_t bank_writes;
} msx_t;

extern msx_t msx;

// the pages the Z80 reads and writes directly, 8 of 8K: cpu_readmap is the
// core's own table (vendor/z80/z80.c); the ROM pages in it are read only
extern unsigned char *cpu_readmap[8];
extern uint8_t *msx_wpage[8];   // NULL: a write the machine handles (msx_write_slow)

void msx_init(const uint8_t *bios, uint32_t bios_size, const uint8_t *logo, uint32_t logo_size);
// rom: the cartridge, padded with $FF to size (a power of 2, at least 8K).
// sram8k: an 8K buffer for a cartridge with SRAM (sram_kb of it, 2 or 8,
// mirrored through the 8K), loaded by the caller, or NULL.
void msx_cart(const uint8_t *rom, uint32_t size, uint32_t real_size, msx_mapper_t m, uint8_t *sram8k, int sram_kb);
void msx_reset(void);
// one TV frame: the vertical blank's start, then a frame of Z80 time
void msx_frame(void);

// the machine's side of the core (cpu.h's cores call these)
void msx_write_slow(uint16_t a, uint8_t v);
uint8_t msx_in(uint8_t port);
void msx_out(uint8_t port, uint8_t v);

// the input, set before each frame
void msx_key(int row, int bit, bool down);
void msx_keys_up(void);

// where an 8K plain ROM's header says it starts (for MAP_PLAIN)
uint32_t msx_plain_base(const uint8_t *rom, uint32_t size);
