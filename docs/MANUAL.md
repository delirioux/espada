# Playing Espada

## The screen

- **Header:** the name, the profile that sets the chords, the arc's phase (BUILD, BREAK, PEAK, DOWN), phrase.bar
  and the tempo. The thin strip under it is the arc's energy along the timeline.
- **Lanes:** LEAD, PAD, BASS, DRUMS from top to bottom. Notes scroll past the white "now" line; the past is dimmed.
  Solid blocks play now; outlined blocks show what the queued profile will play. Each lane's label shows its profile
  (or USR for your own pattern), LCK when locked and FX when an effect is on. The selected lane has a frame.
- **Footer:** the page's values; changes and messages show here for a moment.

Tap **SEL** to see what the encoders do on the current page.

## Buttons

| Button | Does |
| --- | --- |
| PLAY | start / stop. SEL + PLAY: from the top, with a fresh arc |
| HOME | the PLAY page |
| FX | the FX page (again: back to PLAY) |
| ARP | the TRK page (tracker effects) |
| EDIT | the EDIT page (your own parts) |
| GLO | the SONG page (key, scale, chords, the arc) |
| SEQ | LAUNCH: every unlocked lane moves to the queued profile on the next phrase (again: cancel) |
| REC | DROP: tap = a break from the next bar, the drop on the next phrase; hold = a break while held, the drop on the bar after you let go. The selected lane plays alone in the break. On the EDIT page: GRAB (what the lane plays now becomes its pattern); SEL + REC: clear the pattern |
| ENV | LOCK the selected lane (it keeps its profile). On the EDIT page: TIE (extend the last note) |
| LFO | GENERATE a pattern for the selected lane (a variation of the one there). SEL + LFO: a fresh one |
| OCT- / OCT+ | the lane's octave. On the EDIT page: the octave you enter in |
| SAVE | keep everything (settings and patterns) in flash; while playing it waits until you stop. SEL + SAVE: show the audio interrupt's time |
| SEL | held: encoders turn in steps of 10 |

## Encoders

| Page | SELECT | ALGORITHM | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 | PRESETS |
| --- | --- | --- | --- | --- | --- | --- | --- |
| PLAY | lane | profile (queue) | blend | density | octave | instrument | tempo |
| FX | lane | filter | resonance | wobble | brightness | drive | level |
| TRK | lane | slide | retrigger | retrigger rate | crush | echo | level |
| EDIT | lane | step | length | bars | pattern on/off | | |
| SONG | key | scale | progression | bars per chord | build (OFF = no arc) | peak | auto drop |

- **Blend** moves lanes to the queued profile from the bottom up: drums, then bass, pad, lead (the D B P L boxes).
- **Density:** 50 % is the part as written. Lower thins it; higher adds notes, hats, ghost snares and fills.
- **Instrument:** AUTO (A:) follows the profile; or one of ten FM patches (FM BASS, SLAP, PIZZ, SUB, BRASS, STRINGS,
  BELL, ORGAN, FLUTE, CHIP), or one of three drum kits (STANDARD, TRACKER, ELECTRO).
- **Filter:** one knob, left = low-pass, right = high-pass, centre = off. **Wobble** moves the cutoff in time with the
  tempo (1/1 to 1/16). On screen the filter darkens what it cuts away.
- **Brightness** raises the FM modulation: from a soft sine toward a buzzy saw. **Drive** saturates.
- **Slide** glides each note in from the last one. **Retrigger** makes a share of the notes stutter (always the same
  notes, so it stays a pattern); the rate sets 1 to 3 repeats per 16th. **Crush** lowers the bits and the sample rate.
  **Echo** plays two quieter copies a dotted 8th apart.
- Profile, blend, density, octave and the SONG page change on the next bar; the FX and TRK pages act at once.

## Keys

- On every page but EDIT a key sets the key of the music (from the next bar). The keys of the tonic are lit.
- On the EDIT page the white keys are scale steps counted from the chord root: C4 is the root, D4 the next step, and
  so on (OCT- / OCT+ shift by a scale octave). A note goes in at the cursor with the set length, and the cursor moves
  on. Black keys: a rest; SEL + a black key: tie. On the DRUMS lane the first five white keys (F3 G3 A3 B3 C4) switch
  kick, snare, hat, kettle drum and crash at the cursor; a black key moves the cursor on.

Your patterns follow the chords, as the generated parts do: they are written in scale steps, not fixed notes.

## MIDI

USB-MIDI out: lead, pad and bass on channels 1 to 3, the drums on channel 10 (kick 36, snare 38, hat 42,
tom 47, crash 49). MIDI in: start, continue and stop.
