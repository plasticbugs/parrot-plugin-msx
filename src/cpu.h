// cpu.h -- the Z80 (cpu.c, on vendor/z80). It reads the machine's memory
// through cpu_readmap[] and writes it through msx_wpage[] (msx.h), and
// calls msx_in(), msx_out() and msx_write_slow().
#pragma once
#include <stdbool.h>
#include <stdint.h>

void cpu_reset(void);
// run about n cycles (to the end of the instruction that passes them)
void cpu_exec(int n);
// the INT line, as the VDP holds it
void cpu_irq(bool on);
// cycles run so far
uint32_t cpu_cycles(void);
uint16_t cpu_pc(void);
