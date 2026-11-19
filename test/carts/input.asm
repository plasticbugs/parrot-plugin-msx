; input.asm -- what the MSX sees of the keyboard, the joysticks and a mouse
; in port 1, each frame on screen 1, in hex:
;   KEYS  rows 0-10 of the keyboard matrix (a 0 bit is a key down)
;   JOY   port 1 and port 2 as register 14 reads them (bits: up down
;         left right, trigger A and B; 0 is pressed)
;   MOUSE the last X and Y offsets the mouse in port 1 gave (the MSX
;         mouse protocol on pin 8), its buttons, and the offsets' sums
; sjasmplus --raw=input.rom input.asm (32K)
        include "common.inc"
LINL32  equ 0xF3AF
mx      equ 0xE000
my      equ 0xE001
sumx    equ 0xE002
sumy    equ 0xE004
nib     equ 0xE006              ; 4 nibbles
btn     equ 0xE00A

        header
        include "lib.inc"
start:  xor a
        ld (CLIKSW), a
        ld hl, 0
        ld (sumx), hl
        ld (sumy), hl
        ld a, 32
        ld (LINL32), a
        call INIT32
        at 0, 0
        ld hl, t_title
        call print
.frame: ei
        halt
        ; the keyboard: rows 0-5 on one line, 6-10 on the next
        at 0, 2
        ld hl, t_keys
        call print
        ld c, 0
.k:     ld a, c
        cp 6
        jr nz, .k1
        push bc
        at 5, 3
        pop bc
.k1:    ld a, c
        call SNSMAT
        call hex
        ld a, ' '
        call CHPUT
        inc c
        ld a, c
        cp 11
        jr nz, .k
        ; the joysticks
        at 0, 5
        ld hl, t_joy
        call print
        ld b, 0x0F              ; port 1 (register 15 bit 6 clear)
        call joy
        call hex
        ld a, ' '
        call CHPUT
        ld b, 0x4F              ; port 2
        call joy
        call hex
        ; the mouse
        call mouse
        at 0, 7
        ld hl, t_mouse
        call print
        ld a, (mx)
        call hex
        ld a, ' '
        call CHPUT
        ld a, (my)
        call hex
        ld a, ' '
        call CHPUT
        ld a, (btn)
        call hex
        at 0, 8
        ld hl, t_sum
        call print
        ld a, (sumx + 1)
        call hex
        ld a, (sumx)
        call hex
        ld a, ' '
        call CHPUT
        ld a, (sumy + 1)
        call hex
        ld a, (sumy)
        call hex
        jp .frame

; register 14 with register 15 = B (pin 8 and the port): A
joy:    di
        ld a, 15
        out (0xA0), a
        in a, (0xA2)
        and 0xB0
        or b
        out (0xA1), a
        ld a, 14
        out (0xA0), a
        in a, (0xA2)
        ei
        ret

; the mouse in port 1: four nibbles, each after pin 8 changes
mouse:  di
        ld a, 15
        out (0xA0), a
        in a, (0xA2)
        and 0xBF                ; port 1
        ld c, a
        ld hl, nib
        ld b, 4
.n:     ld a, c
        xor 0x10                ; pin 8 the other way
        ld c, a
        ld a, 15
        out (0xA0), a
        ld a, c
        out (0xA1), a
        ld e, 40                ; about 150 us for the mouse to answer
.w:     dec e
        jr nz, .w
        ld a, 14
        out (0xA0), a
        in a, (0xA2)
        ld (btn), a
        and 0x0F
        ld (hl), a
        inc hl
        djnz .n
        ei
        ld hl, nib
        ld a, (hl)
        inc hl
        rlca
        rlca
        rlca
        rlca
        or (hl)
        inc hl
        ld (mx), a
        ld a, (hl)
        inc hl
        rlca
        rlca
        rlca
        rlca
        or (hl)
        ld (my), a
        ld a, (btn)
        and 0x30
        ld (btn), a
        ld a, (mx)              ; the sums, as signed offsets
        call sext
        ld hl, (sumx)
        add hl, de
        ld (sumx), hl
        ld a, (my)
        call sext
        ld hl, (sumy)
        add hl, de
        ld (sumy), hl
        ret
sext:   ld e, a
        rlca
        sbc a, a
        ld d, a
        ret

t_title: db "INPUT", 0
t_keys: db "KEYS ", 0
t_joy:  db "JOY  ", 0
t_mouse: db "MOUSE ", 0
t_sum:  db "SUM   ", 0
        ds 0x4000 + 0x8000 - $, 0xFF
