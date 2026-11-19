// main.c -- MSX: MSX1 cartridge games on the ColecoVision.
//
// The cart runs the MSX (msx.c: the Z80, the slots and mappers, the PSG's
// registers, the keyboard) on core 0, a frame at a time, paced by the
// console's TV frames. The MSX's video chip is a TMS9918A like the
// console's, so its picture isn't drawn: mirror.c copies its VRAM and
// registers to the console in the frame kernel's lists. Its sound goes to
// the console's SN76489 (sound.c).
//
//   the BIOS       C-BIOS (built in), or the player's own MSX1 BIOS in
//                  /MSX/MSX.ROM (32K)
//   the cartridge  the file the menu opened (.mx1, .mx2: an MSX ROM), in
//                  the PSRAM; its mapper from romtype.c
//   its SRAM       the few cartridges with a battery: <name>.sav beside it
//   the controls   the console's controller (or USB pad 1) is joystick 1,
//                  USB pad 2 joystick 2, a USB keyboard the MSX's keyboard,
//                  a USB mouse an MSX mouse in port 1 (keymap.h)
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "cbios_rom.h"          // made by tools/cbios.sh: C-BIOS's main and logo ROMs
#include "cpu.h"
#include "cvdiag.h"
#include "cvframes.h"
#include "cvinput.h"
#include "cvpsram.h"
#include "cvsd.h"
#include "cvusb.h"
#include "font8.h"
#include "gbkernel.h"
#include "handoff.h"
#include "keymap.h"
#include "mirror.h"
#include "msx.h"
#include "romtype.h"
#include "sha1.h"
#include "sound.h"

#define BIOS_PATH   "/MSX/MSX.ROM"
#define SAVE_QUIET  1000000     // SRAM unchanged this long (us): written to the .sav
#define NOTE_US     1000000

static mirror_t mirror;
static sound_t snd;
static uint8_t list[MIRROR_LIST_MAX];
static size_t list_len;
static bool list_out;           // made, not taken yet
static uint32_t seen;           // the console's frame count the last list was made at
#define OWN_BIOS (CVPSRAM_CACHED + CVPSRAM_SIZE / 2)   // the player's BIOS, past the cartridge's room
static uint8_t sram[0x2000] __attribute__((aligned(4)));
static fs_t fs;
static fs_file_t file;
static char note[33];
static char dir[256], rom_name[FS_LNAME];

// ---- the console ----------------------------------------------------------------------
// One pass of the loop's duties: RESET, the power, USB. Never more than a
// few milliseconds apart (the console model fails a plugin that leaves it
// 100 ms).
static void duties(void) {
    cvf_task(time_us_32());
    cvusb_task();
}

// true when a TV frame has started and the last list has been taken: time
// for the next
static bool new_frame(void) {
    duties();
    if (list_out && cvf_publish(list, list_len)) list_out = false;
    if (list_out || cvf_frames() == seen) return false;
    seen = cvf_frames();
    return true;
}

static void send(const uint8_t *psg, int n) {
    list_len = mirror_frame(&mirror, &msx.vdp, psg, n, list);
    list_out = !cvf_publish(list, list_len);
}

// ---- messages: text on the console before (or instead of) the MSX ---------------------
// Graphics I and the font's 64 characters, 28 to a row (the TV cuts the edges)
static void message(const char *l1, const char *l2, const char *l3) {
    tms_t *t = &msx.vdp;
    memset(t->vram, 0, sizeof t->vram);
    for (int c = 32; c < 96; c++) memcpy(t->vram + c * 8, font8[c - 32], 8);
    memset(t->vram + 0x2000, 0xF1, 32);                     // white on black
    memset(t->vram + 0x1800, ' ', 768);
    t->vram[0x1B00] = 0xD0;
    const char *lines[3] = {l1, l2, l3};
    for (int i = 0; i < 3; i++)
        for (int k = 0; lines[i] && lines[i][k] && k < 28; k++) {
            char ch = lines[i][k];
            if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
            t->vram[0x1800 + 32 * (10 + 2 * i) + 2 + k] = (uint8_t)(ch >= 32 && ch < 96 ? ch : '?');
        }
    static const uint8_t R[8] = {0x00, 0xE0, 0x06, 0x80, 0x00, 0x36, 0x07, 0x01};
    memcpy(t->reg, R, 8);
    memset(t->dirty, 0xFF, sizeof t->dirty);
}

// the message on the TV for a while, or for good (us 0: RESET goes back to the menu)
static void show_for(uint32_t us) {
    const uint32_t t0 = time_us_32();
    while (us == 0 || time_us_32() - t0 < us)
        if (new_frame()) send(NULL, 0);
}

static void __attribute__((noreturn)) fail(const char *l1, const char *l2, const char *why) {
    snprintf(note, sizeof note, "MSX: %.27s", why);
    cvf_note(note);
    message(l1, l2, "RESET: BACK TO THE MENU");
    for (;;) show_for(0);
}

// ---- the card ---------------------------------------------------------------------------
static const char *fs_why(int r) { return r == FS_ENOFILE ? "THE FILE ISN'T THERE" : "THE CARD DIDN'T READ"; }

// the open file's first size bytes (at most max) into dst, the SHA-1 on the way
static bool read_file(uint8_t *dst, uint32_t size, uint32_t max, sha1_t *h) {
    static uint8_t sec[512] __attribute__((aligned(4)));
    if (size > max) size = max;
    for (uint32_t k = 0; k * 512 < size; k++) {
        const uint32_t lba = fs_lba(&fs, k);
        if (!lba || !cvsd_read(lba, sec)) return false;
        const uint32_t n = size - k * 512 < 512 ? size - k * 512 : 512;
        memcpy(dst + k * 512, sec, n);
        if (h) sha1_add(h, sec, n);
        if ((k & 7) == 7) duties();
    }
    return true;
}

// the player's own BIOS: /MSX/MSX.ROM, 32K, into the PSRAM; true if it's there and read
static bool own_bios(void) {
    fs_file_t f;
    if (fs_find(&fs, BIOS_PATH, &f) != FS_OK || f.is_dir || f.size != 0x8000) return false;
    if (fs_open(&fs, &f) != FS_OK) return false;
    return read_file(OWN_BIOS, 0x8000, 0x8000, NULL);
}

// ---- the cartridge's SRAM: <name>.sav ---------------------------------------------------
static uint16_t sav_nm[FS_LNAME];
static int sav_n;               // 0: no .sav (no SRAM, or no name for one)
static uint32_t sav_seen, sav_change_t, sav_writes, sav_fails;

static void sram_start(int kb) {
    sav_n = 0;
    if (!kb) return;
    static uint16_t nm[FS_LNAME];
    const int n = fs_name16(&fs, dir, file.first_clus, nm, FS_LNAME - 1);
    if (n <= 0) return;
    int stem = n;
    for (int i = 0; i < n; i++)
        if (nm[i] == '.') stem = i;
    if (stem + 4 > FS_LNAME - 1) return;
    memcpy(sav_nm, nm, (size_t)stem * 2);
    static const char ext[] = ".sav";
    for (int i = 0; i < 4; i++) sav_nm[stem + i] = (uint16_t)ext[i];
    sav_n = stem + 4;
    fs_file_t f;
    bool made;
    if (fs_create(&fs, dir, sav_nm, sav_n, 0, &f, &made) != FS_OK) return;   // none yet
    if (f.size < 512 || fs_open(&fs, &f) != FS_OK) return;
    if (!read_file(sram, (uint32_t)f.size, sizeof sram, NULL)) memset(sram, 0xFF, sizeof sram);
}

static bool sram_write(void) {
    if (!sav_n) return true;
    fs_file_t f;
    bool made;
    if (fs_create(&fs, dir, sav_nm, sav_n, sizeof sram, &f, &made) != FS_OK || f.size < sizeof sram ||
        fs_open(&fs, &f) != FS_OK) {
        sav_fails++;
        return false;
    }
    for (uint32_t k = 0; k < sizeof sram / 512; k++)
        if (fs_write_sec(&fs, k, sram + 512 * k)) {
            sav_fails++;
            return false;
        }
    sav_writes++;
    return true;
}

// each frame: the .sav, once the SRAM has kept still a second
static void sram_service(uint32_t now) {
    if (!sav_n) return;
    if (msx.sram_writes != sav_seen) {
        sav_seen = msx.sram_writes;
        sav_change_t = now | 1;
    } else if (sav_change_t && now - sav_change_t > SAVE_QUIET) {
        sav_change_t = 0;
        sram_write();
    }
}

static void going_back(void) {
    if (!sav_change_t) return;
    snprintf(note, sizeof note, sram_write() ? "MSX: saved" : "MSX: the .sav didn't write");
    cvf_note(note);
}

// ---- the controls -----------------------------------------------------------------------
static cvin_t in;

static uint8_t joy_bits(uint32_t up, uint32_t down, uint32_t left, uint32_t right, uint32_t a, uint32_t b) {
    return (uint8_t)((up ? JOY_UP : 0) | (down ? JOY_DOWN : 0) | (left ? JOY_LEFT : 0) | (right ? JOY_RIGHT : 0) |
                     (a ? JOY_A : 0) | (b ? JOY_B : 0));
}

static void controls(void) {
    uint8_t joy, fire, key;
    cvf_input(&joy, &fire, &key);
    cvin_frame(&in, joy, fire, key);
    cvin_update(&in);
    const uint8_t h = in.held;
    msx.joy[0] = joy_bits(h & CVIN_UP, h & CVIN_DOWN, h & CVIN_LEFT, h & CVIN_RIGHT, h & CVIN_FIRE_L, h & CVIN_FIRE_R);
    gamepad_t p;
    bool plus = false, minus = false;
    if (cvusb_pad(0, &p)) {
        plus = p.buttons & GP_PLUS;
        minus = p.buttons & GP_MINUS;
    }
    keymap_pad(in.key, plus, minus);
    msx.joy[1] = 0;
    if (cvusb_pad(1, &p)) {
        const uint32_t d = cvusb_pad_dirs(&p);
        msx.joy[1] = joy_bits(d & GP_UP, d & GP_DOWN, d & GP_LEFT, d & GP_RIGHT, p.buttons & GP_SOUTH, p.buttons & GP_EAST);
    }
    cvmouse_t m;
    msx.mouse_in = cvusb_mouse(&m);
    if (msx.mouse_in) {
        msx.mouse_dx += m.dx;
        msx.mouse_dy += m.dy;
        msx.mouse_buttons = (uint8_t)(m.buttons & 3);
    }
    cvusb_status_t s;
    cvusb_status(&s);
    static bool kb;
    if (kb && !s.keyboards) keymap_release();
    kb = s.keyboards;
    cvkey_t k;
    while (cvusb_key(&k)) keymap_usb(k.usage, k.down);
}

// ---- the note: what the menu shows after RESET ------------------------------------------
static uint32_t frames_run, late, us_max;

static void update_note(uint32_t now) {
    static uint32_t t_last, frames_last, late_last;
    if (now - t_last < NOTE_US) return;
    snprintf(note, sizeof note, "MSX stuck: pc %04X", cpu_pc());
    cvf_note(note);
    cvf_note_now();             // (the return block has this one, should the watchdog reboot)
    snprintf(note, sizeof note, "MSX %lu/s %lu late %lu.%lums%s", (unsigned long)(frames_run - frames_last),
             (unsigned long)(late - late_last), (unsigned long)(us_max / 1000), (unsigned long)(us_max / 100 % 10),
             sav_writes ? " saved" : "");
    cvf_note(note);
    t_last = now;
    frames_last = frames_run;
    late_last = late;
    us_max = 0;
}

// ---- the start, and the loop ------------------------------------------------------------
int main(void) {
    static const cvf_config_t cfg = {gbkernel_bin, GBKERNEL_BIN_LEN, handoff_bin, HANDOFF_BIN_LEN};
    cvf_start(&cfg);
    cvd_start();
    cvusb_start();
    cvin_init(&in);
    mirror_reset(&mirror);
    sound_reset(&snd, false);
    cvf_note("MSX");

    const cvl_t *b = cvf_launch();
    const bool have_file = b && b->path[0];
    if (have_file) {
        snprintf(dir, sizeof dir, "%s", b->path);
        char *slash = strrchr(dir, '/');
        snprintf(rom_name, sizeof rom_name, "%s", slash ? slash + 1 : dir);
        if (slash) *slash = 0;
        else dir[0] = 0;
    }
    message("MSX", have_file ? "STARTING" : NULL, have_file ? rom_name : NULL);
    show_for(200000);
    if (!have_file) fail("OPEN AN .MX1 FILE", "FROM THE MENU", "no file opened");
    if (!cvpsram_init()) fail("THE PSRAM DIDN'T ANSWER", NULL, "the PSRAM didn't answer");

    // the cartridge, into the PSRAM, padded to a power of 2
    const int r = cvsd_open_launched(&fs, b, true, &file);
    if (r != FS_OK) fail(fs_why(r), NULL, "the file couldn't be read");
    if (file.size < 0x2000 || file.size > CVPSRAM_SIZE / 2) fail("THAT ISN'T AN MSX ROM", "(8K TO 4 MB)", "not an MSX ROM");
    const uint32_t real = (uint32_t)file.size;
    uint32_t size = 0x2000;
    while (size < real) size <<= 1;
    uint8_t *rom = CVPSRAM_CACHED;
    sha1_t h;
    sha1_init(&h);
    if (!read_file(rom, real, real, &h)) fail("THE CARD DIDN'T READ", NULL, "a card read failed");
    memset(rom + real, 0xFF, size - real);
    uint8_t sum[20];
    sha1_done(&h, sum);
    const romtype_t rt = romtype_find(rom_name, sum, rom, real);

    // the BIOS: the player's own, or C-BIOS (with its logo ROM)
    const bool own = own_bios();
    memset(sram, 0xFF, sizeof sram);
    sram_start(rt.sram_kb);

    if (own) msx_init(OWN_BIOS, 0x8000, NULL, 0);
    else msx_init(cbios_main, sizeof cbios_main, cbios_logo, sizeof cbios_logo);
    msx_cart(rom, size, real, rt.mapper, rt.sram_kb ? sram : NULL, rt.sram_kb);
    msx_reset();
    sound_reset(&snd, rt.mapper == MAP_KONAMI_SCC);
    snprintf(note, sizeof note, "MSX %s %s%s", msx_mapper_name[rt.mapper], rt.why, own ? " OWN BIOS" : "");
    cvf_note(note);
    cvf_on_exit(going_back);

    for (;;) {
        const uint32_t before = seen;
        if (!new_frame()) continue;
        if (seen - before > 1 && frames_run) late += seen - before - 1;
        const uint32_t t0 = time_us_32();
        controls();
        msx_frame();
        uint8_t psg[16];
        const int n = sound_frame(&snd, &msx, psg);
        send(psg, n);
        frames_run++;
        const uint32_t now = time_us_32();
        if (now - t0 > us_max) us_max = now - t0;
        sram_service(now);
        update_note(now);
    }
}
