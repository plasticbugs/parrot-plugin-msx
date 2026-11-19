# MSX: MSX1 cartridge games on the ColecoVision. Pick a .mx1 or .mx2 file (an
# MSX cartridge ROM) in the menu: the cart runs an MSX1 with C-BIOS and
# mirrors its video chip to the console's. RESET goes back to the menu.
#
#   cmake -B build -G Ninja -DPICO_SDK_PATH=$HOME/pico-sdk
#   ninja -C build                     -> build/MSX.CVP
#
# and copy the .CVP into the card's /PLUGINS folder. Needs sjasmplus: the
# frame kernel, and C-BIOS (tools/cbios.sh). docs/design.md explains it all.
cmake_minimum_required(VERSION 3.13)
include(cmake/parrotcart.cmake)
project(msx C CXX ASM)
parrotcart_init()

# The same source makes the same file, whenever it's built: the compiler's
# __DATE__ and __TIME__ (the build date in the binary info) come from
# SOURCE_DATE_EPOCH, fixed here at the tested build's day, 2026-11-19.
set(BUILD_EPOCH 1795089600)
set(CMAKE_C_COMPILER_LAUNCHER ${CMAKE_COMMAND} -E env SOURCE_DATE_EPOCH=${BUILD_EPOCH})
set(CMAKE_CXX_COMPILER_LAUNCHER ${CMAKE_COMMAND} -E env SOURCE_DATE_EPOCH=${BUILD_EPOCH})

set(S ${CMAKE_CURRENT_LIST_DIR}/src)
add_executable(msx ${S}/main.c ${S}/msx.c ${S}/tms.c ${S}/mirror.c ${S}/sound.c ${S}/cpu.c ${S}/z80core.c
                   ${S}/keymap.c ${S}/romtype.c ${S}/sha1.c)
parrotcart_frames(msx)           # a whole picture every TV frame: the frame kernel

# the Z80 (vendor/z80, through src/z80core.c): its own warnings aren't ours to fix
set_source_files_properties(${S}/z80core.c PROPERTIES COMPILE_OPTIONS "-w")
target_compile_definitions(msx PRIVATE LSB_FIRST=1)

# C-BIOS, assembled from vendor/cbios into cbios_rom.h (the Mac tests make
# the same file in src/, from the same sources)
set(CBIOS_H ${CMAKE_CURRENT_BINARY_DIR}/cbios/cbios_rom.h)
file(GLOB CBIOS_SRC ${CMAKE_CURRENT_LIST_DIR}/vendor/cbios/src/*.asm)
add_custom_command(
    OUTPUT ${CBIOS_H}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/cbios
    COMMAND sh ${CMAKE_CURRENT_LIST_DIR}/tools/cbios.sh ${CBIOS_H}
    DEPENDS ${CBIOS_SRC} ${CMAKE_CURRENT_LIST_DIR}/tools/cbios.sh
    VERBATIM)
target_sources(msx PRIVATE ${CBIOS_H})
target_include_directories(msx PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/cbios)

# The features: USB pads, keyboard and mouse; the PSRAM for the cartridge;
# the card for the cartridge, the BIOS and the saves.
cvcart_features(msx PADS KEYBOARD MOUSE PSRAM CARD)

# The plugin: the ID is ours for good; the version is the release's tag
# without its v. The cart shows CART-README.md (the start of README.md).
cvcart_plugin(msx ID parrotcart.msx NAME "MSX" VERSION 0.1.0 BY "Parrot Cart"
              ICON src/icon.txt README CART-README.md FILE MSX.CVP
              HOME /MSX OPENS .mx1 .mx2 FILE_ICON src/file_icon.txt)
