# Playing Espada

## Views

**HOME** steps through the views: MAIN, LEAD, PAD, BASS, DRUMS, SFX, and back to MAIN. The SELECT encoder does the
same in both directions; **SEL + HOME** goes straight back to MAIN.

- **MAIN:** all five lanes, small. Its encoders are global: the queued profile, blend and the tempo.
- **A channel** (LEAD .. SFX): that one lane on most of the screen, its notes large, and under it the six values of
  its encoders written out (the one you just turned gets a frame). **FX**, **ARP** and **EDIT** switch the channel
  to its effects, its tracker effects and its step editor; the same button again takes it back. From MAIN these
  buttons open the channel you were on last. HOME keeps the page as it moves on: from BASS FX to DRUMS FX.
- **SONG** (GLO, from anywhere): key, scale, chords and the arc, for the whole piece. GLO or HOME takes you back.

## The screen

- **Header:** where you are (MAIN, BASS, BASS FX, SONG ...), the profile (on MAIN the bass lane's, which sets the
  chords; on a channel its own), the arc's phase (BUILD, BREAK, PEAK, DOWN), phrase.bar and the tempo. The thin strip
  under it is the arc's energy along the timeline.
- **Lanes:** notes scroll past the white "now" line; the past is dimmed. Solid blocks play now; outlined blocks show
  what the queued profile will play. Each lane's label shows its profile (USR for your own pattern; inverted when
  the lane is locked) and FX when an effect is on.
- **Footer:** the queued profile and the blend (D B P L), launches and breaks, and messages.

The **legend** (what each encoder does here, and its value) is up when Espada starts. Tap **SEL** to hide it or bring
it back; moving to another view hides it.

## Buttons

| Button | Does |
| --- | --- |
| PLAY | start / stop. SEL + PLAY: from the top, with a fresh arc |
| HOME | the next view: MAIN, LEAD, PAD, BASS, DRUMS, SFX. SEL + HOME: MAIN |
| FX | the channel's FX page (again: back) |
| ARP | the channel's TRK page (tracker effects) |
| EDIT | the channel's EDIT page (your own parts) |
| GLO | the SONG page (key, scale, chords, the arc) |
| SEQ | LAUNCH: every unlocked lane moves to the queued profile on the next phrase (again: cancel) |
| REC | DROP: tap = a break from the next bar, the drop on the next phrase; hold = a break while held, the drop on the bar after you let go. The selected lane plays alone in the break. On the EDIT page: GRAB (what the lane plays now becomes its pattern); SEL + REC: clear the pattern |
| ENV | LOCK the selected lane (it keeps its profile). On the EDIT page: TIE (extend the last note) |
| LFO | GENERATE a pattern for the selected lane (a variation of the one there). SEL + LFO: a fresh one |
| OCT- / OCT+ | the lane's octave. On the EDIT page: the octave you enter in |
| SAVE | keep everything (settings and patterns) in flash; while playing it waits until you stop. SEL + SAVE: show the audio interrupt's time |
| SEL | tap: the legend on / off; held: encoders turn in steps of 10 |

## Encoders

| Page | SELECT | ALGORITHM | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 | PRESETS |
| --- | --- | --- | --- | --- | --- | --- | --- |
| MAIN | view | profile (queue) | blend | | | | tempo |
| a channel: PLAY | view | its profile (SFX: its sound) | density | octave | instrument | level | vary |
| FX | view | filter | resonance | wobble | brightness | drive | level |
| TRK | view | slide | retrigger | retrigger rate | crush | echo | level |
| EDIT | view | step | length | bars | pattern on/off | | |
| SONG | key | scale | chords (the progression) | bars per chord | build (OFF = no arc) | peak | auto drop |

- **Blend** moves lanes to the queued profile from the bottom up: drums, then bass, pad, lead (the D B P L boxes).
- **Density:** 50 % is the part as written. Lower thins it; higher adds notes, hats, ghost snares and fills.
- **Instrument:** AUTO (A:) follows the profile; or one of fourteen FM patches (FM BASS, SLAP, PIZZ, SUB, BRASS,
  STRINGS, BELL, ORGAN, FLUTE, CHIP, RAZOR, PULSE, PIANO, E.PIANO), or one of three drum kits (STANDARD, TRACKER,
  ELECTRO). RAZOR is a cutting, saw-like lead; PULSE a hollow one; PIANO and E.PIANO ring out.
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

- The keys only play: they enter steps on the EDIT page and change nothing else (the key of the music is on the
  SONG page). Elsewhere the keys of the tonic are lit.
- On the EDIT page the white keys are scale steps counted from the chord root: C4 is the root, D4 the next step, and
  so on (OCT- / OCT+ shift by a scale octave). A note goes in at the cursor with the set length, and the cursor moves
  on. Black keys: a rest; SEL + a black key: tie. On the DRUMS lane the first five white keys (F3 G3 A3 B3 C4) switch
  kick, snare, hat, kettle drum and crash at the cursor; a black key moves the cursor on.

Your patterns follow the chords, as the generated parts do: they are written in scale steps, not fixed notes.

## MIDI

USB-MIDI out: lead, pad and bass on channels 1 to 3, the drums on channel 10 (kick 36, snare 38, hat 42,
tom 47, crash 49). MIDI in: start, continue and stop.
