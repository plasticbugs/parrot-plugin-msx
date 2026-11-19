# MSX: MSX1 cartridge games on the ColecoVision.
#
#   cmake -B build -G Ninja -DPICO_SDK_PATH=$HOME/pico-sdk
#   ninja -C build                     -> build/MSX.CVP
#
# and copy the .CVP into the card's /PLUGINS folder (docs/getting-started.md).
# Made by tools/new-plugin.py.
cmake_minimum_required(VERSION 3.13)
include(cmake/parrotcart.cmake)
project(msx C CXX ASM)
parrotcart_init()

add_executable(msx src/main.c)
parrotcart_frames(msx)           # a whole picture every TV frame: the frame kernel

# The features (docs/features.md). Each one links its code, sets up the
# USB port with only what it needs, and puts its flag in the .CVP's USES:
#   PADS KEYBOARD MOUSE MIDI_IN   the USB port as a host, through a powered hub
#   MIDI_OUT                      the USB port as a MIDI device, on a computer
#   PSRAM                         the 8 MB PSRAM
#   CARD                          the card: files and saves
# To add one later, add its word here and its code to src/main.c.
cvcart_features(msx PADS KEYBOARD MOUSE PSRAM)

# The plugin (docs/plugin-rules.md): the ID is yours for good; the version
# is the release's tag without its v.
cvcart_plugin(msx ID parrotcart.msx NAME "MSX" VERSION 0.1.0 BY "Parrot Cart"
              ICON src/icon.txt README README.md FILE MSX.CVP
              HOME /MSX OPENS .mx1 .mx2 FILE_ICON src/file_icon.txt)
