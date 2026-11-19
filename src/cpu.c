// cpu.c -- cpu.h on Juergen Buchmueller's Z80 (vendor/z80, from MAME by
// way of SMS Plus). The core reads memory through its own cpu_readmap[]
// (msx.c fills it in) and calls back here for writes and ports.
#include "cpu.h"

#include <stddef.h>

#include "msx.h"
#ifndef LSB_FIRST
#define LSB_FIRST 1
#endif
#include "../vendor/z80/z80.h"

extern int z80_ICount;

static uint32_t done;           // cycles run before the current slice
static int debt;                // cycles the last slice ran past its end
static int slice;
static bool running;

void cpu_writemem16(int a, int v) {
    uint8_t *p = msx_wpage[(a >> 13) & 7];
    if (p) p[a & 0x1FFF] = (uint8_t)v;
    else msx_write_slow((uint16_t)a, (uint8_t)v);
}
int cpu_readport(int port) { return msx_in((uint8_t)port); }
void cpu_writeport(int port, int v) { msx_out((uint8_t)port, (uint8_t)v); }

static int irq_vector(int line) {
    (void)line;
    return 0xFF;                // the MSX's bus floats: RST 38h in IM 0 and IM 1
}

void cpu_reset(void) {
    z80_reset(NULL);
    z80_set_irq_callback(irq_vector);
    done = 0;
    debt = 0;
    running = false;
}

void cpu_irq(bool on) { z80_set_irq_line(0, on ? ASSERT_LINE : CLEAR_LINE); }

void cpu_exec(int n) {
    const int want = n + debt;
    if (want <= 0) {
        debt = want;
        return;
    }
    slice = want;
    running = true;
    const int ran = z80_execute(want);
    running = false;
    done += (uint32_t)ran;
    debt = want - ran;
}

uint32_t cpu_cycles(void) { return running ? done + (uint32_t)(slice - z80_ICount) : done; }

uint16_t cpu_pc(void) { return (uint16_t)z80_get_pc(); }
