// keymap.c -- USB keys and the keypad on the MSX's keyboard matrix (keymap.h).
#include "keymap.h"

#include <string.h>

#include "msx.h"

#define K(row, bit) (uint8_t)((row) << 3 | (bit) | 0x80)
#define SHIFT K(6, 0)

// HID usage -> the MSX's row and bit (0: none)
static const uint8_t USB[256] = {
    [0x04] = K(2, 6), [0x05] = K(2, 7), [0x06] = K(3, 0), [0x07] = K(3, 1), [0x08] = K(3, 2),   // A-E
    [0x09] = K(3, 3), [0x0A] = K(3, 4), [0x0B] = K(3, 5), [0x0C] = K(3, 6), [0x0D] = K(3, 7),   // F-J
    [0x0E] = K(4, 0), [0x0F] = K(4, 1), [0x10] = K(4, 2), [0x11] = K(4, 3), [0x12] = K(4, 4),   // K-O
    [0x13] = K(4, 5), [0x14] = K(4, 6), [0x15] = K(4, 7), [0x16] = K(5, 0), [0x17] = K(5, 1),   // P-T
    [0x18] = K(5, 2), [0x19] = K(5, 3), [0x1A] = K(5, 4), [0x1B] = K(5, 5), [0x1C] = K(5, 6),   // U-Y
    [0x1D] = K(5, 7),                                                                           // Z
    [0x1E] = K(0, 1), [0x1F] = K(0, 2), [0x20] = K(0, 3), [0x21] = K(0, 4), [0x22] = K(0, 5),   // 1-5
    [0x23] = K(0, 6), [0x24] = K(0, 7), [0x25] = K(1, 0), [0x26] = K(1, 1), [0x27] = K(0, 0),   // 6-9, 0
    [0x28] = K(7, 7),   // Enter: RETURN
    [0x29] = K(7, 2),   // Esc
    [0x2A] = K(7, 5),   // Backspace: BS
    [0x2B] = K(7, 3),   // Tab
    [0x2C] = K(8, 0),   // Space
    [0x2D] = K(1, 2),   // -
    [0x2E] = K(1, 3),   // =
    [0x2F] = K(1, 5),   // [
    [0x30] = K(1, 6),   // ]
    [0x31] = K(1, 4),   // backslash
    [0x33] = K(1, 7),   // ;
    [0x34] = K(2, 0),   // '
    [0x35] = K(2, 1),   // `
    [0x36] = K(2, 2),   // ,
    [0x37] = K(2, 3),   // .
    [0x38] = K(2, 4),   // /
    [0x39] = K(6, 3),   // Caps Lock: CAPS
    [0x3A] = K(6, 5), [0x3B] = K(6, 6), [0x3C] = K(6, 7), [0x3D] = K(7, 0), [0x3E] = K(7, 1),   // F1-F5
    [0x48] = K(7, 4),   // Pause: STOP
    [0x49] = K(8, 2),   // Insert: INS
    [0x4A] = K(8, 1),   // Home
    [0x4C] = K(8, 3),   // Delete: DEL
    [0x4D] = K(7, 6),   // End: SELECT
    [0x4F] = K(8, 7), [0x50] = K(8, 4), [0x51] = K(8, 6), [0x52] = K(8, 5),   // right, left, down, up
    [0x54] = K(9, 2), [0x55] = K(9, 0), [0x56] = K(10, 5), [0x57] = K(9, 1),  // keypad / * - +
    [0x58] = K(7, 7),   // keypad Enter: RETURN
    [0x59] = K(9, 4), [0x5A] = K(9, 5), [0x5B] = K(9, 6), [0x5C] = K(9, 7), [0x5D] = K(10, 0),   // keypad 1-5
    [0x5E] = K(10, 1), [0x5F] = K(10, 2), [0x60] = K(10, 3), [0x61] = K(10, 4), [0x62] = K(9, 3),  // 6-9, 0
    [0x63] = K(10, 7),  // keypad .
    [0xE0] = K(6, 1), [0xE4] = K(6, 1),   // Ctrl: CTRL
    [0xE1] = K(6, 0), [0xE5] = K(6, 0),   // Shift: SHIFT
    [0xE2] = K(6, 2),   // left Alt: GRAPH
    [0xE6] = K(6, 4),   // right Alt: CODE
};
// F6-F10: SHIFT and F1-F5
static const uint8_t SHIFTED[5] = {K(6, 5), K(6, 6), K(6, 7), K(7, 0), K(7, 1)};

// each matrix key held by how many things (keys, the keypad), so one letting
// go doesn't let go of another's
static uint8_t held[11][8];

static void hold(uint8_t k, bool down) {
    if (!k) return;
    const int row = k >> 3 & 15, bit = k & 7;
    if (row > 10) return;
    if (down) held[row][bit]++;
    else if (held[row][bit]) held[row][bit]--;
    msx_key(row, bit, held[row][bit] != 0);
}

void keymap_usb(uint8_t usage, bool down) {
    if (usage >= 0x3F && usage <= 0x43) {      // F6-F10
        hold(SHIFT, down);
        hold(SHIFTED[usage - 0x3F], down);
        return;
    }
    hold(USB[usage], down);
}

void keymap_pad(int keypad, bool plus, bool minus) {
    static uint8_t last[3];
    static const uint8_t DIGIT[10] = {K(0, 0), K(0, 1), K(0, 2), K(0, 3), K(0, 4), K(0, 5), K(0, 6), K(0, 7), K(1, 0), K(1, 1)};
    const uint8_t now[3] = {
        (uint8_t)(keypad >= 0 && keypad <= 9 ? DIGIT[keypad] : keypad == 10 ? K(8, 0) : keypad == 11 ? K(6, 5) : 0),
        (uint8_t)(plus ? K(8, 0) : 0),
        (uint8_t)(minus ? K(6, 5) : 0),
    };
    for (int i = 0; i < 3; i++) {
        if (now[i] == last[i]) continue;
        hold(last[i], false);
        hold(now[i], true);
        last[i] = now[i];
    }
}

void keymap_release(void) {
    memset(held, 0, sizeof held);
    msx_keys_up();
}
