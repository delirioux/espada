Since 1.0:

- AUTOPAN and PAN RATE (TRACKER, after PAN): each new note on the other side of the lane's pan, or a slow sweep
  from side to side in time (8 bars to an 8th a cycle), as far as you set it.
- Settings and sets saved by 1.0 are read and kept.
- CRACKLE and BITS (sfx) at a tenth of their old level: far in the background.
- Pictures on SETUP's free knobs: SCOPE, LANES, STEREO (a goniometer), SPECTRUM (a landscape), and RESET's three
  knobs each picking one of CRYSTAL, TUNNEL, DOTS, PLASMA, TWIRL, STARS, FIRE, LOGO, SCOPE, SPECTRUM.
- Every lane starts at PAN 0; the lead and the arp at AUTOPAN 20.
- The keys light up the notes the marked lane plays (drums and sfx: their keys as they hit), and their octave follows
  the marked lane to where it plays.
- A voice budget (SETUP, VOICES: now, peak, LIMIT): all that sounds may cost at most LIMIT units, so echo on every
  lane can no longer make the sound late (a crack); past it the quietest fading voice stops first.
- BACK (SETUP): a picture behind the whole screen on every page, dimmed (10..60 %), CYCLE to the next one every
  4, 8 or 16 bars with a cut; saved with the look.
- DEMO (SEL + HOME): the pictures full screen while the music plays, cut on the bar every 4, 8 or 16 bars with
  flashes, blinds, wipes, dissolves, an iris, dips and pushes; a drop cuts with a flash.
- A new identity, FM-1_47625642 (1.0 reported FM-1_960, as another firmware does), the same for every version: the FM-1 Firmware Hub tells Makohai apart by it.

Makohai 1.0 (formerly Espada): the first release of a generative groovebox for the M-VAVE FM-1 in the style of early-90s PC tracker music.

- Six lanes (lead, arp, pad, bass, drums, sfx) with FM voices, five synthesized drum kits of nine hits (kick, snare, hat, open hat, clap, tom, rim shot, kettle drum, crash) and eight noise effects (wash, surf, gale, crackle, bits, impact, zap, riser); notes scroll past a fixed play line.
- Ten styles: EPIC, HYPERSPACE, FUSION, OLDSKOOL, SYMPHONY, TROPIC, STEEL, DRIFT, FUNK, RAVE, each with its chords, parts, instruments, tempo and way of playing. It starts with OLDSKOOL. Queue, blend lane by lane, launch at once, lock a lane. Every change is heard at once.
- An arp lane: arpeggios through the chord that plays, each style its own pattern and sound.
- Every style plays the tracker's effects its own way, note by note: vibrato, slides, dives, cut-off stabs, volume slides, retriggered fills, swing, a trance gate, wide stereo, orchestra hits.
- Played as in a tracker: lead notes glide in, long chords swell and fade, an echo channel on the lead; strings with bow noise, darker and longer snares and cymbals.
- One screen as a mixing desk: a group button (PLAY, SOUND, TRACKER, SONG), PRESETS through its settings, KNOB 1-4 the setting for four lanes side by side; ALGORITHM slides that window over all six lanes.
- MELODY, RHYTHM, PITCH and CHANGE per lane. The lead plays 16-bar arcs of two 8-bar periods, closed on the root.
- Instruments including piano, strings, violins, cellos, brass, a supersaw, a choir (AH VOX), an orchestra hit and a chaos instrument; stereo voices and a room (SPACE).
- Density from sparse to busy, inside the chord and its scale; ENERGY for all lanes at once.
- Per lane: filter with a tempo-synced wobble, FM brightness, drive, crush, volume; slide, retrigger, echo, porta up and down, vibrato, tremolo, tremor, gate, swing and an equal-power pan; VARY changes the character per phrase.
- ENV fades a lane out and in; VOLUME sets it.
- Your own parts on the EDIT page, up to 512 bars a lane, entered in scale steps so they follow the chords; live RECORD, an effect column per step (volume, slide, porta, vibrato, retrigger, cut, delay), undo, transpose, shift, bars in and out, copy and paste ranges, FILL a bar from any style, REPEAT, COPY from another lane, GRAB what plays; chords in key on the pad keys; after your last bar LOOP, PROFILE or MY STYLE (new bars learned from yours).
- SONG: key, scale, suggested progressions, bars per chord.
- Song form by hand: an INTRO from one lane, INTERs (one lane alone, the rest back one by one), DROPs and an END that fades the lanes out to bass or drums and stops.
- The arc (off until you turn it on): builds from the drums, a break, the drop, and again; DROP by hand (tap or hold).
- USB-MIDI out (lead, pad and bass on channels 1 to 3, the arp on 4, drums on 10, all settable), MIDI start / stop in.
- Eight sets (SONG, SETS): everything but your parts, loaded at once, also while playing.
- SETUP (hold HOME): six palettes, the screen's level, the lights' glow, MIDI channels, back to defaults.
- Playable in the browser: the same code, compiled to WebAssembly.

Install it over M-VAVE's official firmware (V15). Hold OCT- while switching on for the USB rescue.
