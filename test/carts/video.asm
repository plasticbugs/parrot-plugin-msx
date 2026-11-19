; video.asm -- the TMS9918A's modes one after another, 2 seconds each:
;   1  screen 1 (graphics I): text, colours, eight 16x16 sprites, six of
;      them on one line (the fifth-sprite flag) and two overlapping (the
;      collision flag); the status the BIOS read, shown each frame
;   2  screen 2 (graphics II): every pattern and colour byte different,
;      the sprites again
;   3  screen 3 (multicolour): every block's colours
;   4  screen 0 (text): 40 columns
;   5  screen 1 again with the status bits seen in step 1, and DONE
; sjasmplus --raw=video.rom video.asm (32K)
        include "common.inc"
LINL40  equ 0xF3AE
LINL32  equ 0xF3AF
TXTNAM  equ 0xF3B3
T32NAM  equ 0xF3BD
T32COL  equ 0xF3BF
T32ATR  equ 0xF3C3
T32PAT  equ 0xF3C5
GRPCOL  equ 0xF3C9
GRPCGP  equ 0xF3CB
GRPATR  equ 0xF3CD
GRPPAT  equ 0xF3CF
MLTCGP  equ 0xF3D5
MLTATR  equ 0xF3D7
MLTPAT  equ 0xF3D9
RG1SAV  equ 0xF3E0
seen    equ 0xE000
last    equ 0xE001

        header
        include "lib.inc"
start:  xor a
        ld (CLIKSW), a
        ld (seen), a
        ; 1: screen 1
        ld a, 32
        ld (LINL32), a
        ld a, 15
        ld (FORCLR), a
        ld a, 4
        ld (BAKCLR), a
        ld (BDRCLR), a
        call INIT32
        at 0, 0
        ld hl, t_s1
        call print
        at 0, 2
        ld hl, t_chars
        call print
        ld hl, colours          ; characters 64-127 in colours
        ld de, (T32COL)
        ld a, e
        add a, 8
        ld e, a
        ld bc, 8
        call LDIRVM
        ld hl, (T32ATR)
        ld de, (T32PAT)
        call sprites
        ld b, 120
.s1:    ei
        halt
        ld a, (STATFL)
        ld (last), a
        ld c, a
        ld a, (seen)
        or c
        ld (seen), a
        push bc
        at 0, 22
        ld hl, t_status
        call print
        ld a, (last)
        call hex
        ld hl, t_seen
        call print
        ld a, (seen)
        call hex
        pop bc
        djnz .s1

        ; 2: screen 2
        call INIGRP
        ld hl, (GRPCGP)
        call setwrt
        ld de, 0
.p2:    ld a, e                 ; pattern byte: its address's low byte xor its high byte
        xor d
        out (0x98), a
        inc de
        ld a, d
        cp 0x18
        jr nz, .p2
        ld hl, (GRPCOL)
        call setwrt
        ld de, 0
.c2:    ld a, d                 ; colour: a foreground for each character row, on black
        add a, a
        add a, a
        add a, a
        add a, a
        ld c, a
        ld a, e
        rlca
        rlca
        rlca
        and 0x07
        or c
        or 0x20
        and 0xF7
        out (0x98), a
        inc de
        ld a, d
        cp 0x18
        jr nz, .c2
        ld hl, (GRPATR)
        ld de, (GRPPAT)
        call sprites
        ld b, 120
        call frames

        ; 3: screen 3
        call INIMLT
        ld hl, (MLTCGP)
        call setwrt
        ld de, 0
.m3:    ld a, e                 ; two colours a byte
        add a, d
        rlca
        xor e
        out (0x98), a
        inc de
        ld a, d
        cp 0x08
        jr nz, .m3
        ld hl, (MLTATR)
        ld de, (MLTPAT)
        call sprites
        ld b, 120
        call frames

        ; 4: screen 0
        ld a, 40
        ld (LINL40), a
        ld a, 15
        ld (FORCLR), a
        ld a, 1
        ld (BAKCLR), a
        call INITXT
        at 0, 0
        ld hl, t_s0
        call print
        at 0, 2
        ld hl, t_chars
        call print
        at 0, 23
        ld hl, t_bottom
        call print
        ld b, 120
        call frames

        ; 5: screen 1 and what was seen
        ld a, 4
        ld (BAKCLR), a
        call INIT32
        at 0, 0
        ld hl, t_done
        call print
        at 0, 2
        ld hl, t_status
        call print
        ld a, (last)
        call hex
        ld hl, t_seen
        call print
        ld a, (seen)
        call hex
.end:   ei
        halt
        jr .end

; the VDP's write address HL
setwrt: di
        ld a, l
        out (0x99), a
        ld a, h
        and 0x3F
        or 0x40
        out (0x99), a
        ei
        ret

; 16x16 sprites: patterns 0 and 4 to DE, eight sprites to the attribute table at HL
sprites:
        push hl
        ld hl, spat
        ld bc, 64
        call LDIRVM
        pop de
        ld hl, sat
        ld bc, sat_end - sat
        call LDIRVM
        ld a, (RG1SAV)
        or 0x02
        ld b, a
        ld c, 1
        jp WRTVDP

colours: db 0x21, 0x31, 0x61, 0x81, 0xA1, 0xD1, 0xE1, 0xF6
spat:   ; a disc, then a ring
        db 0x07, 0x1F, 0x3F, 0x7F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F, 0x3F, 0x1F, 0x07
        db 0xE0, 0xF8, 0xFC, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFE, 0xFC, 0xF8, 0xE0
        db 0x07, 0x18, 0x20, 0x40, 0x40, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x40, 0x40, 0x20, 0x18, 0x07
        db 0xE0, 0x18, 0x04, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x04, 0x18, 0xE0
sat:    db 95, 16, 0, 2         ; six on one line
        db 95, 52, 0, 3
        db 95, 88, 4, 6
        db 95, 124, 0, 7
        db 95, 160, 4, 8        ; the fifth: sprite 4
        db 95, 196, 0, 10
        db 40, 100, 0, 11       ; two that overlap
        db 46, 108, 4, 13
        db 0xD0
sat_end:
t_s1:   db "SCREEN 1: GRAPHICS I", 0
t_s0:   db "SCREEN 0: TEXT, 40 COLUMNS, 24 ROWS", 0
t_chars: db " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~", 0
t_bottom: db "THE BOTTOM ROW, AND ITS LAST COLUMN ->|", 0
t_status: db "STATUS ", 0
t_seen: db " SEEN ", 0
t_done: db "DONE", 0
        ds 0x4000 + 0x8000 - $, 0xFF
