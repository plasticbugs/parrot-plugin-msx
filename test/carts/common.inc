; common.inc -- what the test carts share: the BIOS calls and the header.
; lib.inc has the routines (after the header)

DISSCR  equ 0x0041
ENASCR  equ 0x0044
WRTVDP  equ 0x0047      ; B to register C
WRTVRM  equ 0x004D      ; A to VRAM HL
FILVRM  equ 0x0056      ; A to BC bytes of VRAM from HL
LDIRVM  equ 0x005C      ; BC bytes from HL to VRAM DE
INITXT  equ 0x006C
INIT32  equ 0x006F
INIGRP  equ 0x0072
INIMLT  equ 0x0075
WRTPSG  equ 0x0093      ; E to PSG register A
RDPSG   equ 0x0096
CHPUT   equ 0x00A2
POSIT   equ 0x00C6      ; H column, L row, from 1
SNSMAT  equ 0x0141      ; keyboard row A -> A

STATFL  equ 0xF3E7      ; the VDP status, as the BIOS read it at the last interrupt
JIFFY   equ 0xFC9E      ; frames
FORCLR  equ 0xF3E9
BAKCLR  equ 0xF3EA
BDRCLR  equ 0xF3EB
CLIKSW  equ 0xF3DB

; the cartridge header; the code starts at start
header: macro
        org 0x4000
        db "AB"
        dw start
        ds 12, 0
        endm

; at (col, row), from 0
at:     macro col, row
        ld hl, (col + 1) << 8 | (row + 1)
        call POSIT
        endm

