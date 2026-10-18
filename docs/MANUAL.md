# Playing Espada

## The screen

As on Tuna: the **header** says where you are (MAIN, BASS, BASS FX, SONG ...), with four boxes for the bar in the
phrase and the tempo. Under it, **four cards**: what KNOB 1 to 4 do here and their values, in the order the knobs
sit on the panel. The card of the knob you turn lights up. The **panel** shows the music: on MAIN all five lanes,
on a channel its own lane large, on EDIT the step grid, on SONG the chords. The **footer** says what PRESETS sets
(MAIN: the queued profile; a channel: its sound), and messages.

In the lanes, notes scroll past the white "now" line; the past is dimmed. Solid blocks play now; outlined blocks
show what the queued profile will play. The label shows the profile (USR for your own pattern), FX when an effect
is on; a locked lane's name is inverted.

## Knobs

| Knob | Does |
| --- | --- |
| SELECT | the tempo, everywhere |
| ALGORITHM | the channel: MAIN, LEAD, PAD, BASS, DRUMS, SFX (as HOME, both ways) |
| PRESETS | the sound: on MAIN the queued profile, on a channel its instrument (DRUMS: the kit, SFX: the sound) |
| KNOB 1 .. 4 | the four cards |

Hold **SEL** to turn in steps of 10.

## Pages

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| MAIN | queue (the next profile) | blend | energy | space (the room) |
| MIX (MAIN, FX) | lead level | pad level | bass level | drums level (PRESETS: sfx) |
| a channel (HOME) | melody | rhythm | pitch | change (SFX: sound, rhythm, change, level) |
| PART (EDIT twice) | its profile | density | octave | level |
| FX | filter | resonance | wobble | drive |
| FX 2 (FX again) | brightness | crush | vary | |
| TRK (ARP) | slide | retrigger | rate | echo |
| EDIT | step | length | bars | pattern (yours or the profile's) |
| SONG (GLO) | key | scale | chords | bars per chord |
| ARC (GLO again) | arc on / off | build | peak | auto drop |

## Buttons

| Button | Does |
| --- | --- |
| HOME | the next channel: MAIN, LEAD, PAD, BASS, DRUMS, SFX; it keeps the page (BASS FX to DRUMS FX). SEL + HOME: MAIN |
| FX | the channel's effects; again: FX 2; again: back. On MAIN: the MIX page (the lanes' levels) |
| ARP | the channel's tracker effects (again: back) |
| EDIT | the channel's own pattern, step by step; again: PART (profile, density, octave, level); again: back |
| GLO | the song: key, scale, chords; again: the arc; again: back |
| PLAY | start / stop. SEL + PLAY: from the top |
| SEQ | LAUNCH: every unlocked lane moves to the queued profile on the next phrase (again: cancel) |
| REC | DROP, while playing: tap = a break from the next bar and the drop on the next phrase; hold = a break while held, the drop on the bar after you let go. The channel in view (MAIN: the last one) plays alone in the break. On EDIT: GRAB (what the lane plays now becomes its pattern); SEL + REC: clear it |
| ENV | MUTE the channel: it fades out over a bar; again: it fades back in. SEL + ENV: LOCK (it keeps its profile when you blend or launch). On EDIT: TIE |
| LFO | a new idea for the channel from the next bar (MELODY moves on). On EDIT: GENERATE a pattern (a variation; SEL + LFO: a fresh one) |
| OCT- / OCT+ | the keys' octave; both together: back to 0. On EDIT: the octave you enter in |
| SAVE | keep everything (settings and patterns); while playing it waits until you stop. SEL + SAVE: the audio interrupt's time in the header |

Every button and key glows faintly; lit means: the page you are on, playing, a break, a launch waiting, a locked
lane, a save waiting, the keys' octave off 0.

## What the knobs do

- **The four knobs of a channel** (as Tuna's): **MELODY** picks the idea: every position is its own melody, riff,
  voicing or drum pattern, and turning back brings one back. **RHYTHM** goes from slow long notes to busy 16ths (on
  the other lanes: sparser or busier). **PITCH** goes from small steps to wide leaps and a higher register (the bass
  jumps octaves, the pad's chords open up, the drums bring in the timpani). **CHANGE** says how soon new material
  comes: FROZEN (the same 16 bars over and over), 64, 32 or 16 bars. All of it from the next bar.
- **Long arcs.** The lead thinks in 16-bar sections of two 8-bar periods: the motif stated, answered upside down,
  sequenced a step up and closed open on the third or fifth; then stated again, driven to the period's highest
  notes and closed on the root. The second period climbs higher. Every second time round the progression comes as a
  B section (its pairs swapped), so 8 chords make a 16-chord form.

- **Energy** (MAIN) adds to or takes from every lane's density at once: one knob for a build or a breakdown.
- **Space** (MAIN) is the room: a stereo reverb the lanes send to, the pad and sfx most, the bass hardly.
- **The arc** is off until you turn it on (GLO twice, knob 1). On: it starts with a few drum hits and builds bass,
  pad and lead in as the energy rises, breaks, drops and builds again; the ARC page draws it. DROP works without it.
- **Blend** moves lanes to the queued profile from the bottom up: drums, then bass, pad, lead (the D B P L boxes).
- **Density:** 50 % is the part as written. Lower thins it; higher adds notes, hats, ghost snares and fills.
- **Instrument:** AUTO (A:) follows the profile; or one of seventeen FM patches (FM BASS, SLAP, PIZZ, SUB, BRASS,
  STRINGS, BELL, ORGAN, FLUTE, CHIP, RAZOR, PULSE, PIANO, E.PIANO, CHAOS, VIOLINS, CELLOS), or one of three drum kits (STANDARD,
  TRACKER, ELECTRO). RAZOR is a cutting, saw-like lead; PULSE a hollow one; PIANO and E.PIANO ring out. CHAOS is
  three sounds in one: a light saw lead, a heavy pad under it (two detuned oscillators, one an octave down, fading
  in slowly and out late) and brown noise that breathes with the pad.
- **Vary:** each 4-bar phrase gives the lane a character on top of your effects: CLEAN, ECHO, SLIDE, BITE
  (brightness and drive), FILTER (a low-pass with resonance) or STUTTER (retrigger). VARY sets how far it reaches;
  the value shows the phrase's character. The lead starts at 40 %, the other lanes at 0. Your own settings stay as
  you set them; the lane on screen shows what plays.
- **Filter:** one knob, left = low-pass, right = high-pass, centre = off. **Wobble** moves the cutoff in time with the
  tempo (1/1 to 1/16). On screen the filter darkens what it cuts away.
- **Brightness** raises the FM modulation: from a soft sine toward a buzzy saw. **Drive** saturates.
- **Slide** glides each note in from the last one. **Retrigger** makes a share of the notes stutter (always the same
  notes, so it stays a pattern); the rate sets 1 to 3 repeats per 16th. **Crush** lowers the bits and the sample rate.
  **Echo** plays two quieter copies a dotted 8th apart.
- **Stereo:** as a tracker spreads its channels left and right, Espada spreads its voices: the pad's and the sfx's
  every other voice to the right, hats and crash right of centre, the lead a little right, bass and kick in the
  middle. STRINGS and BRASS play as sections (two more players a little out of tune) and the strings swell in.
  HYPERSPACE's pad is chopped: the chord struck every 16th with an accent pattern, as a tracker re-strikes a long
  pad sample.
- **SYMPHONY**, the fifth profile, is the orchestra: two bars a chord in minor with a major dominant before the
  return; strings in open voicing (root, fifth, tenth) with a suspension resolving on each new chord and the inner
  voice moving on the last bar; a legato violin line in long arcs; cellos and basses holding the root and walking
  down into the next chord; timpani on the one with a roll into each phrase, a bass drum every other bar, a cymbal
  on the phrase and a cymbal swell (sfx) before it; a march snare when the energy is up.
- **A channel's profile** sets that lane alone (MAIN's profile is the one blend and launch move to).
- **The lead** follows the chords: every beat is a chord tone, the notes between lead into the next one by scale
  steps, and each 4-bar phrase states a motif, answers it, varies it and lands on the root or third. Density adds a
  16th into a beat, never a stray note.
- Profile, blend, density, octave and the SONG page change on the next bar; the FX and TRK pages act at once.

## SFX

The fifth channel plays noise, no notes. Its sounds: **WASH** (filtered white noise with a slow sweep), **RISER**
(noise opening up under a tone that climbs four octaves), **IMPACT** (a falling sub and a noise burst), **ZAP** (a
laser: a sine falling fast), **CRACKLE** (vinyl clicks and hiss) and **BITS** (1-bit periodic noise, the metallic
8-bit kind, on the chord's root).

On **AUTO** (the default) it follows the arc: a riser fills the bar before each drop, the impact lands on it, and in
between it plays what fits the profile: washes for EPIC and FUSION, zaps and bits for HYPERSPACE, crackle and bits
for OLDSKOOL, more as the energy and the density rise. With the arc off: a riser every 8th bar, an impact after it.
Turn **SOUND** to one of the six to hear only that one, in its own pattern. Its FX and TRK pages work as on any
lane (a filter sweep over a wash, echo on the zaps). On its EDIT page the first six white keys (F3 .. D4) switch
WASH, RISER, IMPACT, ZAP, CRACKLE and BITS at the cursor.

## Keys

- The keys play the channel in view, live (on MAIN the last channel), from F3 to G5 shifted by OCT- / OCT+: a note
  sounds as long as you hold it, with the channel's sound. On DRUMS the first five white keys play kick, snare, hat,
  kettle drum and crash; on SFX the first six its sounds. The keys of the tonic are lit, and the keys you hold.
- They change no settings: the key of the music is on the SONG page.
- On the EDIT page the white keys are scale steps counted from the chord root: C4 is the root, D4 the next step, and
  so on (OCT- / OCT+ shift by a scale octave). A note goes in at the cursor with the set length, and the cursor moves
  on. Black keys: a rest; SEL + a black key: tie. On the DRUMS lane the first five white keys (F3 G3 A3 B3 C4) switch
  kick, snare, hat, kettle drum and crash at the cursor; a black key moves the cursor on.

Your patterns follow the chords, as the generated parts do: they are written in scale steps, not fixed notes.

## MIDI

USB-MIDI out: lead, pad and bass on channels 1 to 3, the drums on channel 10 (kick 36, snare 38, hat 42,
tom 47, crash 49). MIDI in: start, continue and stop.
