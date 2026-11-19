; timing.asm -- how long things take, against the frame: after a HALT, a
; piece of work, then a counting loop until the next interrupt. The counts
; on screen 1, in hex, are the time left in the frame, so they show the
; Z80's timing (its M1 wait states included) to a loop pass:
;   PLAIN  nothing: the counting loop alone
;   LDIR   4096 bytes copied
;   BC     a DEC BC / LD A,B / OR C / JR NZ delay loop, 1000 times
;   DJNZ   DJNZ $ from 0, 8 times
;   OTIR   256 bytes out to the PSG's address port
; sjasmplus --raw=timing.rom timing.asm (32K)
        include "common.inc"
LINL32  equ 0xF3AF
counts  equ 0xE000

        header
        include "lib.inc"
start:  xor a
        ld (CLIKSW), a
        ld a, 32
        ld (LINL32), a
        call INIT32
        at 0, 0
        ld hl, t_title
        call print
        ld ix, counts
        ei
        halt                    ; plain
        call count
        halt                    ; LDIR
        ld hl, 0x0000
        ld de, 0xD000
        ld bc, 4096
        ldir
        call count
        halt                    ; the BC loop
        ld bc, 1000
.bc:    dec bc
        ld a, b
        or c
        jr nz, .bc
        call count
        halt                    ; DJNZ $
        ld c, 8
.dj:    ld b, 0
        djnz $
        dec c
        jr nz, .dj
        call count
        halt                    ; OTIR to port 0xA0, register 0x7F-0 (none)
        ld hl, 0x0000
        ld bc, 0x00A0
        otir
        call count
        ; the counts
        ld ix, counts
        ld hl, names
        ld c, 0
.show:  push hl
        ld a, c
        add a, 2
        ld l, a
        ld h, 1
        push bc
        call POSIT
        pop bc
        pop hl
        call print
        inc hl
        ld a, (ix + 1)
        call hex
        ld a, (ix)
        call hex
        inc ix
        inc ix
        inc c
        ld a, c
        cp 5
        jr nz, .show
.end:   ei
        halt
        jr .end

; DE passes of a loop until the frame counter changes, to (IX), IX on 2
count:  ld a, (JIFFY)
        ld b, a
        ld de, 0
.c:     inc de
        ld a, (JIFFY)
        cp b
        jr z, .c
        ld (ix), e
        ld (ix + 1), d
        inc ix
        inc ix
        ret

t_title: db "TIMING: PASSES LEFT IN A FRAME", 0
names:  db "PLAIN ", 0
        db "LDIR  ", 0
        db "BC    ", 0
        db "DJNZ  ", 0
        db "OTIR  ", 0
        ds 0x4000 + 0x8000 - $, 0xFF
