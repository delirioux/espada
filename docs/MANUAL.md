# Playing Makohai

## The screen

One screen, always the same: a mixing desk.

- The **header** names the group you are in (PLAY, SOUND, TRACKER, SONG, or EDIT and its lane), with four boxes for
  the bar in the phrase and the tempo.
- The **title** shows the setting in large letters, dots for where it sits in its group, and on the right what is
  about to happen (LAUNCH 3, DROP IN 2), the lane you act on when it is faded out or locked, or the queued style. Beside the group the header names the
  style playing.
  Messages appear here for a moment.
- The **lanes**: all six, always (LEAD, ARP, PAD, BASS, DRUMS, SFX, top to bottom). Notes scroll past the white "now" line; the past is dimmed. Solid blocks play
  now; outlined blocks show what the queued style will play. A faded-out lane is dark, a locked lane's name
  inverted. The white bar left of a name marks the lane the buttons and keys act on. (SONG's KEY row shows the chords
  here instead, its ARC row the arc, EDIT the step grid.)
- The **knobs**: KNOB 1 to 4, in the order they sit on the panel. In PLAY, SOUND and TRACKER they are four lanes side
  by side, each with its value for the setting in the title, as on a mixer: a window over the six lanes, in the
  screen's order. ALGORITHM moves the marked lane and the window follows it: LEAD ARP PAD BASS, then ARP PAD BASS
  DRUMS, then PAD BASS DRUMS SFX. The name over each knob says which lane it is. In SONG they are four settings of
  their own. A bar shows a share, a word under it what the
  value means. "-" means the setting does nothing on that lane.

## Knobs

| Knob | Does |
| --- | --- |
| SELECT | the tempo, everywhere |
| PRESETS | the setting: steps through the group's settings |
| ALGORITHM | the lane the buttons and keys act on (LEAD, ARP, PAD, BASS, DRUMS, SFX, in the screen's order) |
| KNOB 1 .. 4 | the setting for the four lanes in the window, as their names show (SONG: the four settings shown) |

Turning a lane's knob also makes it the lane the buttons act on. Hold **SEL** to turn in steps of 10.

## The settings

A group is a button; pressed again, it steps to its next setting, as PRESETS does.

| Group | Button | Settings (KNOB 1..4 = the four lanes in the window) |
| --- | --- | --- |
| PLAY | HOME | volume, style, sound, melody, rhythm, pitch, density, octave, change |
| SOUND | FX | reverb, filter, resonance, wobble, brightness, drive, crush, vary |
| TRACKER | ARP | pan, autopan, pan rate, swing, echo, slide, glides (the lead: how many of the style's own glides it keeps), porta, vibrato, tremolo, tremor, gate, retrigger, rate |

| SONG (GLO) | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| LIVE | style: playing, the next one; stopped, every lane at once | blend | energy | space (the room) |
| KEY | key | scale | chords | bars per chord |
| ARC | arc on / off | build | peak | auto drop |
| SETS | set 1..8 (its style, or EMPTY) | load: turn right, the set at once | store: turn right, what plays now into the set | clear: three clicks right |

| EDIT | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| NOTES | step in the bar (1..16) | bar (1 .. your last + 1) | the length a note goes in with | undo: turn, one step back |
| FX | step | bar | effect at the step: VOL, SLIDE, UP, DOWN, VIB, RETRIG, CUT, DELAY | its value, 0..15 |
| PART | pattern (yours or the style's) | after your last bar: LOOP, PROFILE or MY STYLE | fill (the style LFO fills from) | copy (the lane SEQ copies from) |
| BAR | repeat: the bar before copied here, on to the next | transpose: the bar by scale steps | shift: the bar round by 16ths | bars: right = an empty bar in here, left = this bar out |
| RANGE | from bar | to bar | paste: those bars here | clear: three clicks right, those bars cleared |

## Setup

Hold **HOME** for SETUP; HOME (or any group button, or EDIT) leaves it. PRESETS steps through its rows:

| SETUP | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| LOOK | palette: MAKOHAI (its colours), AMBER, GREEN, CYAN, RED, MONO | screen: 100, 75, 50 or 25 % | lights: the keys' and buttons' glow on or off | SCOPE (a picture) |
| MIDI | the lead's channel | the pad's | the bass's | the drums' (1..16 or OFF) |
| MIDI 2 | the arp's channel | LANES (a picture) | STEREO (a picture) | SPECTRUM (a picture) |
| RESET | defaults: five clicks to the right puts everything back as it comes (patterns cleared); SAVE keeps it | COPPER (a picture) | STARS (a picture) | MAKO (a picture) |
| SYSTEM | stack: free, of 23 K (the least it has had free since power-on) | the interrupts' stack: the same, of 7 K | RAM free | the pool free (the big buffers) |
| AUDIO | the audio interrupt's worst time, µs | its average over the last half second | the worst as a share of the 5805 µs it has | any knob here starts the worst again |

The pictures fill the knobs SETUP has no use for; turning those knobs does nothing. **SCOPE** draws the output's
wave, held still on a rising zero as a tracker's scope does. **LANES** is a level meter per lane in its colour,
**STEREO** the left and right output with a dot for the balance (it follows AUTOPAN and PAN RATE), **SPECTRUM** what
sounds by pitch, from the bass on the left to the hats on the right. **COPPER** moves three raster bars and flashes
them on the drums, **STARS** flies through a starfield at the tempo, and in **MAKO** a fin crosses the sea, bobbing
on the bass.

SAVE keeps the setup with everything else. SYSTEM and AUDIO only show; they are how a firmware that runs short of
memory or time is caught before it crackles or crashes. In the browser, SYSTEM shows N/A (there is no FM-1 memory
there).

## Buttons

Every button does one thing, everywhere. ENV, LFO, REC, EDIT and the keys act on the marked lane.

| Button | Does |
| --- | --- |
| HOME, FX, ARP, GLO | the groups PLAY, SOUND, TRACKER, SONG; pressed again: the next setting. From EDIT: back. Hold HOME: SETUP |
| EDIT | the lane's own part, step by step, bar by bar (again: back); PRESETS through its five pages. SEL + EDIT: clear this bar (the last bar goes) |
| PLAY | stopped: start from the top with an INTRO (SEL + PLAY: everything at once). Playing: the END (SEL + PLAY, or PLAY again during the END: stop now) |
| SEQ | LAUNCH: every unlocked lane moves to the queued style at once, at the style's tempo. SEL + SEQ: launch and keep your tempo. On EDIT: COPY the whole part of the lane set on EDIT 2 into this one |
| REC | while playing, the marked lane alone at once. Tap = INTER (it plays alone for two bars, then the others come back one by one); hold = DROP (alone while held, everything back the moment you let go, with a crash and an impact). On EDIT: RECORD on or off (what you play goes into your part); SEL + REC: GRAB (what the lane plays now goes into this bar) |
| ENV | fade the lane out over a bar; again: back in. SEL + ENV: LOCK (it keeps its style when you blend or launch). On EDIT: TIE |
| LFO | a new idea for the lane, at once (MELODY moves on). On EDIT: GENERATE this bar: with FILL on OWN a variation of the one there (SEL + LFO: a fresh one), with FILL on a style a fresh one in that style |
| OCT- / OCT+ | the keys' octave; both together: back to 0. On EDIT: the octave you enter in |
| SAVE | keep everything (settings and your parts); while playing it waits until you stop. SEL + SAVE: the audio interrupt's time in the header (also SETUP, AUDIO) |

Every button and key glows faintly; lit means: the group you are in, EDIT, playing, a break, a launch waiting, the
lane faded out, a save waiting, the keys' octave off 0.

## What the knobs do

- **The musical settings** (PLAY: MELODY, RHYTHM, PITCH, and CHANGE at the end): **MELODY** picks the idea: every position is its own melody, riff,
  voicing or drum pattern, and turning back brings one back. Without a saved set every power-on sets each lane's
  MELODY at random, so Makohai starts with a new song each time; after a SAVE it comes back as you saved it. **RHYTHM** goes from slow long notes to busy 16ths (on
  the other lanes: sparser or busier). **PITCH** goes from small steps to wide leaps and a higher register (the bass
  jumps octaves, the pad's chords open up, the drums bring in the timpani). **CHANGE** says how soon new material
  comes: FROZEN (the same 16 bars over and over), 64, 32 or 16 bars. Every change is heard at once, from the next
  16th: the rest of the bar is written again.
- **Long arcs.** The lead thinks in 16-bar sections of two 8-bar periods: the motif stated, answered upside down,
  sequenced a step up and closed open on the third or fifth; then stated again, driven to the period's highest
  notes and closed on the root. The second period climbs higher. Every second time round the progression comes as a
  B section (its pairs swapped), so 8 chords make a 16-chord form.

- **Energy** (SONG, LIVE) adds to or takes from every lane's density at once: one knob for a build or a breakdown.
- **Space** (SONG, LIVE) is the room, a stereo reverb, and how loud it is. **Reverb** (SOUND) is how much of each lane goes into it. It starts with the sfx all the way in (100), the pad much (60),
  the lead some (35), the drums a little (18) and the bass hardly (3). VOLUME starts at 80, the arp at 60, the lead at
  50, the sfx at 30.
- **The arc** is off until you turn it on (SONG, ARC, knob 1). On: it starts with a few drum hits and builds bass,
  pad and lead in as the energy rises, breaks, drops and builds again; the ARC row draws it. DROP works without it.
- **Blend** moves lanes to the queued style from the bottom up: drums, then bass, pad, lead (the word under it
  says how far).
- **Density:** 50 % is the part as written. Lower thins it; higher adds notes, hats, ghost snares and fills.
- **Sound** (PLAY): AUTO follows the style; or one of twenty patches (FM BASS, SLAP, PIZZ, SUB, BRASS,
  STRINGS, BELL, ORGAN, FLUTE, CHIP, RAZOR, PULSE, PIANO, E.PIANO, CHAOS, VIOLINS, CELLOS, SUPERSAW, AH VOX, ORCH HIT), or one of
  five drum kits (STANDARD, TRACKER, ELECTRO, DEEP, PUNCH). AH VOX is a choir's "aah": two voices a little apart, a
  breath, through the vowel's formants, swelling in slowly; DRIFT's pad sings it. PUNCH is the sharp kit: the kick
  clicks, the snare keeps its crack, the hats are bright. SUPERSAW is not FM: seven saws detuned around the note, wide and bright,
  for big chords and leads. ORCH HIT is the orchestra hit: one key plays the note with its fifth, octave and tenth
  above, brassy, with a burst of noise on the attack, gone in half a second; for stabs on the lead or the pad. DEEP has a kick that drops to about 33 Hz and rings long, with a dry kit around it; RAVE
  plays on it, and the styles use the orchestra hit in their own places (below); SUPERSAW is RAVE's arp. (A small speaker hardly plays DEEP's kick: use headphones or the output.) A kit has nine hits: kick, snare, hat, kettle drum, crash, clap (three quick bursts and a
  tail), open hat (the closed hat cuts it off), tom (toms in a row step down, as in a fill) and rim shot. RAZOR is a cutting, saw-like lead; PULSE a hollow one; PIANO and E.PIANO ring out. CHAOS is
  three sounds in one: a light saw lead, a heavy pad under it (two detuned oscillators, one an octave down, fading
  in slowly and out late) and brown noise that breathes with the pad. STRINGS, VIOLINS, CELLOS, BRASS and FLUTE carry
  a little noise in the tone (bow, breath), as recorded ones do; the kits' snares and cymbals ring darker and longer.
- **Vary:** each 4-bar phrase gives the lane a character on top of your effects: CLEAN, ECHO, SLIDE, BITE
  (brightness and drive), FILTER (a low-pass with resonance) or STUTTER (retrigger). VARY sets how far it reaches;
  the value shows the phrase's character. The lead starts at 40 %, the other lanes at 0. Your own settings stay as
  you set them; the lane on screen shows what plays.
- **The tracker's effects** (TRACKER), per lane, as the effect column has them:
  - **PORTA** (1xx / 2xx): every note keeps rising (right) or falling (left) while it sounds, up to an octave a second.
  - **VIBRATO** (4xy): the pitch wobbles from the start of each note, up to half a semitone, 5.6 times a second.
  - **TREMOLO** (7xy): the volume wobbles, a cycle every 8th note, in time with the beat.
  - **TREMOR** (Ixy): the sound chopped on and off in 8ths, 16ths or 32nds: the trance gate.
  - **GATE** (ECx): notes cut short, at 100 % to 15 % of their length: staccato.
  - **SWING** (EDx): the off 16ths come later, at 100 % by a third of a step (triplet feel); a lane each, so the hats
    can swing while the bass stays straight.
  - **PAN** (8xx): the lane from left to right, with equal power: a lane sounds as loud wherever you put it, and in
    the middle exactly as before. The pad's and the sfx's voices keep their spread around it, the hats stay right
    of the drums. The lead starts a little right, the arp a little left.
  - **AUTOPAN**: how far the lane moves away from PAN, to either side (100: from hard left to hard right, around
    the middle). How it moves is PAN RATE.
  - **PAN RATE**: NOTE puts each new note on the other side, left, right, left: a chord moves as one, and so do
    the hits that fall together on the drums; the echo and the retriggers bounce too. 8 BARS, 4 BARS, 2 BARS, 1/1,
    1/2, 1/4 and 1/8 sweep the lane slowly from side to side instead, a cycle that long, in time with the beat.
    Either way the sound glides across, so nothing clicks.
  Porta, vibrato and gate act on notes, so the drums and the sfx show "--" for them.
- **Filter:** one knob, left = low-pass, right = high-pass, centre = off. **Wobble** moves the cutoff in time with the
  tempo (1/1 to 1/16). On screen the filter darkens what it cuts away.
- **Brightness** raises the FM modulation: from a soft sine toward a buzzy saw. **Drive** saturates.
- **Slide** glides each note in from the last one. **Retrigger** makes a share of the notes stutter (always the same
  notes, so it stays a pattern); the rate sets 1 to 3 repeats per 16th. **Crush** lowers the bits and the sample rate.
  **Echo** plays two quieter copies a dotted 8th apart.
- **Stereo:** as a tracker spreads its channels left and right, Makohai spreads its voices: the pad's and the sfx's
  every other voice to the right, hats and crash right of centre, the lead a little right, bass and kick in the
  middle. STRINGS and BRASS play as sections (two more players a little out of tune) and the strings swell in.
  HYPERSPACE's pad is chopped: the chord struck every 16th with an accent pattern, as a tracker re-strikes a long
  pad sample.
- **The form.** A piece has an INTRO, INTERs and an END, played by hand:
  - **INTRO** (PLAY): pad, drums or bass starts alone, chosen at random; the other two follow two bars apart, then
    the lead with the arp, and the sfx. All in after six bars. While Makohai is stopped the lanes already show the INTRO that
    PLAY will start; each stop picks a new one.
  - **INTER** (a tap on REC): the marked lane alone for two bars, then the others back one by one, a bar apart, in a
    random order.
  - **DROP** (holding REC): the marked lane alone for as long as you hold; everything back at once when you let go.
  - **END** (PLAY while playing): lead (with the arp), sfx and pad fade out one by one, a bar apart, then bass or drums; the one
    left (chosen at random) plays alone two bars and fades last. Then Makohai stops. The title says INTRO, INTER or
    END while one is running.
- **The ten styles.** Each is a chord progression, a part for each lane, an instrument for each, a tempo, and a way
  of playing. Change the mood with STYLE (SONG, LIVE) and SEQ. Without a saved set Makohai starts with OLDSKOOL.

| Style | Tempo | What it is |
| --- | --- | --- |
| EPIC | 125 | orchestral minor: long lead lines over strings, a driving bass, timpani, a tom fill every other phrase |
| HYPERSPACE | 130 | techno minor: power chords, a chopped pad, chord stabs, straight drums, a clap every other bar |
| FUSION | 125 | melodic major with a chromatic turn: a riff through every chord, a pedal bass, lush chords, the hat opening into the one |
| OLDSKOOL | 125 | the 4-channel sound: arpeggio chords, an octave bass, a busy lead with an echo |
| SYMPHONY | 110 | the orchestra: two bars a chord, swelling strings, a violin line, cellos, timpani |
| TROPIC | 110 | sunny mixolydian: chords on the off-beat, a deep bass with rests, rim shots, congas, a flute |
| STEEL | 140 | heavy: power chords, a galloping bass, double kick, tom fills, a saw lead that glides |
| DRIFT | 96 | floating: sus chords resolving, two bars a chord, swells, a breathy lead with a long echo, no snare, a soft rim |
| FUNK | 112 | a major vamp with a diminished passing chord: slap bass, clav stabs, ghost notes, an open hat, a rim |
| RAVE | 138 | four on the floor, claps, open hats: an acid bass that slides, organ chords, brass stabs, gusts of noise |

- **How a style plays.** As a tracker musician plays the notes by hand: some lead notes glide in from the last one
  (STEEL most, DRIFT and FUSION often), long chords swell in or fade out (DRIFT and SYMPHONY most), and the lead
  gets an echo, a quieter copy three 16ths later (DRIFT, OLDSKOOL, RAVE). The style's echo adds to the lane's ECHO.
- **The styles use the tracker's effects** too, mostly note by note in their own effect column, as the modules'
  composers did: on every drop, in every style, the orchestra hits (also on DROP by hand); EPIC's long lead notes sing with vibrato and an orchestra hit opens each phrase and lands the drop;
  HYPERSPACE cuts its stabs short and dives the bass into the phrase; FUSION slides some lead notes in and swings the
  drums a little; OLDSKOOL does the 4-channel tricks (vibrato and slides on the lead, volume slides on the arpeggio
  chords, a retriggered snare fill); SYMPHONY gives its strings vibrato and an orchestra hit at the climax (the drop,
  or every 16 bars with the arc off); TROPIC swings drums and arp and spreads lead and arp wide; STEEL dives the lead
  off the phrase's last note and chugs the bass; DRIFT gives the pad a slow tremolo and the lead vibrato, spread wide;
  FUNK swings the drums, plays the clav and bass short and slides the bass; RAVE chops the pad in 16ths (a trance
  gate), hits the orchestra on the drop (every 8 bars with the arc off), on the DEEP kit with a supersaw arp. All of
  it comes on top of your settings: your knobs show yours, the lanes play both.
- **SYMPHONY** is the orchestra: two bars a chord in minor with a major dominant before the
  return; strings in open voicing (root, fifth, tenth) with a suspension resolving on each new chord and the inner
  voice moving on the last bar; a legato violin line in long arcs; cellos and basses holding the root and walking
  down into the next chord; timpani on the one with a roll into each phrase, a bass drum every other bar, a cymbal
  on the phrase and a cymbal swell (sfx) before it; a march snare when the energy is up.
- **A lane's style** (PLAY) sets that lane alone; the queued style (SONG, LIVE) is the one blend and launch move to.
  While Makohai is stopped there is nothing to blend into: SONG's STYLE then switches every lane (not the locked ones)
  at once, with the style's tempo.
- **The lead** follows the chords: every beat is a chord tone, the notes between lead into the next one by scale
  steps, and each 4-bar phrase states a motif, answers it, varies it and lands on the root or third. Density adds a
  16th into a beat, never a stray note.
- Every setting acts at once: the music changes from the next 16th (a note already sounding plays on). LAUNCH and
  DROP too. Only ENV takes a bar: it fades.

## ARP

The sixth lane plays arpeggios: the notes of the chord that is playing, one after another, so it always fits the
harmony and moves with every chord change. Each style brings its own: EPIC plucks up and down, HYPERSPACE a pulse
going up, FUSION bells in broken thirds, OLDSKOOL the chip arpeggio, SYMPHONY a harp-like pizzicato in 8ths, TROPIC
an Alberti figure on electric piano, STEEL a saw running down, DRIFT slow bells, FUNK a random electric piano, RAVE
a supersaw going up.

Its settings are on the knobs as every lane's (ALGORITHM to ARP, and the window shows it). **MELODY** steps to the next pattern: UP, DOWN, UP-DOWN, ALBERTI (low, high, middle,
high), BROKEN (in thirds) and RANDOM. **RHYTHM** makes it faster or slower (16ths, 8ths, quarters), **PITCH** wider or
narrower (1 to 3 octaves), **DENSITY** leaves out off-beat notes or throws in octave jumps, **CHANGE** moves to a new
pattern every 16 bars. The pattern runs on across bar lines, so a chord change lands in the middle of a run, as an
arpeggiator does. Like every lane it can have a part of your own on EDIT, fade with ENV and take the sound of any
patch.

## SFX

The SFX lane plays noise, no notes. Its sounds: **WASH** (filtered white noise with a slow sweep), **RISER**
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
Turn **SOUND** (PLAY, the SFX knob) to one of the eight to hear only that one, in its own pattern. SOUND and TRACKER
give it the effects of any lane (a filter sweep over a wash, echo on the zaps). On EDIT the first eight white keys (F3 .. F4)
switch WASH, RISER, IMPACT, ZAP, CRACKLE, BITS, SURF and GALE at the cursor.

## Keys

- The keys play the marked lane, live, from F3 to G5 shifted by OCT- / OCT+: a note
  sounds as long as you hold it, with the lane's sound. On DRUMS the first nine white keys (F3 .. G4) play kick,
  snare, hat, kettle drum, crash, clap, open hat, tom and rim shot; on SFX the first eight its sounds. The keys of the tonic are lit, and the keys you hold.
- On PAD a key is a chord in key: the triad on that note in the song's scale (in C major: C, Dm, Em, F, G, Am, ...);
  SEL + a key the sus4. Your pad parts and recordings play the same chords.
- They change no settings: the key of the music is SONG, KEY.
- On EDIT the white keys are scale steps counted from the chord root: C4 is the root, D4 the next step, and
  so on (OCT- / OCT+ shift by a scale octave). A note goes in at the cursor with the set length, and the cursor moves
  on. Black keys: a rest; SEL + a black key: tie. On the DRUMS lane the first nine white keys (F3 .. G4) switch
  kick, snare, hat, kettle drum, crash, clap, open hat, tom and rim shot at the cursor (the rows K S H T C X O M R);
  a black key moves the cursor on.

Your patterns follow the chords, as the generated parts do: they are written in scale steps, not fixed notes.

## Your own parts

Each lane can have a part of its own, as long as you write it: up to 512 bars. EDIT shows one bar at a time, with a
band over it: the page (NOTES, FX, PART, BAR, RANGE; PRESETS steps through them) and a map of your bars, the one you
are on white, the one playing marked cyan. BAR moves through your bars and one more, the new bar after the last (the
grid says NEW BAR). Writing there makes the part a bar longer; you cannot go further, so a part never has holes at
its end, and at 512 bars BAR says FULL and there is no new bar. SEL + EDIT clears the bar you are on (on the last bar
it goes, and the part is a bar shorter); RANGE, CLEAR from bar 1 to the last clears the whole part.

**Recording.** On EDIT, REC switches RECORD on (the band shows a red light). While Makohai plays, the keys then play
the lane as usual and what you play goes into your part at the bar playing, on the nearest 16th; a note you hold
gets its length when you let go. Past your last bar the part grows by the bar right after it (so recording from the
song's start writes bar after bar); further on it goes round your bars, so you can overdub a loop. What you record
is heard from the next bar on. REC again stops it; UNDO takes a whole recording back.

**The effect column** (page FX), as a tracker row has it: each step can carry one effect with a value 0..15.
**VOL** sets the note's volume (0 = a 16th, 15 = full), **SLIDE** glides into it from the note before, **UP** and
**DOWN** make it rise or fall while it sounds, **VIB** gives it vibrato, **RETRIG** repeats it every value/6 of a
step, **CUT** stops it after value/4 of a step, **DELAY** starts it value/16 of a step late. The grid shows the
effect's letter under its step. The effects work on drums and sfx as well (volume, retrigger, delay).

**The tools.** UNDO (NOTES) takes the last change back: a step, a fill, a paste, a recording. BAR: REPEAT copies the
bar before into this one and moves on, a bar a click; TRANSPOSE moves the bar's notes by scale steps; SHIFT turns it
round by 16ths; BARS puts an empty bar in here (right) or takes this one out (left). RANGE: FROM and TO pick bars,
PASTE copies them here (over what is there, the part growing if it has to), CLEAR empties them. LFO writes this bar
from a style (FILL) or varies it; SEL + REC grabs what the lane plays; COPY (SEQ) takes a whole part from another lane.

Your part plays from the song's first bar: bar 1 of yours with the song's bar 1, and so on. AFTER (PART) says what
comes after your last bar:

- **LOOP**: your part again from its bar 1.
- **PROFILE**: the lane's generated part.
- **MY STYLE**: new bars in your manner. Makohai counts what your bars do at each place in the 4-bar phrase: which
  16ths have notes (or which drums), which scale-step jumps you make, how high and low you go. Each new bar takes one
  of your bars at the same place in the phrase, keeps most of its rhythm (or draws it from your counts), keeps some
  of its notes and moves the rest by your kind of jumps; the last bar of each phrase closes on your note. Drums keep
  most of your hits there and add the ones you often play. The more bars you write, the more it sounds like you.

SAVE keeps all parts (in a part of the FM-1's flash Makohai has to itself: every write is checked against it, and the
whole 6 x 512 bars with their effects always fit). A save of long parts takes up to two seconds; it waits until you
stop, as always. Parts saved before the effect column came in are not read: enter them again.

## MIDI

USB-MIDI out: lead, pad and bass on channels 1 to 3, the arp on 4, the drums on channel 10 (kick 36, snare 38, hat 42,
kettle drum 47, crash 49, clap 39, open hat 46, toms 50 48 47 45 43 as they step down, rim shot 37); SETUP, MIDI changes the channels or turns a lane off. MIDI in: start, continue and stop.
