# Licensing

Espada is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). You may use, study, change and share it. If you give someone a firmware built from it, give them its
complete source under the same licence too.

## Who made what

| | |
| --- | --- |
| The platform: `firmware/hal/`, `firmware/loader/`, `crt0.S`, `app.ld`, `firmware/src/` (`usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `bootguard.h`, `uac_fir.h`), `tools/` (but `make_logo.py` and `check_names.py`) | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)), with the changes of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) |
| The browser page and its audio worklet `web/emu/index.html`, `web/emu/worklet.js` (Felucca's, after charlesvestal's [X0X](https://github.com/charlesvestal/fm1-x0x), GPL-3.0); the web update protocol `web/fm1pkg.js`, `web/fm1ota.js`; `web/espada_installer.html` is made after Felucca's installer | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)) |
| The wiring `platform.c`, the host mode `tests/host.c`, `run_host.sh`, the platform docs (`docs/API.md`, `docs/RULES.md`, `docs/MAKE-IT-YOURS.md`) | zvenson ([Dinghy](https://github.com/zvenson/dinghy)), with Espada's changes (a patterns sector, encoders and releases in host mode) |
| The app `firmware/src/espada*.c`, the boot logo (`tools/make_logo.py`, `firmware/gen/espada_logo.h`), `tools/check_names.py`, the browser build's FM-1 (`web/emu/espada_web.c`, `espada_build.sh`, `espada_emu_test.mjs`), `tests/demo.txt`, `docs/MANUAL.md`, the installer's Espada parts and `web/espada_make_site.py` | delirioux |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| DotGothic16 (cut to printable ASCII), the browser page's knob labels: `web/emu/fonts/` | SIL OFL 1.1 | `web/emu/fonts/OFL.txt` |
| Terminus font (ter-u16n), rendered into `firmware/gen/felucca_font.h`: all of Espada's screen text, and the platform's rescue and update screens | SIL OFL 1.1 | `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 (`LICENSES/Apache-2.0.txt`) | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## The logo

`assets/logo.png` is Espada's own logo, under the same licence as the rest; the boot screen shows it (`firmware/gen/espada_logo.h`).

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; "M-VAVE" and "FM-1" belong to their
owners. Espada is not made or endorsed by them.
