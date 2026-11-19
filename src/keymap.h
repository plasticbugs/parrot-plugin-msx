// keymap.h -- the MSX's keyboard from a USB keyboard and the controllers.
//
// The USB keyboard is the MSX's international (US) keyboard, key for key
// where both have the key. The MSX's own keys:
//
//   GRAPH  left Alt          CODE    right Alt
//   STOP   Pause             SELECT  End
//   F1-F5  F1-F5 (F6-F10: SHIFT and F1-F5, as on the MSX)
//   the keypad: the MSX's keypad (Enter is RETURN)
//
// The ColecoVision's keypad: * is SPACE, # is F1 (pause in Konami's games),
// the digits are the digit keys. A USB pad's + is SPACE and - is F1.
#pragma once
#include <stdbool.h>
#include <stdint.h>

// a USB key (HID usage, 0xE0-0xE7 the modifiers) went down or up
void keymap_usb(uint8_t usage, bool down);

// the controllers' keys held this frame: the console's keypad (0-9,
// 10 = *, 11 = #, -1 none), and a USB pad's + and -
void keymap_pad(int keypad, bool plus, bool minus);

// let go of every key (the keyboard unplugged)
void keymap_release(void);
