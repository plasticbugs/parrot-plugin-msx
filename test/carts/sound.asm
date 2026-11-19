; sound.asm -- the AY-3-8910 in steps of half a second, the step's number on
; screen 1; after step 7 it's quiet:
;   1  A: tone 428 (261 Hz), level 15
;   2  + B: tone 214, level 12
;   3  + C: tone 4095 (27 Hz, under the SN76489's lowest), level 8
;   4  C: noise 16 in place of its tone
;   5  A alone on the envelope: triangle (14), period 1024, slow
;   6  A: no tone, the envelope fast: sawtooth (8), period 16, a buzz
;   7  all levels 0
; sjasmplus --raw=sound.rom sound.asm (32K)
        include "common.inc"
LINL32  equ 0xF3AF

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
        ld ix, steps
        ld c, '1'
.step:  ld a, (ix)
        cp 0xFF
        jr z, .end
.reg:   ld a, (ix)              ; register, value pairs to 0xFE
        cp 0xFE
        jr z, .done
        ld e, (ix + 1)
        call WRTPSG
        inc ix
        inc ix
        jr .reg
.done:  inc ix
        push bc
        at 0, 2
        pop bc
        ld a, c
        call CHPUT
        inc c
        ld b, 30
        call frames
        jr .step
.end:   at 0, 4
        ld hl, t_end
        call print
.halt:  ei
        halt
        jr .halt

steps:  db 0, 0xAC, 1, 0x01, 8, 15, 7, 0xBE, 0xFE
        db 2, 0xD6, 3, 0x00, 9, 12, 7, 0xBC, 0xFE
        db 4, 0xFF, 5, 0x0F, 10, 8, 7, 0xB8, 0xFE
        db 6, 16, 7, 0x9C, 0xFE
        db 9, 0, 10, 0, 7, 0xBE, 11, 0x00, 12, 0x04, 8, 0x10, 13, 14, 0xFE
        db 7, 0xBF, 11, 16, 12, 0, 13, 8, 0xFE
        db 8, 0, 9, 0, 10, 0, 0xFE
        db 0xFF
t_title: db "SOUND: SEVEN STEPS", 0
t_end:  db "QUIET", 0
        ds 0x4000 + 0x8000 - $, 0xFF
