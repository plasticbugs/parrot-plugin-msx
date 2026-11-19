; lib.inc -- routines the test carts share: text out through the BIOS
; (screen 0 or 1), hex, waiting, the slot

; the text at HL, to a 0
print:  ld a, (hl)
        or a
        ret z
        call CHPUT
        inc hl
        jr print

; A in hex
hex:    push af
        rrca
        rrca
        rrca
        rrca
        call .digit
        pop af
.digit: and 15
        add a, '0'
        cp '9' + 1
        jr c, .out
        add a, 'A' - '9' - 1
.out:   jp CHPUT

; wait for B frames
frames: ei
.f:     halt
        djnz .f
        ret

; the cartridge's slot in 0x8000-0xBFFF too (its init runs with it at 0x4000 only)
page2:  in a, (0xA8)
        and 0x0C
        add a, a
        add a, a
        ld b, a
        in a, (0xA8)
        and 0xCF
        or b
        out (0xA8), a
        ret
