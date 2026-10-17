/* SPDX-License-Identifier: GPL-3.0-only */
/* Espada: a generative groovebox for the M-VAVE FM-1 in the style of early-90s PC tracker music.
 * Five lanes (lead, pad, bass, drums, sfx) play generated parts from a profile; you mix profiles live (queue,
 * blend bottom-up, launch on the phrase), shape each lane with filter, wobble, FM brightness, drive,
 * slide, retrigger, crush and echo, write your own parts step by step, set key, scale and chords, and let
 * an arc build the piece up from the drums to a drop. The sfx lane plays noise: risers into the drop, the
 * impact on it, washes, zaps, crackle and 8-bit noise.
 *
 * One compilation unit, as Dinghy: platform.c first (the FM-1: boot, USB, updates, panel, screen,
 * settings), then Espada's parts:
 *   espada_synth.c   the audio interrupt: FM voices, drum voices, lane effects, the step clock
 *   espada_music.c   harmony, profiles, generators, density, the arc, your patterns, the bar cache
 *   espada_ui.c      the screen, the pages, the panel, saving; app_main
 *
 * Rules of the chip (docs/RULES.md): integers only, no float, no 64-bit division. */
#define APP_NAME "ESPADA"
#ifdef DINGHY_HOST
static void espada_host_stats(void);
#define HOST_STATS_HOOK espada_host_stats
#endif
#include "platform.c"
#include "espada_logo.h"

/* ---- shared between the main loop and the audio interrupt (one writer each) */
#define NLANE 5u
enum { L_LEAD = 0, L_PAD = 1, L_BASS = 2, L_DRUMS = 3, L_SFX = 4 };
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
} lanefx_t;
static lanefx_t fx[NLANE];

/* one generated event: a note or chord (melodic lanes) or a drum hit */
typedef struct {
    uint8_t st, len, n, flags;  /* step 0..15, length in steps, notes in it, bit 0 = arpeggio */
    uint8_t rv;                 /* fixed random value 0..255 (decides which notes stutter) */
    uint8_t note[3];            /* MIDI notes; drums: note[0] = drum kind; sfx: note[0] = kind, note[1] = pitch */
} ev_t;
#define EV_ARP 1u
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

enum { DK_KICK = 0, DK_SNARE, DK_HAT, DK_KETTLE, DK_CRASH, NDK };
enum { SX_WASH = 0, SX_RISER, SX_IMPACT, SX_ZAP, SX_CRACKLE, SX_BITS, NSX };   /* the sfx lane's sounds */

static volatile int32_t key_off;    /* the key: semitones from D, -6..5 (the kettle drum is tuned to it) */

#include "espada_synth.c"
#include "espada_music.c"
#include "espada_ui.c"

#ifdef DINGHY_HOST
static void espada_host_stats(void)            /* host mode: the audio interrupt's worst time and the frames drawn */
{
    printf("audio interrupt: worst %u us, average %u us of 5805 (on this computer)\n", isr_us_max, isr_n ? isr_us_sum / isr_n : 0);
}
#endif
