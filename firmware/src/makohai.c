/* SPDX-License-Identifier: GPL-3.0-only */
/* Makohai: a generative groovebox for the M-VAVE FM-1 in the style of early-90s PC tracker music.
 * Five lanes (lead, pad, bass, drums, sfx) play generated parts from a profile; you mix profiles live (queue,
 * blend bottom-up, launch on the phrase), shape each lane with filter, wobble, FM brightness, drive,
 * slide, retrigger, crush and echo, write your own parts step by step, set key, scale and chords, and let
 * an arc build the piece up from the drums to a drop. The sfx lane plays noise: risers into the drop, the
 * impact on it, washes, zaps, crackle and 8-bit noise.
 *
 * One compilation unit, as Dinghy: platform.c first (the FM-1: boot, USB, updates, panel, screen,
 * settings), then Makohai's parts:
 *   makohai_synth.c   the audio interrupt: FM voices, drum voices, lane effects, the step clock
 *   makohai_music.c   harmony, profiles, generators, density, the arc, your patterns, the bar cache
 *   makohai_ui.c      the screen, the pages, the panel, saving; app_main
 *
 * Rules of the chip (docs/RULES.md): integers only, no float, no 64-bit division. */
#define APP_NAME "MAKOHAI"
#ifdef DINGHY_HOST
static void makohai_host_stats(void);
#define HOST_STATS_HOOK makohai_host_stats
#endif
#include "platform.c"
#include "makohai_logo.h"

/* ---- shared between the main loop and the audio interrupt (one writer each) */
#define NLANE 6u
enum { L_LEAD = 0, L_PAD = 1, L_BASS = 2, L_DRUMS = 3, L_SFX = 4, L_ARP = 5 };   /* the arp lane last (saved lanes keep their place) */
#define HITS(l) ((l) == L_DRUMS || (l) == L_SFX)    /* lanes of hits (a kind per event), not of notes */

typedef struct {                /* written by the main loop, read by the audio interrupt */
    volatile int32_t filt;      /* -100..100: left = low-pass, right = high-pass, 0 = off */
    volatile int32_t res;       /* 0..100 */
    volatile int32_t wob;       /* 0 off, 1..5 = 1/1 1/2 1/4 1/8 1/16 */
    volatile int32_t bright;    /* 0..100: FM index; 25 = the patch as designed */
    volatile int32_t drive;     /* 0..100 */
    volatile int32_t slide;     /* 0..100: portamento, up to 0.3 s */
    volatile int32_t retrig;    /* 0..100: share of notes that stutter */
    volatile int32_t rtg_rate;  /* 1..3 repeats per 16th */
    volatile int32_t crush;     /* 0..100: bits + sample rate */
    volatile int32_t echo;      /* 0..100: two delayed copies, a dotted 8th apart */
    volatile int32_t level;     /* 0..100 */
    volatile int32_t inst;      /* -1 = AUTO (the profile's), else a patch (melodic), a kit (drums), one sound (sfx) */
    volatile int32_t vary;      /* 0..100: how far each phrase's character moves the effects (VARY) */
    volatile int32_t send;      /* 0..100: how much of the lane goes to the room (REVERB) */
    /* the tracker's effects (TRACKER) */
    volatile int32_t porta;     /* -100..100: every note keeps falling or rising, up to an octave a second (1xx / 2xx) */
    volatile int32_t vibr;      /* 0..100: vibrato from the start, up to half a semitone (4xy) */
    volatile int32_t trem;      /* 0..100: tremolo, the volume wobbling an 8th note a cycle (7xy) */
    volatile int32_t gate;      /* 0..100: notes cut short, at 100 to 15 % of their length (ECx) */
    volatile int32_t tremor;    /* 0 off, 1..3: the sound chopped on and off in 8ths, 16ths, 32nds (Ixy) */
    volatile int32_t swing;     /* 0..100: the off 16ths later, at 100 by a third of a step (EDx) */
    volatile int32_t pan;       /* -100..100: left .. right, equal power (8xx) */
    volatile int32_t aglide;    /* 0..100: how much of the style's own glides the lane keeps (GLIDES; 100 = all) */
    volatile int32_t autopan;   /* 0..100: each note on the other side of PAN, this far from it (AUTOPAN) */
} lanefx_t;
static lanefx_t fx[NLANE];      /* what plays: your settings plus the phrase's character (the audio interrupt reads it) */
static lanefx_t fxu[NLANE];     /* your settings (the encoders) */

/* one generated event: a note or chord (melodic lanes) or a drum hit */
typedef struct {
    uint8_t st, len, n, flags;  /* step 0..15, length in steps, notes in it, bit 0 = arpeggio */
    uint8_t rv;                 /* fixed random value 0..255 (decides which notes stutter) */
    uint8_t fx;                 /* your effect column: code (FX_x) in the high 4 bits, value 0..15 in the low */
    uint8_t note[3];            /* MIDI notes; drums: note[0] = drum kind; sfx: note[0] = kind, note[1] = pitch */
} ev_t;
#define EV_ARP 1u
#define EV_ACC1 2u              /* accents (the chopped pad): a step softer */
#define EV_ACC2 4u              /* two steps softer */
#define EV_GLIDE 8u             /* the note glides in from the lane's last one (a tracker's tone portamento) */
#define EV_SWELL 16u            /* the note swells in over most of its length (volume slide up) */
#define EV_FADE 32u             /* the note fades out over its length (volume slide down) */
#define EV_ORCH 64u             /* played on the orchestra hit, whatever the lane's sound (the styles' hits) */
/* the effect column of your own parts, as a tracker row has it: a code and a value 0..15 */
enum { FX_NONE, FX_VOL, FX_SLIDE, FX_UP, FX_DOWN, FX_VIB, FX_RETRIG, FX_CUT, FX_DELAY, NFX };
#define MAXEV 80u
typedef struct {
    int32_t num;                /* the bar this buffer holds (-1 = none) */
    uint8_t cnt[NLANE], prof[NLANE], muted[NLANE], inst[NLANE], user[NLANE];
    uint8_t crash_on_one;       /* the drop */
    ev_t ev[NLANE][MAXEV];
} bar_t;
#define NSLOT 8u
static bar_t bars[NSLOT][2] __attribute__((section(".pool")));
static volatile uint8_t bar_sel[NSLOT];

enum { DK_KICK = 0, DK_SNARE, DK_HAT, DK_KETTLE, DK_CRASH, DK_CLAP, DK_OHAT, DK_TOM, DK_RIM, NDK };   /* at most 16: a bit each */
enum { SX_WASH = 0, SX_RISER, SX_IMPACT, SX_ZAP, SX_CRACKLE, SX_BITS, SX_SURF, SX_GALE, NSX };   /* the sfx lane's sounds (at most 8: a bit each) */
#define SX_LONG(k) ((k) == SX_WASH || (k) == SX_RISER || (k) == SX_CRACKLE || (k) == SX_SURF || (k) == SX_GALE)   /* held over steps */

static volatile int32_t key_off;    /* the key: semitones from D, -6..5 (the kettle drum is tuned to it) */

#include "makohai_synth.c"
#include "makohai_music.c"
#include "makohai_ui.c"

#ifdef DINGHY_HOST
static void makohai_host_stats(void)            /* host mode: the audio interrupt's worst time and the frames drawn */
{
    printf("audio interrupt: worst %u us, average %u us of 5805 (on this computer)\n", isr_us_max, isr_n ? isr_us_sum / isr_n : 0);
}
#endif
