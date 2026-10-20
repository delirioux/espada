# Playing Espada

## The screen

One screen, always the same: a mixing desk.

- The **header** names the group you are in (PLAY, SOUND, TRACKER, SONG, or EDIT and its lane), with four boxes for
  the bar in the phrase and the tempo.
- The **title** shows the setting in large letters, dots for where it sits in its group, and on the right what is
  about to happen (LAUNCH 3, DROP IN 2), the lane you act on when it is faded out or locked, or the queued profile.
  Messages appear here for a moment.
- The **lanes**: all five, always. Notes scroll past the white "now" line; the past is dimmed. Solid blocks play
  now; outlined blocks show what the queued profile will play. A faded-out lane is dark, a locked lane's name
  inverted. The white bar left of a name marks the lane the buttons and keys act on. (SONG's KEY row shows the chords
  here instead, its ARC row the arc, EDIT the step grid.)
- The **knobs**: KNOB 1 to 4, in the order they sit on the panel. In PLAY, SOUND and TRACKER they are the four lanes,
  LEAD, PAD, BASS and DRUMS, each with its value for the setting in the title: you see all four side by side and set
  them as on a mixer. In SONG they are four settings of their own. A bar shows a share, a word under it what the
  value means. "-" means the setting does nothing on that lane.

## Knobs

| Knob | Does |
| --- | --- |
| SELECT | the tempo, everywhere |
| PRESETS | the setting: steps through the group's settings |
| ALGORITHM | the lane the buttons and keys act on (LEAD, PAD, BASS, DRUMS, SFX) |
| KNOB 1 .. 4 | the setting for LEAD, PAD, BASS, DRUMS (SONG: the four settings shown) |

Turning a lane's knob also makes it the lane the buttons act on. Hold **SEL** to turn in steps of 10.

## The settings

A group is a button; pressed again, it steps to its next setting, as PRESETS does.

| Group | Button | Settings (KNOB 1..4 = LEAD, PAD, BASS, DRUMS) |
| --- | --- | --- |
| PLAY | HOME | melody, rhythm, pitch, change, density, octave, profile, sound, level |
| SOUND | FX | filter, resonance, wobble, drive, brightness, crush, vary |
| TRACKER | ARP | slide, retrigger, rate, echo |

| SONG (GLO) | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| LIVE | queue (the next profile) | blend | energy | space (the room) |
| KEY | key | scale | chords | bars per chord |
| ARC | arc on / off | build | peak | auto drop |
| SFX | sound | rhythm | change | level |
| SFX PART | profile | density | melody | vary |
| SFX FX | filter | resonance | wobble | drive |
| SFX FX 2 | crush | echo | retrigger | rate |

| EDIT | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| the lane's steps | step | length | bars | pattern (yours or the profile's) |

## Buttons

Every button does one thing, everywhere. ENV, LFO, REC, EDIT and the keys act on the marked lane.

| Button | Does |
| --- | --- |
| HOME, FX, ARP, GLO | the groups PLAY, SOUND, TRACKER, SONG; pressed again: the next setting. From EDIT: back |
| EDIT | the lane's own pattern, step by step (again: back) |
| PLAY | start / stop. SEL + PLAY: from the top |
| SEQ | LAUNCH: every unlocked lane moves to the queued profile on the next phrase (again: cancel) |
| REC | DROP, while playing: tap = a break from the next bar and the drop on the next phrase; hold = a break while held, the drop on the bar after you let go. The marked lane plays alone in the break. On EDIT: GRAB (what the lane plays now becomes its pattern); SEL + REC: clear it |
| ENV | fade the lane out over a bar; again: back in. SEL + ENV: LOCK (it keeps its profile when you blend or launch). On EDIT: TIE |
| LFO | a new idea for the lane from the next bar (MELODY moves on). On EDIT: GENERATE a pattern (a variation; SEL + LFO: a fresh one) |
| OCT- / OCT+ | the keys' octave; both together: back to 0. On EDIT: the octave you enter in |
| SAVE | keep everything (settings and patterns); while playing it waits until you stop. SEL + SAVE: the audio interrupt's time in the header |

Every button and key glows faintly; lit means: the group you are in, EDIT, playing, a break, a launch waiting, the
lane faded out, a save waiting, the keys' octave off 0.

## What the knobs do

- **The musical settings** (PLAY, the first four): **MELODY** picks the idea: every position is its own melody, riff,
  voicing or drum pattern, and turning back brings one back. **RHYTHM** goes from slow long notes to busy 16ths (on
  the other lanes: sparser or busier). **PITCH** goes from small steps to wide leaps and a higher register (the bass
  jumps octaves, the pad's chords open up, the drums bring in the timpani). **CHANGE** says how soon new material
  comes: FROZEN (the same 16 bars over and over), 64, 32 or 16 bars. All of it from the next bar.
- **Long arcs.** The lead thinks in 16-bar sections of two 8-bar periods: the motif stated, answered upside down,
  sequenced a step up and closed open on the third or fifth; then stated again, driven to the period's highest
  notes and closed on the root. The second period climbs higher. Every second time round the progression comes as a
  B section (its pairs swapped), so 8 chords make a 16-chord form.

- **Energy** (SONG, LIVE) adds to or takes from every lane's density at once: one knob for a build or a breakdown.
- **Space** (SONG, LIVE) is the room: a stereo reverb the lanes send to, the pad and sfx most, the bass hardly.
- **The arc** is off until you turn it on (SONG, ARC, knob 1). On: it starts with a few drum hits and builds bass,
  pad and lead in as the energy rises, breaks, drops and builds again; the ARC row draws it. DROP works without it.
- **Blend** moves lanes to the queued profile from the bottom up: drums, then bass, pad, lead (the word under it
  says how far).
- **Density:** 50 % is the part as written. Lower thins it; higher adds notes, hats, ghost snares and fills.
- **Sound** (PLAY): AUTO follows the profile; or one of seventeen FM patches (FM BASS, SLAP, PIZZ, SUB, BRASS,
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
- **A lane's profile** (PLAY) sets that lane alone; the queued profile (SONG, LIVE) is the one blend and launch move to.
- **The lead** follows the chords: every beat is a chord tone, the notes between lead into the next one by scale
  steps, and each 4-bar phrase states a motif, answers it, varies it and lands on the root or third. Density adds a
  16th into a beat, never a stray note.
- Profile, blend, density, octave and the SONG settings change on the next bar; SOUND and TRACKER act at once.

## SFX

The fifth lane plays noise, no notes. Its sounds: **WASH** (filtered white noise with a slow sweep), **RISER**
(noise opening up under a tone that climbs four octaves), **IMPACT** (a falling sub and a noise burst), **ZAP** (a
laser: a sine falling fast), **CRACKLE** (vinyl clicks and hiss) and **BITS** (1-bit periodic noise, the metallic
8-bit kind, on the chord's root).

On **AUTO** (the default) it follows the arc: a riser fills the bar before each drop, the impact lands on it, and in
between it plays what fits the profile: washes for EPIC and FUSION, zaps and bits for HYPERSPACE, crackle and bits
for OLDSKOOL, more as the energy and the density rise. With the arc off: a riser every 8th bar, an impact after it.
Turn **SOUND** (SONG, SFX) to one of the six to hear only that one, in its own pattern. SFX FX and SFX FX 2 give it
the effects of any lane (a filter sweep over a wash, echo on the zaps). On EDIT the first six white keys (F3 .. D4) switch
WASH, RISER, IMPACT, ZAP, CRACKLE and BITS at the cursor.

## Keys

- The keys play the marked lane, live, from F3 to G5 shifted by OCT- / OCT+: a note
  sounds as long as you hold it, with the lane's sound. On DRUMS the first five white keys play kick, snare, hat,
  kettle drum and crash; on SFX the first six its sounds. The keys of the tonic are lit, and the keys you hold.
- They change no settings: the key of the music is SONG, KEY.
- On EDIT the white keys are scale steps counted from the chord root: C4 is the root, D4 the next step, and
  so on (OCT- / OCT+ shift by a scale octave). A note goes in at the cursor with the set length, and the cursor moves
  on. Black keys: a rest; SEL + a black key: tie. On the DRUMS lane the first five white keys (F3 G3 A3 B3 C4) switch
  kick, snare, hat, kettle drum and crash at the cursor; a black key moves the cursor on.

Your patterns follow the chords, as the generated parts do: they are written in scale steps, not fixed notes.

## MIDI

USB-MIDI out: lead, pad and bass on channels 1 to 3, the drums on channel 10 (kick 36, snare 38, hat 42,
tom 47, crash 49). MIDI in: start, continue and stop.
