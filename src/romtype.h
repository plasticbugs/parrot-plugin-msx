// romtype.h -- which mapper a cartridge ROM uses, and whether it has SRAM.
//
// In order: a tag in the file's name ("[KONAMISCC]", see romtype.c), then a
// short table of ROMs the guess gets wrong (by SHA-1), then a guess from
// the ROM's bytes, as openMSX guesses for a ROM it doesn't know: count the
// LD (nnnn),A instructions that hit each mapper's bank registers.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "msx.h"

typedef struct {
    msx_mapper_t mapper;
    int sram_kb;                // 0, 2 (ASCII16) or 8 (ASCII8)
    const char *why;            // "TAG", "LIST" or "GUESS"
} romtype_t;

// a tag in the name, e.g. "Game [ASCII8].mx1": true and *m set if there is one
bool romtype_tag(const char *name, msx_mapper_t *m);

// the guess from the ROM's bytes alone
msx_mapper_t romtype_guess(const uint8_t *rom, uint32_t size);

// all three: name may be NULL, sha1 the ROM's (20 bytes) or NULL
romtype_t romtype_find(const char *name, const uint8_t sha1[20], const uint8_t *rom, uint32_t size);
