; mapper.asm -- a 128K cartridge for one MSX mapper: every bank in every
; window it switches, checked from RAM, then a line a window on screen 1
; ('.' right, 'X' wrong) and PASS or FAIL. The Konami SCC one then checks
; the SCC's wave RAM and leaves its first voice playing middle C.
;   sjasmplus -DMAPPER=n --raw=OUT.rom mapper.asm
;   1 ASCII8, 2 ASCII16, 3 Konami, 4 Konami SCC
; Each 8K page ends with 'P' and its number.
        include "common.inc"
LINL32  equ 0xF3AF
res     equ 0xE000              ; a byte a bank, a row of 16 a window
PAGES   equ 16

        if MAPPER == 2
BANKS   equ 8                   ; 16K banks
        else
BANKS   equ 16
        endif

        header
        include "lib.inc"
start:  di
        call page2
        xor a
        ld (CLIKSW), a
        ld a, 32
        ld (LINL32), a
        call INIT32
        di
        ld hl, ramcode
        ld de, 0xC000
        ld bc, ramcode_end - ramcode
        ldir
        call test
        ei
        at 0, 0
        ld hl, t_title
        call print
        ld ix, res
        ld c, 0                 ; window
.row:   ld a, c
        add a, 2
        ld l, a
        ld h, 1
        push bc
        call POSIT
        pop bc
        ld a, c
        add a, '0'
        call CHPUT
        ld a, ' '
        call CHPUT
        ld b, BANKS
.col:   ld a, (ix)
        inc ix
        call CHPUT
        djnz .col
        inc c
        ld a, c
        cp 4
        jr nz, .row
        at 0, 7
        ld hl, res
        ld bc, 4 * 16
        ld a, 'X'
        cpir
        ld hl, t_pass
        jr nz, .p
        ld hl, t_fail
.p:     call print
        if MAPPER == 4
        call scc
        endif
.end:   ei
        halt
        jr .end

        if MAPPER == 4
; the SCC: enabled by bank 0x3F at 0x9000; a square wave in voice 1's RAM,
; read back; then voice 1 at middle C, full volume
scc:    ld a, 0x3F
        ld (0x9000), a
        ld hl, 0x9800
        ld b, 32
.w:     ld a, b
        cp 17
        sbc a, a
        xor 0x80                ; 0x7F for the first 16, 0x80 after
        ld (hl), a
        inc hl
        djnz .w
        ld hl, 0x9800
        ld b, 32
        ld c, 0
.r:     ld a, b
        cp 17
        sbc a, a
        xor 0x80
        cp (hl)
        jr z, .ok
        inc c
.ok:    inc hl
        djnz .r
        at 0, 9
        ld hl, t_sccok
        ld a, c
        or a
        jr z, .t
        ld hl, t_sccbad
.t:     call print
        ld hl, 427              ; 3579545 / (32 * (427 + 1)) = 261.4 Hz
        ld (0x9880), hl
        ld a, 15
        ld (0x988A), a
        ld a, 1
        ld (0x988F), a
        ret
        endif

t_title:
        if MAPPER == 1
        db "ASCII8: 4 WINDOWS, 16 BANKS", 0
        elseif MAPPER == 2
        db "ASCII16: 2 WINDOWS, 8 BANKS", 0
        elseif MAPPER == 3
        db "KONAMI: 3 WINDOWS, 16 BANKS", 0
        else
        db "KONAMI SCC: 4 WINDOWS, 16 BANKS", 0
        endif
t_pass: db "PASS", 0
t_fail: db "FAIL", 0
t_sccok: db "SCC WAVE RAM OK, MIDDLE C", 0
t_sccbad: db "SCC WAVE RAM WRONG", 0

; what runs from RAM, at 0xC000: the windows change under it
ramcode:
        disp 0xC000
try:    macro reg, win
        ld b, 0
.bank:  ld a, b
        ld (reg), a
        ld hl, win + 0x1FFE
        if MAPPER == 2
        add a, a
        endif
        call check
        if MAPPER == 2
        ld hl, win + 0x3FFE
        ld a, b
        add a, a
        inc a
        call check2
        endif
        inc b
        ld a, b
        cp BANKS
        jr nz, .bank
        ld de, 16 - BANKS
        add ix, de
        endm

test:   ld hl, res              ; all '-' (a window not tested)
        ld de, res + 1
        ld bc, 4 * 16 - 1
        ld (hl), '-'
        ldir
        ld ix, res
; each bank into the window: its register, its address
        if MAPPER == 1
        try 0x6000, 0x4000
        try 0x6800, 0x6000
        try 0x7000, 0x8000
        try 0x7800, 0xA000
        xor a                   ; as it started
        ld (0x6000), a
        inc a
        ld (0x6800), a
        inc a
        ld (0x7000), a
        inc a
        ld (0x7800), a
        elseif MAPPER == 2
        try 0x6000, 0x4000
        try 0x77FF, 0x8000
        xor a
        ld (0x6000), a
        inc a
        ld (0x77FF), a
        elseif MAPPER == 3
        ld ix, res + 16         ; 0x4000-0x5FFF is fixed
        try 0x6000, 0x6000
        try 0x8000, 0x8000
        try 0xA000, 0xA000
        ld a, 1
        ld (0x6000), a
        inc a
        ld (0x8000), a
        inc a
        ld (0xA000), a
        else
        try 0x5000, 0x4000
        try 0x7000, 0x6000
        try 0x9000, 0x8000
        try 0xB000, 0xA000
        xor a
        ld (0x5000), a
        inc a
        ld (0x7000), a
        inc a
        ld (0x9000), a
        inc a
        ld (0xB000), a
        endif
        ret


; HL: a page's end, A: the page it should be: '.' or 'X' at IX, IX on
check:  ld (ix), '.'
        inc ix
check2: ld c, a
        ld a, (hl)
        cp 'P'
        jr nz, .bad
        inc hl
        ld a, (hl)
        cp c
        ret z
.bad:   ld (ix - 1), 'X'
        ret
        ent
ramcode_end:

        ds 0x6000 - 2 - $, 0xFF
        db 'P', 0
        dup PAGES - 1, k
        org 0x6000              ; fileorg-ok (the raw file goes on: page k + 1)
        ds 0x1FFE, 0xFF
        db 'P', k + 1
        edup
