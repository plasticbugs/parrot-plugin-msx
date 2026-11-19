# Z80 core, vendored

- What: Juergen Buchmueller's portable Z80 emulator, V2.7, from MAME.
- From: the SMS plugin (`parrot-plugin-sms-gamegear`, `vendor/smsplus/`),
  which took it from [pico-smsplus](https://github.com/PicoPlus-devel/pico-smsplus)
  at commit `51432b2ef1a13c33ba38c5b543fbeb776bab88f6`, SMS Plus's copy
  with the Pico port's deferred-interrupt handling.
- Files: `z80.c`, `z80.h`, `z80daa.h`, `cpuintrf.h`, `osd_cpu.h`. All but
  `z80.c` are unchanged. The changes in `z80.c`, each marked "Parrot Cart"
  and listed under its notice, as the notice asks:
  - `Z80_M1_WAIT`: T-states added to each opcode fetch, because the MSX
    puts a wait state on every M1 cycle. It counts in the opcode
    dispatch, the block instructions' repeats, the loop shortcuts
    (`BURNODD`) and HALT (`z80_burn`). Without the define it's 0, as
    before.
  - The block instructions' repeats (`REPEAT_AT_ONCE`) charged a pass
    twice when the time slice ran out in the middle of one. They now
    loop until they break out themselves.
- Ours: `shared.h`, which stands in for SMS Plus's header of that name so
  `z80.c` builds without the rest of SMS Plus. `src/z80core.c` compiles
  `z80.c`; `src/cpu.c` connects it to the MSX.
- License: the notice at the top of `z80.c`. It is freeware for
  non-commercial use; it may be redistributed, changed or not, with the
  author in the credits; a changed file must say it was changed; the
  notice must stay; commercial use needs the author's permission.
