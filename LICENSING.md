# Licensing

Espada is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). You may use, study, change and share it. If you give someone a firmware built from it, give them its
complete source under the same licence too.

## Who made what

| | |
| --- | --- |
| The platform: `firmware/hal/`, `firmware/loader/`, `crt0.S`, `app.ld`, `firmware/src/` (`usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `bootguard.h`, `uac_fir.h`), `tools/` (but `make_font5x7.py` and `make_logo.py`) | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)), with the changes of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) |
| The wiring `platform.c`, the host mode `tests/host.c`, `run_host.sh`, the platform docs (`docs/API.md`, `docs/RULES.md`, `docs/MAKE-IT-YOURS.md`) | zvenson ([Dinghy](https://github.com/zvenson/dinghy)), with Espada's changes (a patterns sector, encoders and releases in host mode) |
| The app `firmware/src/espada*.c`, the 5x7 font (`tools/make_font5x7.py`, `firmware/gen/espada_font5x7.h`), the boot logo (`tools/make_logo.py`, `firmware/gen/espada_logo.h`), `tests/demo.txt`, `docs/MANUAL.md` | delirioux |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Terminus font 8x16 (ter-u16n), rendered into `firmware/gen/felucca_font.h` (the platform's rescue and update screens) | SIL OFL 1.1 | `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## The logo

`assets/logo.png` is Espada's own logo, under the same licence as the rest; the boot screen shows it (`firmware/gen/espada_logo.h`).

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; "M-VAVE" and "FM-1" belong to their
owners. Espada is not made or endorsed by them.
