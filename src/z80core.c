// z80core.c -- builds vendor/z80/z80.c (Juergen Buchmueller's Z80, from
// MAME by way of SMS Plus) from here: the Mac tests and the simulator build
// what is in src/. Its warnings are its own. One change in it
// (vendor/z80/VENDORED.md): Z80_M1_WAIT, the MSX's wait state on each
// opcode fetch.
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wshift-negative-value"
#pragma clang diagnostic ignored "-Wparentheses"
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wshift-negative-value"
#pragma GCC diagnostic ignored "-Wparentheses"
#endif
#ifndef LSB_FIRST
#define LSB_FIRST 1             // the register pairs, little-endian (as SMS Plus builds it)
#endif
#ifndef Z80_M1_WAIT
#define Z80_M1_WAIT 1           // as an MSX: one wait state each M1 cycle
#endif
#include "../vendor/z80/z80.c"
