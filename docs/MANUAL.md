# Playing Espada

## The screen

One screen, always the same: a mixing desk.

- The **header** names the group you are in (PLAY, SOUND, TRACKER, SONG, or EDIT and its lane), with four boxes for
  the bar in the phrase and the tempo.
- The **title** shows the setting in large letters, dots for where it sits in its group, and on the right what is
  about to happen (LAUNCH 3, DROP IN 2), the lane you act on when it is faded out or locked, or the queued style. Beside the group the header names the
  style playing.
  Messages appear here for a moment.
- The **lanes**: all five, always. Notes scroll past the white "now" line; the past is dimmed. Solid blocks play
  now; outlined blocks show what the queued style will play. A faded-out lane is dark, a locked lane's name
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
| PLAY | HOME | melody, rhythm, pitch, change, density, octave, style, sound, level |
| SOUND | FX | filter, resonance, wobble, drive, brightness, crush, vary, reverb |
| TRACKER | ARP | slide, retrigger, rate, echo |

| SONG (GLO) | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| LIVE | style (the next one) | blend | energy | space (the room) |
| KEY | key | scale | chords | bars per chord |
| ARC | arc on / off | build | peak | auto drop |
| SFX | sound | rhythm | change | level |
| SFX PART | style | density | melody | vary |
| SFX FX | filter | resonance | wobble | drive |
| SFX FX 2 | crush | echo | retrigger | rate |
| SFX ROOM | reverb | | | |

| EDIT | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| the lane's steps | step | length | bars | pattern (yours or the style's) |
| EDIT 2 (PRESETS) | fill (the style LFO fills from) | copy (the lane SEQ copies from) | | |

## Setup

Hold **HOME** for SETUP; HOME (or any group button, or EDIT) leaves it. PRESETS steps through its rows:

| SETUP | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| LOOK | palette: ESPADA (its colours), AMBER, GREEN, CYAN, RED, MONO | screen: 100, 75, 50 or 25 % | lights: the keys' and buttons' glow on or off | |
| MIDI | the lead's channel | the pad's | the bass's | the drums' (1..16 or OFF) |
| RESET | defaults: five clicks to the right puts everything back as it comes (patterns cleared); SAVE keeps it | | | |

SAVE keeps the setup with everything else.

## Buttons

Every button does one thing, everywhere. ENV, LFO, REC, EDIT and the keys act on the marked lane.

| Button | Does |
| --- | --- |
| HOME, FX, ARP, GLO | the groups PLAY, SOUND, TRACKER, SONG; pressed again: the next setting. From EDIT: back. Hold HOME: SETUP |
| EDIT | the lane's own pattern, step by step (again: back) |
| PLAY | stopped: start from the top with an INTRO (SEL + PLAY: everything at once). Playing: the END (SEL + PLAY, or PLAY again during the END: stop now) |
| SEQ | LAUNCH: every unlocked lane moves to the queued style at once, at the style's tempo. SEL + SEQ: launch and keep your tempo. On EDIT: COPY the pattern of the lane set on EDIT 2 into this one |
| REC | while playing, the marked lane alone at once. Tap = INTER (it plays alone for two bars, then the others come back one by one); hold = DROP (alone while held, everything back the moment you let go, with a crash and an impact). On EDIT: GRAB (what the lane plays now becomes its pattern); SEL + REC: clear it |
| ENV | fade the lane out over a bar; again: back in. SEL + ENV: LOCK (it keeps its style when you blend or launch). On EDIT: TIE |
| LFO | a new idea for the lane, at once (MELODY moves on). On EDIT: GENERATE a pattern: with FILL on OWN a variation of the one there (SEL + LFO: a fresh one), with FILL on a style a fresh one in that style |
| OCT- / OCT+ | the keys' octave; both together: back to 0. On EDIT: the octave you enter in |
| SAVE | keep everything (settings and patterns); while playing it waits until you stop. SEL + SAVE: the audio interrupt's time in the header |

Every button and key glows faintly; lit means: the group you are in, EDIT, playing, a break, a launch waiting, the
lane faded out, a save waiting, the keys' octave off 0.

## What the knobs do

- **The musical settings** (PLAY, the first four): **MELODY** picks the idea: every position is its own melody, riff,
  voicing or drum pattern, and turning back brings one back. **RHYTHM** goes from slow long notes to busy 16ths (on
  the other lanes: sparser or busier). **PITCH** goes from small steps to wide leaps and a higher register (the bass
  jumps octaves, the pad's chords open up, the drums bring in the timpani). **CHANGE** says how soon new material
  comes: FROZEN (the same 16 bars over and over), 64, 32 or 16 bars. Every change is heard at once, from the next
  16th: the rest of the bar is written again.
- **Long arcs.** The lead thinks in 16-bar sections of two 8-bar periods: the motif stated, answered upside down,
  sequenced a step up and closed open on the third or fifth; then stated again, driven to the period's highest
  notes and closed on the root. The second period climbs higher. Every second time round the progression comes as a
  B section (its pairs swapped), so 8 chords make a 16-chord form.

- **Energy** (SONG, LIVE) adds to or takes from every lane's density at once: one knob for a build or a breakdown.
- **Space** (SONG, LIVE) is the room, a stereo reverb, and how loud it is. **Reverb** (SOUND, and SFX ROOM for the
  sfx lane) is how much of each lane goes into it. It starts with the sfx all the way in (100), the pad much (60),
  the lead some (35), the drums a little (18) and the bass hardly (3). The sfx lane starts at level 30.
- **The arc** is off until you turn it on (SONG, ARC, knob 1). On: it starts with a few drum hits and builds bass,
  pad and lead in as the energy rises, breaks, drops and builds again; the ARC row draws it. DROP works without it.
- **Blend** moves lanes to the queued style from the bottom up: drums, then bass, pad, lead (the word under it
  says how far).
- **Density:** 50 % is the part as written. Lower thins it; higher adds notes, hats, ghost snares and fills.
- **Sound** (PLAY): AUTO follows the style; or one of seventeen FM patches (FM BASS, SLAP, PIZZ, SUB, BRASS,
  STRINGS, BELL, ORGAN, FLUTE, CHIP, RAZOR, PULSE, PIANO, E.PIANO, CHAOS, VIOLINS, CELLOS), or one of three drum kits (STANDARD,
  TRACKER, ELECTRO). RAZOR is a cutting, saw-like lead; PULSE a hollow one; PIANO and E.PIANO ring out. CHAOS is
  three sounds in one: a light saw lead, a heavy pad under it (two detuned oscillators, one an octave down, fading
  in slowly and out late) and brown noise that breathes with the pad. STRINGS, VIOLINS, CELLOS, BRASS and FLUTE carry
  a little noise in the tone (bow, breath), as recorded ones do; the kits' snares and cymbals ring darker and longer.
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
- **The form.** A piece has an INTRO, INTERs and an END, played by hand:
  - **INTRO** (PLAY): pad, drums or bass starts alone, chosen at random; the other two follow two bars apart, then
    the lead and the sfx. All in after six bars.
  - **INTER** (a tap on REC): the marked lane alone for two bars, then the others back one by one, a bar apart, in a
    random order.
  - **DROP** (holding REC): the marked lane alone for as long as you hold; everything back at once when you let go.
  - **END** (PLAY while playing): lead, sfx and pad fade out one by one, a bar apart, then bass or drums; the one
    left (chosen at random) plays alone two bars and fades last. Then Espada stops. The title says INTRO, INTER or
    END while one is running.
- **The ten styles.** Each is a chord progression, a part for each lane, an instrument for each, a tempo, and a way
  of playing. Change the mood with STYLE (SONG, LIVE) and SEQ.

| Style | Tempo | What it is |
| --- | --- | --- |
| EPIC | 125 | orchestral minor: long lead lines over strings, a driving bass, toms and timpani |
| HYPERSPACE | 130 | techno minor: power chords, a chopped pad, chord stabs, straight drums |
| FUSION | 125 | melodic major with a chromatic turn: a riff through every chord, a pedal bass, lush chords |
| OLDSKOOL | 125 | the 4-channel sound: arpeggio chords, an octave bass, a busy lead with an echo |
| SYMPHONY | 110 | the orchestra: two bars a chord, swelling strings, a violin line, cellos, timpani |
| TROPIC | 110 | sunny mixolydian: chords on the off-beat, a deep bass with rests, congas, a flute |
| STEEL | 140 | heavy: power chords, a galloping bass, double kick, a saw lead that glides |
| DRIFT | 96 | floating: sus chords resolving, two bars a chord, swells, a breathy lead with a long echo, no snare |
| FUNK | 112 | a major vamp with a diminished passing chord: slap bass, clav stabs, ghost notes |
| RAVE | 138 | four on the floor: an acid bass that slides, organ chords, brass stabs, gusts of noise |

- **How a style plays.** As a tracker musician plays the notes by hand: some lead notes glide in from the last one
  (STEEL most, DRIFT and FUSION often), long chords swell in or fade out (DRIFT and SYMPHONY most), and the lead
  gets an echo, a quieter copy three 16ths later (DRIFT, OLDSKOOL, RAVE). The style's echo adds to the lane's ECHO.
- **SYMPHONY** is the orchestra: two bars a chord in minor with a major dominant before the
  return; strings in open voicing (root, fifth, tenth) with a suspension resolving on each new chord and the inner
  voice moving on the last bar; a legato violin line in long arcs; cellos and basses holding the root and walking
  down into the next chord; timpani on the one with a roll into each phrase, a bass drum every other bar, a cymbal
  on the phrase and a cymbal swell (sfx) before it; a march snare when the energy is up.
- **A lane's style** (PLAY) sets that lane alone; the queued style (SONG, LIVE) is the one blend and launch move to.
- **The lead** follows the chords: every beat is a chord tone, the notes between lead into the next one by scale
  steps, and each 4-bar phrase states a motif, answers it, varies it and lands on the root or third. Density adds a
  16th into a beat, never a stray note.
- Every setting acts at once: the music changes from the next 16th (a note already sounding plays on). LAUNCH and
  DROP too. Only ENV takes a bar: it fades.

## SFX

The fifth lane plays noise, no notes. Its sounds: **WASH** (filtered white noise with a slow sweep), **RISER**
(noise opening up under a tone that climbs four octaves), **IMPACT** (a falling sub and a noise burst), **ZAP** (a
laser: a sine falling fast), **CRACKLE** (vinyl clicks and hiss), **BITS** (1-bit periodic noise, the metallic
8-bit kind, on the chord's root), **SURF** (dark brown noise in waves, brighter on each crest) and **GALE** (white
noise through a narrow band that wanders, a wind in gusts).

On **AUTO** (the default) it follows the arc: a gale fills the bar before each drop, the impact lands on it, and in
between it plays what fits the style: washes and surf for FUSION and DRIFT, surf and washes for TROPIC, washes and a
gale for EPIC, a cymbal swell and a far wind for SYMPHONY, zaps, gusts and bits for HYPERSPACE, crackle and bits for
OLDSKOOL, crackle for FUNK, impacts and a storm for STEEL, zaps and a gust into every phrase for RAVE, more as the
energy and the density rise. With the arc off: a gale every 8th bar, an impact after it. The RISER is never chosen
on AUTO; turn it on yourself.
Turn **SOUND** (SONG, SFX) to one of the eight to hear only that one, in its own pattern. SFX FX and SFX FX 2 give it
the effects of any lane (a filter sweep over a wash, echo on the zaps). On EDIT the first eight white keys (F3 .. F4)
switch WASH, RISER, IMPACT, ZAP, CRACKLE, BITS, SURF and GALE at the cursor.

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
tom 47, crash 49); SETUP, MIDI changes the channels or turns a lane off. MIDI in: start, continue and stop.
