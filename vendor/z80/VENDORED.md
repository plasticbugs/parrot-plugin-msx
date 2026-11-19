# Z80 core, vendored

- What: Juergen Buchmueller's portable Z80 emulator, V2.7, from MAME.
- From: the SMS plugin (`parrot-plugin-sms-gamegear`, `vendor/smsplus/`),
  which took it from [pico-smsplus](https://github.com/PicoPlus-devel/pico-smsplus)
  at commit `51432b2ef1a13c33ba38c5b543fbeb776bab88f6`, SMS Plus's copy
  with the Pico port's deferred-interrupt handling.
- Files, unchanged: `z80.c`, `z80.h`, `z80daa.h`, `cpuintrf.h`, `osd_cpu.h`.
- Ours: `shared.h`, which stands in for SMS Plus's header of that name so
  `z80.c` builds without the rest of SMS Plus. `src/z80core.c` compiles
  `z80.c`; `src/cpu.c` connects it to the MSX.
- License: the notice at the top of `z80.c`. It is freeware for
  non-commercial use; it may be redistributed, changed or not, with the
  author in the credits; a changed file must say it was changed; the
  notice must stay; commercial use needs the author's permission. This
  copy is unchanged.
