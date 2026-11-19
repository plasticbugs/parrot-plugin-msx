// romlist.h -- made by tools/romlist.py; don't edit. The ROMs whose mapper
// romtype.c's guess can't find, by the first 4 bytes of their SHA-1. From
// openMSX's software database (softwaredb.xml: Nicolas Beyaert, the blueMSX
// team, the openMSX team).
#pragma once
#include <stdint.h>

typedef struct {
    uint8_t sha1[4];
    uint8_t mapper, sram_kb;   // msx_mapper_t, K of SRAM
} romlist_t;

static const romlist_t ROMLIST[] = {
    {{0x32, 0x54, 0xD7, 0xA5}, 1, 0},   // Batman - Rescue The Robin (ASCII8)
    {{0x37, 0x39, 0xD8, 0x41}, 1, 0},   // Batman - Rescue The Robin (ASCII8)
    {{0x70, 0x9F, 0xB3, 0x53}, 1, 0},   // Batman - Rescue The Robin (ASCII8)
    {{0xC5, 0x22, 0xD4, 0x06}, 1, 0},   // Batman - Rescue The Robin (ASCII8)
    {{0x48, 0x2C, 0xD6, 0x50}, 1, 0},   // Bomber King (ASCII8)
    {{0x98, 0x65, 0xF0, 0xBC}, 6, 0},   // Confused ? (GenericKonami)
    {{0xDB, 0x3B, 0xF1, 0x58}, 6, 0},   // Confused ? (GenericKonami)
    {{0xE6, 0x41, 0x95, 0x19}, 1, 2},   // Deep Dungeon 1 (ASCII8SRAM2)
    {{0x2A, 0x20, 0x9A, 0x0E}, 1, 8},   // Deep Dungeon 2 (ASCII8SRAM8)
    {{0xD8, 0x21, 0x35, 0xA5}, 1, 2},   // Deep Dungeon 2 (ASCII8SRAM2)
    {{0xD5, 0xB1, 0x64, 0x79}, 6, 0},   // Demonia I (GenericKonami)
    {{0x6C, 0x18, 0x14, 0xC7}, 1, 8},   // Dragon Slayer 2 - Xanadu (ASCII8SRAM8)
    {{0x88, 0xCD, 0x90, 0x59}, 1, 8},   // Dragon Slayer 2 - Xanadu (ASCII8SRAM8)
    {{0xA3, 0xDE, 0x07, 0x61}, 2, 2},   // Harry Fox - MSX Special (ASCII16SRAM2)
    {{0x0F, 0xE0, 0x7B, 0x5F}, 2, 2},   // Hydlide 2 - Shine Of Darkness (ASCII16SRAM2)
    {{0x1E, 0xAE, 0x08, 0x24}, 2, 2},   // Hydlide 2 - Shine Of Darkness (ASCII16SRAM2)
    {{0xA3, 0x53, 0x79, 0x34}, 2, 2},   // Hydlide 2 - Shine Of Darkness (ASCII16SRAM2)
    {{0xB1, 0x8D, 0x36, 0xCC}, 2, 2},   // Hydlide 2 - Shine Of Darkness (ASCII16SRAM2)
    {{0xDA, 0x4B, 0x44, 0xC7}, 2, 2},   // Hydlide 2 - Shine Of Darkness (ASCII16SRAM2)
    {{0x12, 0x2F, 0x65, 0x92}, 2, 0},   // King's Knight (ASCII16)
    {{0x46, 0x06, 0x2D, 0x33}, 2, 0},   // King's Knight (ASCII16)
    {{0xA2, 0xCA, 0x7E, 0x6E}, 2, 0},   // King's Knight (ASCII16)
    {{0xF0, 0x22, 0x81, 0xAA}, 1, 8},   // Konami's Game Master 2 (ASCII8SRAM8)
    {{0x47, 0x3B, 0xC0, 0xEA}, 1, 0},   // MSX Eiwa Jiten (ASCII8)
    {{0x0B, 0x37, 0x96, 0x10}, 5, 0},   // R-Type (R-Type)
    {{0x1A, 0xF1, 0x83, 0x69}, 5, 0},   // R-Type (R-Type)
    {{0x37, 0xBD, 0x46, 0x80}, 5, 0},   // R-Type (R-Type)
    {{0x5E, 0xFA, 0xE0, 0xC1}, 5, 0},   // R-Type (R-Type)
    {{0x96, 0xC8, 0x4B, 0xFC}, 5, 0},   // R-Type (R-Type)
    {{0x9B, 0x6C, 0xB2, 0x24}, 5, 0},   // R-Type (R-Type)
    {{0x9C, 0x88, 0x6F, 0xFF}, 5, 0},   // R-Type (R-Type)
    {{0x16, 0x35, 0x1E, 0x6A}, 1, 8},   // Shigun (ASCII8SRAM8)
    {{0x1D, 0x12, 0x9B, 0xDE}, 3, 0},   // Spy Vs Spy II (Konami)
    {{0x25, 0x88, 0xB9, 0xAD}, 2, 0},   // The Black Onyx 2 (ASCII16)
};
#define ROMLIST_N 34
