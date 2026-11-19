// mirror.h -- the MSX's picture on the console: its VRAM and registers
// copied to the console's own TMS9918A, one frame kernel list a frame.
//
// The console's chip is the MSX's chip, so nothing is drawn here: what the
// MSX program wrote to its VRAM goes to the console's VRAM. A list has
// about 1,800 bytes of VRAM writes in it at most (gbkernel.asm's costs), and
// the MSX can write all 16K in a frame, so this keeps a copy of what the
// console holds, sends what differs, most visible first, and leaves the
// rest for the next frames:
//
//   1  the registers that changed (R1 always with the console's frame
//      interrupt on and 16K set), in the vertical blank
//   2  the sprite attributes, then the name table, while the vertical
//      blank lasts (so sprites and tiles move with the TV frame)
//   3  the pattern and colour tables and the sprite patterns the
//      registers point at, during the picture
//   4  everything else in VRAM, a little each frame
//
// A table base register (R2-R6) that moves waits until the table where it
// points has arrived, a few frames at most. And when a game has had its
// screen off (as most do while they fill VRAM) and turns it back on
// before the console has caught up, the console's screen stays off until
// it has, at most HOLD_FRAMES frames.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tms.h"

#define MIRROR_LIST_MAX 4096    // bytes; a list never comes near it
#define HOLD_FRAMES     20      // the longest the console's screen stays off to catch up

typedef struct {
    uint8_t vram[0x4000];       // the console's VRAM once the lists so far have run
    uint8_t reg[8];             // its registers likewise
    uint32_t pending[8];        // 64-byte blocks that may differ from the MSX's
    uint32_t cursor;            // where the scan of the rest goes on from
    bool was_off;               // the MSX's screen was off and the console hasn't caught up since
    uint8_t held;               // frames the console's screen has been kept off for that
    uint8_t reg_wait[8];        // frames a base register has waited for its table
    // for the note and the tests
    uint32_t bytes;             // VRAM bytes in the last list
    uint32_t cost;              // its cost in Z80 cycles
    uint32_t backlog;           // bytes that still differ after it
    uint32_t max_cost, max_backlog;
    uint32_t frames_behind;     // frames that ended with a backlog
} mirror_t;

// the console as the frame kernel leaves it after its start
void mirror_reset(mirror_t *m);

// The list for the console's next frame: the PSG bytes, then the registers
// and VRAM writes that bring the console nearer to t. Returns its length.
size_t mirror_frame(mirror_t *m, tms_t *t, const uint8_t *psg, int npsg, uint8_t list[MIRROR_LIST_MAX]);
