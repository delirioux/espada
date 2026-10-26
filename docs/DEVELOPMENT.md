# Developing Makohai

How Makohai is made and where it is going: the agreements, how changes reach GitHub, how the code is laid out
and why, how to build and test it, the open risks and what comes next. What Makohai is and how to play it is in
the [README](../README.md) and the [manual](MANUAL.md); the chip's rules are in [RULES.md](RULES.md).

Status: 27 October 2026. Version 1.1 runs on the FM-1. Its first overload (SYMPHONY with echo everywhere, 109 % of
the audio time) is closed by the voice budget; it has run for hours on the hardware without a crack. Six lanes,
ten styles played as in a tracker, long lead arcs, the one-screen interface, the browser version and the web
installer are in.

## Agreements

| What | Agreed |
| --- | --- |
| Home | GitHub, `github.com/delirioux/makohai`, branch `main`, public. |
| Upstream | Built on [Dinghy](https://github.com/zvenson/dinghy) 0.3, which carries Felucca's FM-1 platform as SLOOP has it. Never push to Felucca or Dinghy. Platform files (`platform.c`, `usb.c`, `ota.c`, `lcd.c`, `gfx.c`, `libc.c`, `recovery.c`) are touched only where Makohai hooks in. |
| Language | English throughout: code, comments, commits, docs. |
| Licence | GPL-3.0-only, as Dinghy and Felucca. JieLi SDK files in the package under Apache-2.0 ([LICENSING.md](../LICENSING.md)). |
| Commits | Author `delirioux <340923392+delirioux@users.noreply.github.com>`. Work done with Claude ends with the `Co-Authored-By` and `Claude-Session` lines. |
| The panel | The keys play music or enter steps, nothing else: never settings, never a channel. Settings are on the knobs and buttons. |
| The repo | The product and its plan. Conversations stay out of it. |

## How changes reach GitHub

Claude works in a sandbox that cannot push. Changes travel as git bundles:

1. Claude commits on `main` and writes `makohai.bundle` (`git bundle create makohai.bundle origin/main..main`).
2. Download it and apply it with the sync script: `Makohai-Sync.ps1` on Windows, `FM1-Sync.sh apply makohai` on
   Linux. It checks the bundle against the repo, shows the new commits, fast-forwards `main` and pushes.
3. A bundle builds on what GitHub has. When the other computer pushed last, pull first
   (`FM1-Sync.sh pull makohai`, or `git pull --ff-only`).

Every push builds the firmware, the browser version and the site (GitHub Actions, below).

## Where Makohai lives

The whole firmware is one translation unit: `makohai.c` holds the shared state and includes the parts, and
`platform.c` (with USB, updates, the screen) comes in with it. Host mode and the browser swap the platform for a
stand-in.

| File | Does |
| --- | --- |
| `firmware/src/makohai.c` | the app's shared state (lanes, effects, the clock), includes the parts below |
| `firmware/src/makohai_synth.c` | the audio interrupt: FM voices (patches with ensembles), drums, sfx voices, the stereo buses and the room, lane effects, the step clock, MIDI out |
| `firmware/src/makohai_music.c` | harmony, scales and progressions, the ten styles, their generators, the lead's long arcs, density, the arc, your own patterns, the bar cache, mute fades |
| `firmware/src/makohai_ui.c` | the screen (header, title, lanes, knobs), the groups and their settings, the panel, the LEDs, saving |
| `firmware/src/platform.c` | Dinghy's wiring of the FM-1 platform |
| `tests/host.c` | host mode: the platform as a stand-in; runs a script of buttons, encoders and keys over time, writes screens and a WAV |
| `web/emu/makohai_web.c` | the browser's stand-in: `web_*` exports for Felucca's browser FM-1 page |
| `web/makohai_make_site.py` | the site: start page, web installer, package, the browser version |
| `tools/check_names.py` | fails when the app and the platform declare a file-scope variable of the same name |

## Decisions and why

- **Dinghy, not Felucca's engines.** Makohai needs its own voices (FM patches in a tracker's manner, a synthesized
  kit, noise effects) and very little of Felucca's sound code. Dinghy gives the platform alone: boot, USB,
  updates, screen, panel, settings.
- **Integers only.** No FPU use, no 64-bit division ([RULES.md](RULES.md)). CI fails on any soft-float or 64-bit
  division routine in the image: a soft-float routine is what crashed an FM-1 running Tuna.
- **One translation unit, checked for names.** C merges two `static` file-scope variables of the same name
  into one without a word, and an enum constant the platform also has fails only the firmware build. Makohai's MIDI queue once shared its counters with the USB driver's that way, and the
  FM-1 froze on PLAY. `tools/check_names.py` runs in CI and catches it.
- **Bars are composed ahead, played in the interrupt.** The main loop writes each lane's next bar into a cache;
  the interrupt only plays events. Composing never runs late, and the screen can show what is coming.
- **Styles are data.** A style is a mode, a chord progression, a harmonic rhythm, a generator and an instrument per
  lane, a tempo, and a way of playing. The bass lane's style sets the chords for everyone, so a half-blended mix
  stays in key. The ten come from a study of 90s tracker modules: their chords (sus chords everywhere), drum grids
  and tempos.
- **The playing is part of the sound.** In the modules most of the character is in how notes are played: lead notes
  glide in (tone portamento), long chords swell and fade (volume slides), a second channel echoes the lead. Makohai
  marks events for that (EV_GLIDE, EV_SWELL, EV_FADE) and each style says how often; the voices do the rest. FM
  patches take a little noise (bow, breath), and the drums ring longer and darker, as recorded ones do.
- **The lead stays on the harmony.** Chord tones on every beat, scale runs between them. It writes 16-bar
  sections of two 8-bar periods: statement, answer, sequence and a half close, then a climb to the high point
  and a close on the root. CHANGE sets how soon new material comes; MELODY picks the idea.
- **50 % density is the style as written.** Lower thins it out, higher adds notes inside the chord and its
  scale. ENERGY moves every lane at once.
- **Your own parts in their own flash.** Up to 512 bars a lane, 48 bytes a bar (with the effect column), held in RAM
  (the pool; the undo copy in the main RAM) and saved to 148 KiB at the start of Felucca's free store (0x97000), which Makohai does not otherwise use. Every erase, write and
  read is checked against that window, a compile-time check proves 6 x 512 bars fit it, the header goes last with a
  CRC (a cut save reads as none), and the linker fails the build if the pool overflows. MY STYLE is counting, not a
  network: per place in the phrase the 16ths and sounds used and the scale-step jumps, rebuilt when the part changes.
- **Muting fades.** ENV fades a lane out or in over a bar instead of cutting it.
- **The room at half rate.** The stereo reverb runs at half the sample rate to fit the interrupt's budget.
- **One screen, a mixing desk.** The FM-1's screen is small: every setting gets the whole width, the four lanes'
  values side by side under the four knobs, all six lanes always in view. A group button picks the kind of setting
  and PRESETS steps through it, so no page hides behind another. The keys only play.
- **The arc is off until asked for.** Makohai starts with all lanes playing. The arc (build, break, drop) is a
  setting, and DROP works without it.
- **Settings are versioned.** A saved record with an older version is ignored rather than misread (now
  version 6).
- **The browser runs the firmware's code.** The same `makohai.c`, compiled to WebAssembly, on Felucca's browser
  FM-1. What plays in the browser is what plays on the FM-1.

## Building and testing

On Linux (or WSL2 / Docker), as in the [README](../README.md#build):

```
./build.sh                                   # the firmware: build/makohai.fwsc
tests/run_host.sh                            # host mode: screens as PNG, the sound as WAV
build/host/makohai OUT tests/demo.txt         # a scripted tour
python3 tools/check_names.py                 # no variable or enum constant shared with the platform
clang -fsyntax-only -w -m32 -ffreestanding -Ifirmware/hal -Ifirmware/src -Ifirmware/gen \
    -DFELUCCA_ID='"FM-1_47625642"' -DAPP_VER='"x"' firmware/src/makohai.c     # the firmware's unit, without the toolchain
web/emu/makohai_build.sh                      # the browser version: build/makohai_emu/
node web/emu/makohai_emu_test.mjs build/makohai_emu/makohai.wasm
```

CI (`.github/workflows/firmware.yml`) runs the name check and host mode, builds the firmware with JieLi's
toolchain, checks the image for 64-bit division and soft-float routines, packages it, builds the browser version
and publishes the site with the web installer. A tag `v*` makes a release with [RELEASE.md](RELEASE.md) as notes.

## Open risks

| Risk | What happens | Check |
| --- | --- | --- |
| The audio interrupt's time | the reverb, the stereo buses and the orchestra's ensembles cost time; above about 2.9 ms the sound may crackle when USB and the key scan interrupt it | SETUP, AUDIO (or SEL + SAVE) while SYMPHONY, STEEL or RAVE plays shows the worst time; trim the room or the ensembles if it is high |
| Sound on the hardware | levels and patches were tuned in host mode and the browser, not yet by ear on the FM-1's output | play every style on the FM-1 |
| Memory | the stacks' depth is decided at run time | SETUP, SYSTEM: the stacks are painted at boot and the least free since is shown; RAM and the pool from the linker's addresses |
| Name clashes in the one unit | a new file-scope variable with a platform name silently becomes one variable | settled: `check_names.py` in CI |
| Soft floats, 64-bit division | a missing routine crashes the FM-1 | settled: CI fails on them |

## Next

In order of value; open to change as Makohai is played.

1. **The new interface on the FM-1.** One screen as a mixing desk: a group button (PLAY, SOUND, TRACKER, SONG),
   PRESETS through its settings, KNOB 1..4 the setting for the four lanes side by side. Tried in the browser; next
   played on the FM-1 and tuned: the order of the settings, what reads at arm's length.
2. **Measured on the FM-1.** The interrupt's worst time with every style, the room trimmed if needed.
3. **Tuned by ear.** Patch levels, the orchestra's balance, the drums against the bass.
4. **Parts in the sets.** The eight sets (SONG, SETS) hold everything but your own parts; a set could take its
   parts along.
5. **More of the trackers' moves.** A key change up a step for the last chorus, a breakdown with only the pad,
   sample-offset style stutters, vibrato that sets in late on long notes.
6. **MIDI clock.** Follow an external clock and send one, so Makohai plays in time with other gear.
7. **The SFX lane on MIDI.** Today only lead, pad, bass (channels 1 to 3) and the drums (10) go out.

## Release checklist

Before 1.0 (done: 1.1 ran for hours on an FM-1 without a crack; the name is in neither the EUIPO nor the BOIP
register, and its likeness to "Mako" is judged acceptable):

1. **It works on the FM-1**: every style played, the interrupt's time measured, the way back to the stock
   firmware tried (OCT- at power-on, the official V15 from the installer).
2. **The name**: search "Makohai" in the EUIPO and BOIP registers (classes 9 and 15). Release as "Makohai for
   FM-1" at least.
3. **Licences and credits**: GPL-3.0-only; Dinghy's, SLOOP's and Felucca's credits in the README and on the
   site; the JieLi files' Apache-2.0 and the fonts' licences (LICENSING.md says where).
4. **Other names**: "for the M-VAVE FM-1" in words only, no logos, nothing that suggests it is official.
5. **Release notes**: [RELEASE.md](RELEASE.md) brought up to date, a tag `v1.0`.
