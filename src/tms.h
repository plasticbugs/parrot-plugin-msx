// tms.h -- the MSX's video chip, a TMS9918A, as the MSX program sees it.
//
// It doesn't draw. The console has the same chip, so the picture is made
// there: mirror.c copies this chip's video RAM and registers to it. What
// this side has to get right is what the MSX program can see: the two
// ports, the address and read-ahead, the registers, and the status byte
// (vertical blank, the fifth sprite on a line, and two sprites touching),
// which the console's own chip can't report back to the cart.
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t vram[0x4000];
    uint8_t reg[8];
    uint8_t status;             // F (bit 7), 5S (6), C (5), the fifth sprite's number (0-4)
    uint8_t latch;              // the first byte of a control port pair
    bool second;                // the next control byte is the second of a pair
    uint16_t addr;
    uint8_t ahead;              // the read-ahead buffer
    uint32_t dirty[8];          // 64-byte blocks of video RAM written since tms_take_dirty()
    uint32_t reg_writes;        // register writes so far
    bool blanked;               // R1 written with the screen off (mirror.c clears it)
} tms_t;

void tms_reset(tms_t *t);

void tms_write_data(tms_t *t, uint8_t v);
uint8_t tms_read_data(tms_t *t);
void tms_write_ctrl(tms_t *t, uint8_t v);
uint8_t tms_read_status(tms_t *t);

// the INT line: F set and interrupts on (R1 bit 5)
static inline bool tms_irq(const tms_t *t) { return (t->status & 0x80) && (t->reg[1] & 0x20); }

// the end of the picture: F set, and the sprites looked at for 5S and C as
// the chip would have found them while it drew the frame
void tms_vblank(tms_t *t);

// the blocks written since the last call, ORed into out[8]
void tms_take_dirty(tms_t *t, uint32_t out[8]);
