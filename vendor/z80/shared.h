// shared.h -- ours, not upstream: z80.c includes SMS Plus's shared.h for
// its types and in_ram(); this stands in for it so z80.c stays unchanged.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef in_ram
#ifdef PICO_BOARD
#include <pico.h>
#define in_ram __not_in_flash_func      // the core runs from SRAM on the cart
#else
#define in_ram
#endif
#endif
