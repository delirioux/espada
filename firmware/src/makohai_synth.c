/* SPDX-License-Identifier: GPL-3.0-only */
/* Makohai's sound: runs in the audio interrupt (44.1 kHz, 256 frames a call). Integers only.
 *
 *   FM voices     two operators (a modulator into a carrier's phase), an index envelope, an amplitude
 *                 envelope, slide, vibrato, the tracker arpeggio (a chord cycled every 42 ms), square option;
 *                 CHAOS layers a light saw lead, a heavy pad (two detuned oscillators, one an octave down,
 *                 fading in slowly and out late) and brown noise breathing with the pad; SUPERSAW is seven saws,
 *                 detuned around the note, through a soft high cut
 *   drum voices   kick and kettle drum (a swept sine), snare (band-passed noise + a tone), hat, crash, clap (three
 *                 quick bursts of band-passed noise and a tail), open hat (choked by the closed one), tom (a swept
 *                 sine; hits in a row step down, as a fill does), rim shot (two sines and a click)
 *   sfx voices    wash (filtered noise, a slow sweep), riser (noise opening up + a tone climbing four octaves),
 *                 impact (a falling sub + a noise burst), zap (a sine falling fast), crackle (clicks + hiss),
 *                 bits (1-bit periodic noise, the 8-bit metallic kind), surf (brown noise in slow waves, brighter
 *                 on each crest), gale (white noise through a resonant band-pass that wanders, in gusts)
 *   lanes         each lane renders into its own bus: drive, a state-variable filter (low- or high-pass)
 *                 with a tempo-synced wobble, the bit crusher, the level; then the mix, in stereo: each lane
 *                 has its place (as a tracker's channels sit left and right) and a send to the room
 *   the room      a small stereo reverb (four damped combs, then two allpasses a side, different left and
 *                 right) at half the sample rate, fed by the lanes' sends; SPACE sets how much
 *   the clock     16ths at the tempo, sample-accurate; each step fires the events of the bar cache; retrigger
 *                 repeats and echo copies wait in a small queue with their sample time */

/* ---- tables (filled at boot) */
static int16_t SIN[1024];
static int32_t sine_q15(uint32_t ph)            /* Dinghy's parabola sine, for the table */
{
    int32_t x = (int32_t)ph >> 16, ax = x < 0 ? -x : x, y, ay;
    y = (x * (32768 - ax)) >> 13;
    ay = y < 0 ? -y : y;
    y += ((((y * ay) >> 15) - y) * 7373) >> 15;
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}
static const uint32_t INC_C9[12] = {            /* MIDI 120..131; lower octaves shift right */
    815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u,
    1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u};
static uint32_t note_inc(int32_t n)             /* phase step per sample of MIDI note n */
{
    if (n < 0) n = 0;
    if (n > 119) n = 119;
    return INC_C9[n % 12] >> (10 - n / 12);
}
#define HZ_INC(hz) ((uint32_t)(hz) * 97392u)    /* 2^32 / 44100 per Hz */

/* ---- the operators' waveforms: the sine; the FM chip's (half, rectified, its quarter pulses, the alternating sine,
 * the camel, a square, a saw); then single cycles drawn at boot, 8-bit and 256 points each, as a tracker's */
enum { W_SINE, W_HALF, W_ABS, W_QUART, W_ALT, W_CAMEL, W_SQR, W_SAW,
       W_TRI, W_P25, W_P12, W_ORGAN, W_VOXA, W_VOXO, W_REED, W_BRIGHT,
       W_HOLLOW, W_GLASS, W_STRING, W_CLAV, W_BASSY, W_NASAL, W_SYNC, W_STEPTRI, NWAVE };
#define W_TAB W_TRI
#define NWT (NWAVE - W_TAB)
static int16_t WV[NWAVE][256] __attribute__((section(".pool")));   /* every waveform as a table: one lookup a sample,
                                                                     * as cheap as the sine (12 KB) */
static int16_t WDC[NWAVE], WLOUD[NWAVE];        /* each wave's mean (the carrier takes it off: no thump), its loudness */
#define WVQ(w, ph) ((int32_t)WV[w][(ph) >> 24]) /* the audio interrupt's lookup, Q15 */
static int32_t wv(uint32_t w, uint32_t ph)      /* Q15, drawn at boot into WV */
{
    uint32_t i = ph >> 22;
    int32_t x;
    switch (w) {
    case W_SINE: return SIN[i];
    case W_HALF: return i < 512u ? SIN[i] : 0;
    case W_ABS: x = SIN[i]; return x < 0 ? -x : x;
    case W_QUART: return (i & 511u) < 256u ? SIN[i & 255u] : 0;
    case W_ALT: return i < 512u ? SIN[(i << 1) & 1023u] : 0;
    case W_CAMEL: x = SIN[(i << 1) & 1023u]; return i < 512u ? (x < 0 ? -x : x) : 0;
    case W_SQR: return i < 512u ? 26000 : -26000;
    case W_SAW: return (int32_t)(ph >> 16) - 32768;
    default: return WV[w % NWAVE][ph >> 24];                         /* the drawn ones: drawn into the table first */
    }
}
/* the drawn waves: harmonics 1..16 (0..255 each; a negative one starts upside down), or drawn point by point */
static const int16_t WHARM[NWT][16] = {
    {255, 0, -28, 0, 10, 0, -5, 0, 3, 0, -2, 0, 2, 0, -1, 0},                      /* TRI: odd, 1/n^2 */
    {0}, {0},                                                                       /* the pulses: drawn */
    {255, 200, 150, 180, 0, 90, 0, 120, 0, 0, 0, 40, 0, 0, 0, 30},                 /* ORGAN: the drawbars */
    {120, 160, 220, 255, 200, 120, 80, 50, 30, 20, 10},                            /* VOX A */
    {255, 230, 120, 60, 30, 15, 8},                                                /* VOX O */
    {255, 60, 200, 40, 160, 20, 120, 10, 90, 0, 60, 0, 40, 0, 25},                 /* REED: odd, a little even */
    {255, 240, 225, 210, 195, 180, 165, 150, 135, 120, 105, 90, 75, 60, 45, 30},   /* BRIGHT */
    {255, 0, 85, 0, 51, 0, 36, 0, 28, 0, 23, 0, 20, 0, 17},                        /* HOLLOW: odd, 1/n */
    {255, 0, 0, 140, 0, 0, 120, 0, 0, 0, 100, 0, 70},                              /* GLASS: high partials far apart */
    {255, 128, 85, 110, 90, 75, 64, 56, 28, 25, 23, 21, 20, 18, 17, 16},           /* STRING: a saw, a bump in 4..8 */
    {150, 255, 200, 120, 220, 180, 150, 100, 90, 80, 70, 60, 50, 40, 30, 20},       /* CLAV */
    {255, 180, 120, 40},                                                           /* BASSY */
    {120, 90, 140, 200, 255, 230, 160, 90, 40, 20},                                /* NASAL */
    {0}, {0}};                                                                      /* SYNC, STEP TRI: drawn */
static void waves_init(void)
{
    static int32_t acc[256];
    uint32_t w, i, h;
    for (w = 0; w < NWT; w++) {
        int32_t pk = 1;
        for (i = 0; i < 256u; i++) {
            int32_t a = 0;
            switch (w + W_TAB) {
            case W_P25: a = i < 64u ? 96 : -32; break;              /* no mean: the high part three times the low */
            case W_P12: a = i < 32u ? 112 : -16; break;
            case W_SYNC: a = (int32_t)((i * 666u >> 8) & 255u) - 128; break;   /* a saw synced: 2.6 cycles, cut off */
            case W_STEPTRI: a = (int32_t)((i < 128u ? i : 255u - i) >> 3) * 17 - 128; break;   /* 16 steps, as an 8-bit console's */
            default:
                for (h = 0; h < 16u; h++) if (WHARM[w][h]) a += WHARM[w][h] * SIN[(i * 4u * (h + 1u)) & 1023u] >> 8;
            }
            acc[i] = a; if ((a < 0 ? -a : a) > pk) pk = a < 0 ? -a : a;
        }
        for (i = 0; i < 256u; i++) WV[w + W_TAB][i] = (int16_t)(acc[i] * 127 / pk * 256);   /* 8-bit, as a tracker's */
    }
    for (w = 0; w < NWAVE; w++) {
        int32_t m = 0, a = 0, x;
        if (w < W_TAB) for (i = 0; i < 256u; i++) { x = wv(w, i << 24); WV[w][i] = (int16_t)(x > 32767 ? 32767 : x < -32767 ? -32767 : x); }
        for (i = 0; i < 256u; i++) m += WV[w][i];
        WDC[w] = (int16_t)(m / 256);
        for (i = 0; i < 256u; i++) { x = WV[w][i] - WDC[w]; a += x < 0 ? -x : x; }
        WLOUD[w] = (int16_t)(a / 256);
    }
}

/* ---- FM patches: index Q8 (phase modulation, already divided by the ratio), amp Q15 for one voice */
typedef struct {
    const char *name;
    uint8_t ratio16;            /* modulator frequency / carrier, x16 */
    uint16_t i0, i1, it_ms;     /* index from i0 to i1 over it_ms */
    uint16_t a_ms, amp;         /* attack, peak level */
    uint8_t sus;                /* sustain, of 255 */
    uint16_t r_ms;              /* release */
    uint8_t vib, square;
    uint16_t d_ms;              /* decay toward the sustain (0 = 80 ms); with sus 0 a long one rings out, as a piano */
    uint8_t chaos;              /* 1: add the pad and the brown noise under the lead (CHAOS); 2: an ensemble, two
                                 * more detuned oscillators with the note (strings and brass as sections);
                                 * 3: no FM, seven detuned saws (SUPERSAW); 4: the orchestra hit, the note with
                                 * its fifth, octave and tenth in one brassy stab (ORCH HIT, the last patch);
                                 * 5: a choir's aah, saws through vowel formants (AH VOX) */
    uint8_t breath;             /* noise in the tone, following the note's level: bow, breath (0 = a clean FM tone) */
    uint8_t cw, mw;             /* the carrier's and the modulator's waveform (W_x; 0 = the sine) */
} patch_t;
static const patch_t PATCH[] = {
    {"FM BASS", 16, 1138, 260, 180,   4, 6200,  178,  80, 0, 0},
    {"SLAP",    32,  975,  81,  60,   2, 5600,  128,  60, 0, 0},
    {"PIZZ",    16,  650,  65, 100,   2, 5900,   10, 120, 0, 0},
    {"SUB",     16,   97,  32, 200,   5, 7400,  230, 100, 0, 0},
    {"BRASS",   16,  420, 1250,  90, 30, 2000,  217, 180, 1, 0,    0, 2, 40},   /* the first harmonics nearly equal: the blat */
    {"STRINGS", 16,  640, 460, 600, 300, 1500,  230, 650, 1, 0,    0, 2, 90},   /* bright as a recorded section, bow noise */
    {"BELL",    56,  371,  28, 800,   2, 2060,   51, 800, 0, 0},
    {"ORGAN",   32,  162, 162, 100,  10, 1770,  255,  60, 0, 0},
    {"FLUTE",   16,  130,  65, 200,  60, 2360,  230, 150, 1, 0,    0, 0, 55},   /* breathy */
    {"CHIP",    16,    0,   0, 100,   3, 1180,  230,  50, 1, 1},
    {"RAZOR",   16,  980, 560, 350,   3, 2300,  215, 110, 1, 0},   /* 1:1, a high index: saw-like, cutting */
    {"PULSE",   32,  420, 150, 250,   2, 1500,  210,  90, 1, 1},   /* 2:1 through the square: a hollow pulse */
    {"PIANO",   16,  700,  90, 450,   1, 2600,    0, 260, 0, 0, 1100},   /* 1:1, the hammer's brightness fades, rings out */
    {"E.PIANO", 224, 120,   0, 160,   1, 2500,    0, 300, 0, 0, 1500},   /* 14:1 tine on the attack, then a sine */
    {"CHAOS",   16,  520, 380, 400,   4, 3800,  230, 500, 1, 0,    0, 1},   /* a light saw + a heavy pad + brown noise */
    {"VIOLINS", 16,  680, 500, 400,  60, 1800,  230, 350, 1, 0,    0, 2, 80},   /* a section, singing: the symphony's lead */
    {"CELLOS",  16,  700, 450, 300,  90, 2700,  220, 300, 1, 0,    0, 2, 70},   /* cellos and basses, bowed */
    {"SUPERSAW", 16,   0,   0, 100,   6, 3000,  235, 380, 0, 0,    0, 3},   /* seven saws, wide and bright */
    {"AH VOX",   16,    0,   0, 100, 280, 5500,  225, 650, 1, 0,    0, 5},   /* a choir's aah: two saws through the vowel's formants */
    {"ORCH HIT", 16, 1100, 260, 160,   1, 2300,    0, 220, 0, 0,  420, 4, 120},   /* one key, a whole orchestra: bright, a burst of noise, gone in half a second */
    /* the families since 1.3, in the waveforms: basses */
    {"PC BASS",  16,  900, 220, 160,   3, 5680,  170,  80, 0, 0,    0, 0,  0, W_SINE, W_HALF},     /* the sound card's: a half-sine modulator */
    {"SAW BASS", 16,    0,   0, 100,   3, 5640,  200,  80, 0, 0,    0, 0,  0, W_SAW},
    {"PICKBASS", 32,  800,   0, 120,   2, 7860,   60,  90, 0, 0,  260},                           /* picked: bright, then round */
    {"UPRIGHT",  16,  500,  80, 150,   4, 6180,    0, 150, 0, 0,  900, 0,  0, W_SINE, W_ABS},      /* plucked, rings out */
    {"SQR BASS", 16,    0,   0, 100,   3, 3740,  210,  70, 0, 0,    0, 0,  0, W_HOLLOW},
    {"RUBBER",   16,  700, 150, 250,   3, 4820,  190,  90, 0, 0,    0, 0,  0, W_SINE, W_CAMEL},          /* round, a little hollow */
    {"WARMBASS", 16,  200,  60, 200,   5, 7130,  220, 100, 0, 0,    0, 0,  0, W_BASSY},
    /* leads */
    {"SAW LEAD", 16,    0,   0, 100,   4, 2410,  225, 120, 1, 0,    0, 0,  0, W_SAW},
    {"HARDSYNC", 16,    0,   0, 100,   3, 2570,  220, 110, 1, 0,    0, 0,  0, W_SYNC},
    {"SQR LEAD", 16,    0,   0, 100,   4, 1680,  225, 110, 1, 0,    0, 0,  0, W_HOLLOW},
    {"PC LEAD",  16,  650, 380, 250,   3, 4550,  215, 140, 1, 0,    0, 0,  0, W_HALF, W_SINE},
    {"PC LEAD2", 32,  500, 250, 300,   3, 2930,  210, 140, 1, 0,    0, 0,  0, W_ALT, W_QUART},
    {"WHISTLE",  16,   30,  30, 100,  40, 1890,  235, 120, 1, 0,    0, 0, 25},
    {"OBOE",     16,  250, 200, 200,  35, 3140,  225, 120, 1, 0,    0, 0, 20, W_REED},
    {"CLARINET", 16,  120, 100, 200,  40, 1640,  230, 130, 1, 0,    0, 0, 18, W_HOLLOW},
    {"TRUMPET",  16,  300, 900,  70,  25, 1930,  215, 130, 1, 0,    0, 0, 15},                     /* opens up as it is blown */
    {"SAX",      16,  450, 350, 200,  25, 2730,  220, 140, 1, 0,    0, 0, 35, W_REED},
    {"BAGPIPE",  16,  150, 120, 200,  30, 5050,  240, 120, 0, 0,    0, 0, 25, W_NASAL},
    {"VOX LEAD", 16,   80,  60, 200,  30, 4490,  230, 160, 1, 0,    0, 0, 10, W_VOXA},
    /* struck and plucked */
    {"MARIMBA",  64,  600,   0,  40,   1, 2790,    0, 120, 0, 0,  400},
    {"VIBES",    64,  200,  40, 300,   1, 2780,    0, 400, 1, 0, 1600},
    {"KALIMBA", 112,  300,   0,  30,   1, 2780,    0, 150, 0, 0,  500},
    {"MUSICBOX", 64,  250,   0, 100,   1, 2780,    0, 300, 0, 0, 1200, 0,  0, W_SINE, W_ABS},
    {"HARPSI",   48,  900, 300, 200,   1, 2790,    0, 150, 0, 0, 1200, 0,  0, W_SINE, W_ABS},
    {"CLAV",     16,  400,   0, 100,   1, 2860,    0,  80, 0, 0,  300, 0,  0, W_CLAV},
    {"GUITAR",   16,  800, 150, 250,   1, 2790,    0, 200, 0, 0, 1500, 0,  0, W_SINE, W_HALF},
    {"MUTE GTR", 16,  700, 100,  80,   1, 2810,    0,  60, 0, 0,  160, 0,  0, W_SINE, W_HALF},
    {"HARP",     32,  300,   0, 300,   1, 2780,    0, 400, 0, 0, 1800},
    {"KOTO",     48,  700, 100, 120,   1, 2790,    0, 200, 0, 0,  900, 0,  0, W_SINE, W_ABS},
    {"STEELPAN", 40,  400, 100, 300,   1, 2780,    0, 250, 0, 0,  700},                          /* 5:2, a little out of tune, as the pan */
    {"TUBULAR",  56,  500, 100,1500,   1, 2790,    0, 800, 0, 0, 2500},
    {"GLOCK",    80,  250,  30, 200,   1, 2780,    0, 300, 0, 0,  900},
    {"TOYPIANO", 32,  400,  50, 200,   1, 4090,    0, 200, 0, 0,  600, 0,  0, W_CAMEL},
    /* organs */
    {"DRAWBAR",  16,    0,   0, 100,   3, 2450,  255,  60, 1, 0,    0, 0,  0, W_ORGAN},
    {"PERC ORG", 48,  400,   0,  80,   3, 2450,  255,  60, 1, 0,    0, 0,  0, W_ORGAN},          /* the third harmonic's click */
    {"REED ORG", 16,    0,   0, 100,  20, 2920,  250, 100, 1, 0,    0, 0, 10, W_REED},
    /* pads: sections (two more players, a little out) */
    {"WARM PAD", 16,   80,  60, 400, 400, 1770,  230, 800, 1, 0,    0, 2,  0, W_TRI},
    {"GLASSPAD", 16,   60,  40, 400, 600, 2700,  230, 900, 1, 0,    0, 2,  0, W_GLASS},
    {"VOX PAD",  16,    0,   0, 100, 500, 2170,  230, 800, 1, 0,    0, 2, 20, W_VOXO},
    {"ENSEMBLE", 16,    0,   0, 100, 200, 2410,  230, 600, 1, 0,    0, 2,  0, W_STRING},
    {"SWEEP",    16,    0, 900,2500, 300, 1530,  230, 800, 1, 0,    0, 2},                        /* opens slowly */
    {"SAW PAD",  16,    0,   0, 100, 300, 1790,  230, 700, 1, 0,    0, 2,  0, W_SAW},
    {"SYNBRASS", 16,  200, 600, 120,  40, 2200,  215, 200, 1, 0,    0, 2,  0, W_SAW},
    /* the chips' */
    {"CHIP 25",  16,    0,   0, 100,   2, 1730,  230,  50, 1, 0,    0, 0,  0, W_P25},
    {"CHIP 12",  16,    0,   0, 100,   2, 2650,  230,  50, 1, 0,    0, 0,  0, W_P12},
    {"CHIP TRI", 16,    0,   0, 100,   2, 2160,  240,  50, 0, 0,    0, 0,  0, W_STEPTRI},
    /* the styles since 1.3: BOOGIE's piano, ZEN's bamboo flute */
    {"HONKYTNK", 16,  700,  90, 450,   1, 2500,    0, 260, 0, 0, 1100, 2},                        /* a piano and two more, out of tune */
    {"SHAKU",    16,   90,  40, 300,  80, 2400,  220, 300, 1, 0,    0, 0, 90},                    /* breath first, then the tone */
};
#define NPATCH (sizeof PATCH / sizeof PATCH[0])
#define P_ORCH 19u                              /* the orchestra hit (the styles' hits) */
/* what a voice of each patch costs the audio interrupt, in units (a plain FM voice is 2), measured: the voice budget */
static const uint8_t PATCH_W[] = {2, 2, 2, 2, 5, 4, 2, 2, 3, 3, 3, 3, 2, 2, 5, 5, 5, 4, 3, 4,
                                  2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3,           /* a waveform costs what the sine does (a table) */
                                  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 3,  4, 4, 5, 4, 4, 4, 4,  2, 2, 2,  4, 3};
typedef char patch_w_all[sizeof PATCH_W == NPATCH ? 1 : -1];

typedef struct { const char *name; uint16_t kh, kl, kd, sf, sd, hf, hd; uint8_t sharp; } kit_t;   /* Hz and ms; sharp: a click, a crack, open hats */
static const kit_t KITS[] = {
    {"STANDARD", 150, 45, 320, 1700, 700, 8000, 60},     /* snares with a body and a tail, as recorded ones */
    {"TRACKER",  220, 60, 180, 2300, 520, 6000, 90},
    {"ELECTRO",  120, 38, 600, 1200, 600, 10000, 35},
    {"DEEP",     140, 33, 750, 1500, 620, 7500, 70},     /* a kick that drops very low and rings long, a dry kit around it */
    {"PUNCH",    260, 52, 240, 2600, 360, 9000, 45, 1},  /* sharp: the kick clicks, the snare cracks, the hats one pole bright */
    {"METAL",    300, 48, 200, 3200, 300, 11000, 30, 1}, /* hard and short: FACTORY's (its rim shots clank) */
};
#define NKIT (sizeof KITS / sizeof KITS[0])

/* ---- voices */
#define CTRL 32u                                /* control rate: envelopes every 32 samples */
enum { V_OFF = 0, V_FM, V_KICK, V_SNARE, V_HAT, V_CRASH, V_KETTLE, V_CLAP, V_OHAT, V_TOM, V_RIM,
       V_WASH, V_RISER, V_IMPACT, V_ZAP, V_CRACKLE, V_BITS, V_SURF, V_GALE };          /* V_WASH + SX_x = the sfx voices */
typedef struct {
    uint8_t kind, lane, stage, square;          /* stage: 0 attack, 1 decay/sustain, 2 release */
    uint8_t midi, midi_ch, arp_n, arp_i;
    uint32_t ph, mph, inc, target, ratio16;     /* carrier phase / step, modulator phase */
    int32_t amp, peak, sus, att, rel_k, dec_k;  /* Q15 levels, per-tick steps and factors (Q15) */
    int32_t idx, idx_end, idx_step;             /* FM index Q8 */
    uint32_t gate, age, ctr, arp_t;             /* samples until release, ticks alive */
    int8_t fx_porta; uint8_t fx_vib;            /* the effect column's porta (-100..100) and vibrato (0..100) for this note */
    int32_t glide_k, vib;                       /* glide factor Q15 (0 = none), vibrato on */
    uint32_t vph, inc_lo, inc_k;                /* vibrato phase; drum sweep target and factor */
    uint32_t arp[3];
    int32_t s1, s2, s3;                         /* filter states (snare, hat) */
    int32_t tone_amp; uint32_t tph;             /* the snare's tone */
    uint8_t chaos;                              /* CHAOS: the pad and the noise */
    uint8_t breath;                             /* the patch's noise in the tone (s1: its low-pass state) */
    uint32_t pph1, pph2;                        /* the pad's two oscillators */
    uint32_t ss[6];                             /* SUPERSAW: the six detuned saws' phases */
    int32_t pamp, brown;                        /* the pad's level (Q15, slow), the brown noise */
    uint8_t wt;                                 /* what it costs (the voice budget) */
    uint8_t key, arpk[3];                       /* the note it plays (0: an echo copy, a drum, a noise), an arpeggio's notes: the keys light them */
    uint8_t cw, mw; int16_t cdc;                /* the operators' waveforms, the carrier's mean */
} voice_t;
#define NVOICE 33u
static voice_t vc[NVOICE];
static const uint8_t VLO[NLANE] = {0, 5, 13, 17, 26, 30}, VHI[NLANE] = {5, 13, 17, 26, 30, 33};   /* voices per lane */
static uint32_t vage, noise_s = 22222u;

/* ---- lane buses and effects state */
/* buses: one a lane, plus right-hand buses for the pad's and sfx's every other voice and the drums' hats and
 * crash (a tracker spreads its channels left and right; this spreads the voices) */
#define B_PADR NLANE
#define B_SFXR (NLANE + 1u)
#define B_HATS (NLANE + 2u)
#define NBUS (NLANE + 3u)
static const uint8_t BUS_LANE[NBUS] = {0, 1, 2, 3, 4, 5, L_PAD, L_SFX, L_DRUMS};   /* whose effects a bus takes */
static int32_t bus[NBUS][HALF_FRAMES];
static int32_t sendb[HALF_FRAMES];                      /* the lanes' sends to the room */
static volatile int32_t space = 35;                     /* 0..100: the room (MAIN) */
/* where each lane sits: its PAN (lead a little right, the arp a little left, the rest in the middle), and around it
 * its buses: the pad's and the sfx's voices spread left and right, the hats right of the drums. How much of each
 * goes to the room is the lane's REVERB (fx.send) */
static const int8_t PAN_OFF[NBUS] = {0, -70, 0, 0, -68, 0, 70, 68, 50};   /* a bus's place around its lane's PAN: pad and sfx spread, the hats right of the drums */
static int32_t pan_gain(int32_t pan, int right)                   /* equal power, Q8: the middle at 256 on both sides (as before),
                                                                 * a lane panned anywhere as loud (hard left: 362 on one side) */
{
    int32_t th = (pan + 100) * 256 / 200, g;                        /* 0 .. a quarter of the sine table */
    if (th < 0) th = 0; else if (th > 256) th = 256;
    g = (362 * SIN[right ? th : th + 256]) >> 15;
    return g < 0 ? 0 : g;
}

/* ---- the room: Schroeder / Moorer, at 22 kHz; comb and allpass lines in the pool */
#define RV_C0 557u
#define RV_C1 593u
#define RV_C2 641u
#define RV_C3 677u
#define RV_AL0 113u
#define RV_AL1 173u
#define RV_AR0 127u
#define RV_AR1 191u
static int16_t rv_c[RV_C0 + RV_C1 + RV_C2 + RV_C3] __attribute__((section(".pool")));
static int16_t rv_a[RV_AL0 + RV_AL1 + RV_AR0 + RV_AR1] __attribute__((section(".pool")));
static uint32_t rv_ci[4], rv_ai[4];
static int32_t rv_lp[4];
static int32_t rv_sat(int32_t v) { return v > 32767 ? 32767 : v < -32767 ? -32767 : v; }
static int32_t rv_comb(uint32_t k, int32_t x)
{
    static const uint16_t LEN[4] = {RV_C0, RV_C1, RV_C2, RV_C3}, OFF[4] = {0, RV_C0, RV_C0 + RV_C1, RV_C0 + RV_C1 + RV_C2};
    int16_t *b = rv_c + OFF[k];
    int32_t y = b[rv_ci[k]];
    rv_lp[k] += ((y - rv_lp[k]) * 22000) >> 15;                        /* the damping: highs die first */
    b[rv_ci[k]] = (int16_t)rv_sat(x + ((rv_lp[k] * 27500) >> 15));    /* feedback 0.84 */
    if (++rv_ci[k] >= LEN[k]) rv_ci[k] = 0;
    return y;
}
static int32_t rv_allpass(uint32_t k, int32_t x)
{
    static const uint16_t LEN[4] = {RV_AL0, RV_AL1, RV_AR0, RV_AR1}, OFF[4] = {0, RV_AL0, RV_AL0 + RV_AL1, RV_AL0 + RV_AL1 + RV_AR0};
    int16_t *b = rv_a + OFF[k];
    int32_t d = b[rv_ai[k]], y = d - x;
    b[rv_ai[k]] = (int16_t)rv_sat(x + (d >> 1));
    if (++rv_ai[k] >= LEN[k]) rv_ai[k] = 0;
    return y;
}
static void room(int32_t *out, uint32_t n)
{
    uint32_t k;
    int32_t g = space * 3;                                              /* Q8 wet level: 100 -> 1.2 */
    if (space <= 0) return;
    for (k = 0; k + 1u < n; k += 2u) {
        int32_t x = (sendb[k] + sendb[k + 1u]) >> 3, c, l, r;            /* half rate; headroom */
        c = rv_comb(0, x) + rv_comb(1, x) + rv_comb(2, x) + rv_comb(3, x);
        c >>= 1;
        l = rv_allpass(1, rv_allpass(0, c));
        r = rv_allpass(3, rv_allpass(2, c));
        l = (l * g) >> 8; r = (r * g) >> 8;
        out[2u * k] += l; out[2u * k + 1u] += r; out[2u * k + 2u] += l; out[2u * k + 3u] += r;
    }
}
static int32_t f_low[NBUS], f_band[NBUS], crush_hold[NBUS], crush_n[NBUS], trem_g[NBUS];
static int32_t pan_cl[NBUS], pan_cr[NBUS];                 /* each bus's pan gains as the last block ended (Q8) */
/* for the pictures on SETUP (the screen reads them, a frame at a time): each lane's and the output's peak since the
 * screen last looked, and the last 1024 samples of the output, every 4th one kept (halved), left and right */
static volatile int32_t lane_pk[NLANE], out_pk[2];
static int16_t scope_l[256], scope_r[256];
static volatile uint32_t scope_w;
static const uint8_t AP_STEPS[8] = {0, 128, 64, 32, 16, 8, 4, 2};   /* PAN RATE: a sweep's cycle in 16ths */
static uint32_t wob_ph[NLANE];
static uint32_t lane_last_inc[NLANE][3];
static volatile uint32_t lane_hit_ms[NLANE], hit_ms[NDK > NSX ? NDK : NSX];   /* the last hit of each drum (or sfx) kind: the keys light it */

/* ---- the clock */
static volatile int32_t seq_playing, seq_start_req, seq_stop_req, seq_start_step;
static volatile uint32_t seq_step_len_q8 = (661500u * 256u) / 125u;   /* samples per 16th, Q8 */
static volatile int32_t seq_bpm = 125;
static volatile uint32_t vis_step, vis_frac;    /* where the play head is (frac: 0..65535 of a step) */
static volatile uint32_t isr_us_max, isr_us_last, isr_us_sum, isr_n;
static uint32_t seq_step, seq_acc_q8, smp;      /* the step playing, samples into it (Q8), samples since boot */

/* ---- retrigger repeats and echo copies: voice starts that wait for their sample time */
typedef struct { uint32_t when, dur; uint8_t lane, n, flags, lvl, kind, orig; uint8_t note[3], fx; } pend_t;   /* orig: the note itself (swung or delayed), not a copy */
#define NPEND 64u
static pend_t pend[NPEND];
static uint32_t npend;

/* ---- MIDI out, all of it sent from here (the audio ISR is USB-MIDI's one sender): notes, bends and the clock at
 * once, in time with the sound; what the main loop sends (controllers, settings) waits in mm_q and goes at the
 * next block. SYNC (SETUP) turns the clock and the controllers off. */
static volatile uint32_t mm_q[128], mm_w, mm_r;          /* main loop -> ISR */
static volatile uint8_t clk_out_on = 1, cc_out_on = 1;   /* SYNC: CLOCK OUT, CC OUT */
static volatile uint8_t clk_ext;                         /* a clock comes in (the main loop): none goes out then */
static volatile int32_t seq_nudge_q8;                    /* the clock in's correction, in samples (Q8), taken once */
static volatile int32_t pos_q8;                          /* where the last block ended (16ths Q8), and when (clk_now) */
static volatile uint32_t pos_ticks;
static void mo_push(uint32_t st, uint32_t d1, uint32_t d2) { if (midi_out_room() > 2u) midi_isr_send(st, d1, d2); }   /* ISR */
static void mm_push(uint32_t st, uint32_t d1, uint32_t d2)                                                            /* main loop */
{
    if (mm_w - mm_r < 128u) { mm_q[mm_w % 128u] = st | d1 << 8 | d2 << 16; RING_PUBLISH(); mm_w++; }
}
static void mm_drain(void)                                /* ISR: leaves room for the notes and the clock */
{
    while (mm_r != mm_w && midi_out_room() > 16u) { uint32_t m = mm_q[mm_r % 128u]; mm_r++; midi_isr_send(m & 0xFFu, m >> 8 & 0xFFu, m >> 16 & 0xFFu); }
}
static uint32_t vel_of(uint32_t lvl) { uint32_t v = (lvl * 127u + 128u) >> 8; return v < 1u ? 1u : v > 127u ? 127u : v; }   /* the accents as velocity */
static uint16_t bend_now[NLANE] = {8192, 8192, 8192, 8192, 8192, 8192};   /* the bend each lane's channel is at */
static const uint8_t GM_DRUM[NDK] = {36, 38, 42, 47, 49, 39, 46, 45, 37};
/* the MIDI channel of each lane (0..15; 16 = off): lead 1, pad 2, bass 3, drums 10, arp 4 (SETUP, MIDI); the sfx sends none */
static volatile uint8_t midi_chan[NLANE] = {0, 1, 2, 9, 16, 3};

static void synth_init(void)
{
    uint32_t i;
    for (i = 0; i < 1024u; i++) SIN[i] = (int16_t)sine_q15(i << 22);
    waves_init();
    static const uint8_t SEND0[NLANE] = {35, 60, 3, 18, 100, 45};  /* REVERB: lead some, pad much, bass hardly, drums a little, sfx all, arp some */
    for (i = 0; i < NLANE; i++) { fx[i].aglide = 100; fx[i].level = 80; fx[i].bright = 25; fx[i].rtg_rate = 1; fx[i].inst = -1; fx[i].res = 30; fx[i].send = SEND0[i]; }
    fx[L_SFX].level = 30;
    fx[L_LEAD].level = 50;                                        /* the lead: the ear hears its range most; 50 sits it in the mix */
    fx[L_ARP].level = 60;                                         /* the arp beside it */
    fx[L_LEAD].autopan = 20; fx[L_ARP].autopan = 20;               /* every lane in the middle; lead and arp move a little, note by note */
    for (i = 0; i < NLANE; i++) fx[i].mutate = i == L_SFX ? 0 : MUTATE0;   /* each song its own sounds */
}

static inline int32_t noise(void) { noise_s = noise_s * 1664525u + 1013904223u; return (int32_t)noise_s >> 17; }
static int32_t tau_k(uint32_t ms)               /* per-tick factor (Q15) of a decay with time constant ms */
{
    uint32_t smp_tau = ms * 441u / 10u;
    if (smp_tau < CTRL * 2u) smp_tau = CTRL * 2u;
    return 32768 - (int32_t)((CTRL * 32768u) / smp_tau);
}

/* the voice budget: all that sounds may cost at most voice_limit units (VOICES, SETUP); past it, the quietest voice
 * that is fading out goes first (an echo's tail, mostly), else the oldest. So no style, echo or chord can make a block
 * of sound late (a crack) */
#ifndef VOICE_LIMIT0
#define VOICE_LIMIT0 80                         /* the default: the styles alone reach about 78; with echo everywhere
                                                * SYMPHONY reached 107 and 109 % of the time on an FM-1 */
#endif
static volatile int32_t voice_limit = VOICE_LIMIT0, voice_now, voice_peak;
static voice_t *alloc_voice(uint32_t lane, uint32_t wt)
{
    uint32_t v, pick = VLO[lane], best = 0xFFFFFFFFu, total = wt;
    for (v = VLO[lane]; v < VHI[lane]; v++) {
        /* a free voice, else the quietest one in its release, else the oldest */
        uint32_t score = vc[v].kind == V_OFF ? 0u
                       : vc[v].stage == 2u || (vc[v].kind != V_FM && vc[v].kind < V_WASH) ? 1u + (uint32_t)vc[v].amp
                       : 0x7FFFFFFFu - ((vage - vc[v].age) & 0x3FFFFFFFu);
        if (score < best) { best = score; pick = v; }
    }
    if (vc[pick].kind != V_OFF && vc[pick].midi) mo_push(0x80u | vc[pick].midi_ch, vc[pick].midi, 0);
    vc[pick].kind = V_OFF;
    for (v = 0; v < NVOICE; v++) if (vc[v].kind != V_OFF) total += vc[v].wt;
    while (total > (uint32_t)voice_limit) {                          /* over budget: one goes, anywhere */
        uint32_t victim = NVOICE, vb = 0xFFFFFFFFu;
        for (v = 0; v < NVOICE; v++) {
            uint32_t score;
            if (vc[v].kind == V_OFF) continue;
            score = vc[v].stage == 2u || (vc[v].kind != V_FM && vc[v].kind < V_WASH) ? 1u + (uint32_t)vc[v].amp : 0x7FFFFFFFu - ((vage - vc[v].age) & 0x3FFFFFFFu);
            if (score < vb) { vb = score; victim = v; }
        }
        if (victim == NVOICE) break;
        if (vc[victim].midi) mo_push(0x80u | vc[victim].midi_ch, vc[victim].midi, 0);
        total -= vc[victim].wt; vc[victim].kind = V_OFF;
    }
    vc[pick].wt = (uint8_t)wt; vc[pick].key = 0;
    return &vc[pick];
}

/* MUTATE: each song plays a lane's sound its own way. From the lane's song seed and the patch: the FM index, the
 * modulator's ratio and waveform, now and then the carrier's, and the envelope's times move; 0 = as designed */
typedef struct { int32_t iq, aq, dq, rq, gq; uint32_t ratio16, cw, mw; } mut_t;
static void mutate(const patch_t *P, uint32_t pi, uint32_t lane, mut_t *M)
{
    int32_t amt = fx[lane].mutate;
    uint32_t h = ((uint32_t)fx[lane].mseed + 1u) * 2654435761u ^ (pi + 7u) * 40503u ^ lane * 97u;
    M->iq = M->aq = M->dq = M->rq = M->gq = 256; M->ratio16 = P->ratio16; M->cw = P->cw; M->mw = P->mw;
    if (amt <= 0) return;
#define MR(n) (h = h * 1103515245u + 12345u, (int32_t)((h >> 16) % (uint32_t)(n)))
    M->iq = 256 + (MR(257) - 128) * amt / 80;                       /* the brightness: up to +-60 % */
    M->aq = 256 + (MR(257) - 128) * amt / 100;                      /* the times: up to +-50 % */
    M->dq = 256 + (MR(257) - 128) * amt / 100;
    M->rq = 256 + (MR(257) - 128) * amt / 100;
    if (P->chaos <= 2u) {                                           /* an FM tone (not the saws, the hit, the choir) */
        if (P->ratio16 % 16u == 0 && P->ratio16 <= 64u && MR(100) < amt / 2) {   /* a harmonic ratio: one next to it */
            int32_t r = (int32_t)P->ratio16 + (MR(2) ? 16 : -16);
            M->ratio16 = (uint32_t)(r < 16 ? 32 : r);
        }
        if (P->mw <= W_QUART && MR(100) < amt / 2) M->mw = (uint32_t)MR(4);   /* the sine, the half, the rectified, the quarters */
        if (P->cw >= W_SAW && MR(100) < amt / 2) {                 /* a drawn waveform: another of its kind */
            static const uint8_t FAM[4][4] = {{W_SAW, W_STRING, W_BRIGHT, W_SYNC}, {W_HOLLOW, W_REED, W_P25, W_CLAV},
                                              {W_TRI, W_VOXO, W_BASSY, W_STEPTRI}, {W_VOXA, W_NASAL, W_ORGAN, W_GLASS}};
            uint32_t f, k;
            for (f = 0; f < 4u; f++) for (k = 0; k < 4u; k++) if (FAM[f][k] == P->cw) { M->cw = FAM[f][MR(4)]; f = 4u; break; }
        }
        if (P->cw == W_SINE && MR(100) < amt / 4) M->cw = W_HALF;   /* now and then a half-sine carrier (the rectified ones
                                                                     * sound an octave up: modulators only) */
    }
#undef MR
    if (M->cw != P->cw && WLOUD[M->cw] > 0) {                      /* another waveform: as loud as the patch's */
        M->gq = WLOUD[P->cw] * 256 / WLOUD[M->cw];
        M->gq = M->gq < 160 ? 160 : M->gq > 384 ? 384 : M->gq;  /* not past 1.5 times: spiky waves */
    }
}

/* the effect column's porta and vibrato for the notes fire() starts now (set around the call, 0 otherwise) */
static int32_t note_porta, note_vib;
/* start one FM voice: lvl Q8, dur in samples; from_inc: slide start (0 = none) */
static voice_t *last_voice;                             /* the voice start_fm just started (the keys hold it) */
static void start_fm(uint32_t lane, const patch_t *P, uint32_t inc, uint32_t dur, uint32_t lvl, uint32_t from_inc,
                     const uint32_t *arp, uint32_t arp_n, uint32_t midi)
{
    uint32_t pi = (uint32_t)(P - PATCH) < NPATCH ? (uint32_t)(P - PATCH) : 0u;
    mut_t M;
    mutate(P, pi, lane, &M);
    voice_t *v = alloc_voice(lane, PATCH_W[pi]);
    last_voice = v;
    int32_t bright = fx[lane].bright, mult = (64 + bright * 768 / 100) * M.iq >> 8;   /* Q8: 25 -> 1.0 */
    uint32_t a = (uint32_t)P->a_ms * 441u / 10u * (uint32_t)M.aq >> 8, i;
    v->kind = V_FM; v->lane = (uint8_t)lane; v->stage = 0; v->square = P->square;
    v->cw = (uint8_t)M.cw; v->mw = (uint8_t)M.mw; v->cdc = WDC[M.cw];
    v->target = inc; v->inc = from_inc ? from_inc : inc; v->ratio16 = M.ratio16;
    v->ph = 0; v->mph = 0;
    v->peak = (int32_t)((((P->amp * (uint32_t)M.gq) >> 8) * lvl) >> 8);
    v->sus = (v->peak * P->sus) >> 8;
    v->amp = 0;
    v->att = (int32_t)((uint32_t)v->peak * CTRL / (a < CTRL ? CTRL : a));
    if (v->att < 1) v->att = 1;
    v->dec_k = tau_k((P->d_ms ? P->d_ms : 80u) * (uint32_t)M.dq >> 8);
    v->rel_k = tau_k(((uint32_t)P->r_ms * (uint32_t)M.rq >> 8) / 3u + 1u);
    v->idx = (P->i0 * mult) >> 8; v->idx_end = (P->i1 * mult) >> 8;
    v->idx_step = (v->idx_end - v->idx) * (int32_t)CTRL / (int32_t)((uint32_t)P->it_ms * 441u / 10u + 1u);
    v->gate = dur; v->age = ++vage; v->ctr = 0;
    v->glide_k = 0;
    if (from_inc && fx[lane].slide > 0) {
        uint32_t g = (uint32_t)fx[lane].slide * 132u;               /* up to 0.3 s, in samples / 100 */
        v->glide_k = (int32_t)((CTRL * 32768u) / (g + CTRL));
    }
    v->vib = P->vib; v->vph = 0;
    v->arp_n = (uint8_t)arp_n; v->arp_i = 0; v->arp_t = 0;
    for (i = 0; i < arp_n && i < 3u; i++) v->arp[i] = arp[i];
    v->key = (uint8_t)midi;
    if (midi_chan[lane] > 15u) midi = 0;                            /* this lane sends no MIDI */
    v->midi = (uint8_t)midi; v->midi_ch = midi_chan[lane];
    v->chaos = P->chaos; v->pph1 = 0x40000000u; v->pph2 = 0; v->pamp = 0; v->brown = 0;
    v->breath = P->breath; v->s1 = 0;
    v->fx_porta = (int8_t)note_porta; v->fx_vib = (uint8_t)note_vib;
    if (P->chaos == 3u) for (i = 0; i < 6u; i++) { (void)noise(); v->ss[i] = noise_s; }   /* free-running: each note starts elsewhere */
    if (P->chaos == 4u) { v->ss[0] = 0x20000000u; v->ss[1] = 0; }
    if (P->chaos == 5u) { v->s2 = v->s3 = 0; v->pph1 = 0x55555555u; }                  /* the formant filters empty, the second voice elsewhere */                      /* the hit's tenth; ss[1]: its noise burst's age */
    if (midi) {
        if (bend_now[lane] != 8192u) { mo_push(0xE0u | v->midi_ch, 0, 64); bend_now[lane] = 8192u; }   /* a new note starts in tune */
        mo_push(0x90u | v->midi_ch, midi, vel_of(lvl));
    }
}

static uint32_t tom_at, tom_n;                  /* toms in a row step down: the last one's time, how many */
static void start_drum(uint32_t kind, uint32_t lvl, uint32_t kit, int32_t is_orig)
{
    voice_t *v;
    uint32_t gm = GM_DRUM[kind < NDK ? kind : 0];
    if (kind == DK_HAT) { uint32_t i; for (i = 0; i < NVOICE; i++) if (vc[i].kind == V_OHAT) vc[i].dec_k = tau_k(8); }   /* the closed hat chokes the open one */
    v = alloc_voice(L_DRUMS, 2u);
    const kit_t *K = &KITS[kit < NKIT ? kit : 0];
    v->lane = L_DRUMS; v->stage = 1; v->ph = 0; v->ctr = 0; v->age = ++vage; v->s1 = v->s2 = v->s3 = 0;
    v->gate = 0xFFFFFFFFu; v->midi = 0; v->chaos = 0; v->breath = K->sharp; v->tone_amp = 0;   /* breath: the kit's sharpness, for drums */
    switch (kind) {
    case DK_KICK: case DK_KETTLE: {
        uint32_t lo = kind == DK_KETTLE ? note_inc(38 + key_off) : HZ_INC(K->kl);
        v->kind = kind == DK_KETTLE ? V_KETTLE : V_KICK;
        v->inc_lo = lo; v->inc = kind == DK_KETTLE ? lo + (lo * 3u >> 2) : HZ_INC(K->kh);
        v->inc_k = (uint32_t)tau_k(kind == DK_KETTLE ? 20u : 30u);
        v->amp = (int32_t)(((kind == DK_KETTLE ? 10300u : 14000u) * lvl) >> 8);
        if (K->sharp && kind == DK_KICK) v->tone_amp = v->amp / 2;
        v->dec_k = tau_k((kind == DK_KETTLE ? 900u : K->kd) * 10u / 69u);
        break; }
    case DK_SNARE:
        v->kind = V_SNARE; v->amp = (int32_t)((6600u * lvl) >> 8); v->dec_k = tau_k(K->sd * 10u / 69u);
        v->tone_amp = (int32_t)((3700u * lvl) >> 8); v->tph = 0;
        v->inc = (uint32_t)K->sf;                /* the band's centre, Hz */
        break;
    case DK_HAT:
        v->kind = V_HAT; v->amp = (int32_t)((2400u * lvl) >> 8); v->dec_k = tau_k(K->hd * 10u / 69u);
        break;
    case DK_CLAP:                                /* the bursts and the tail: see the control tick */
        v->kind = V_CLAP; v->amp = (int32_t)((7000u * lvl) >> 8); v->dec_k = tau_k(K->sd * 4u / 10u * 10u / 69u);
        v->inc_k = (uint32_t)tau_k(3);
        break;
    case DK_OHAT:
        v->kind = V_OHAT; v->amp = (int32_t)((2400u * lvl) >> 8); v->dec_k = tau_k(K->hd * 8u * 10u / 69u);
        break;
    case DK_TOM: {
        static const uint16_t HZ[5] = {196, 165, 131, 110, 92};
        uint32_t lo;
        if (smp - tom_at > 17640u) tom_n = 0;      /* 0.4 s since the last: a fill starts high again */
        tom_at = smp;
        lo = HZ_INC(HZ[tom_n] * K->kh / 150u); gm = 50u - tom_n * 2u + (tom_n > 2u);   /* GM 50 48 46.. -> 50 48 47 45 43 */
        if (tom_n < 4u) tom_n++;
        v->kind = V_TOM; v->inc_lo = lo; v->inc = lo + (lo * 3u >> 3); v->inc_k = (uint32_t)tau_k(40);
        v->amp = (int32_t)((11000u * lvl) >> 8); v->dec_k = tau_k(380u * 10u / 69u);
        v->tone_amp = (int32_t)((3000u * lvl) >> 8);
        break; }
    case DK_RIM:
        v->kind = V_RIM; v->amp = (int32_t)((5000u * lvl) >> 8); v->dec_k = tau_k(45u * 10u / 69u);
        v->tone_amp = (int32_t)((3500u * lvl) >> 8); v->tph = 0;
        break;
    default:
        v->kind = V_CRASH; v->amp = (int32_t)((2950u * lvl) >> 8); v->dec_k = tau_k(2600u * 10u / 69u);   /* ~1 s */
        break;
    }
    v->peak = v->amp;
    if (is_orig && midi_chan[L_DRUMS] <= 15u) { v->midi = (uint8_t)gm; v->midi_ch = midi_chan[L_DRUMS]; mo_push(0x90u | v->midi_ch, v->midi, vel_of(lvl)); }
}

/* an sfx sound: kind SX_x, pitch a MIDI note (riser start, bits), dur in samples (wash, riser, crackle) */
static void start_sfx(uint32_t kind, uint32_t pitch, uint32_t dur, uint32_t lvl)
{
    voice_t *v = alloc_voice(L_SFX, kind == SX_GALE || kind == SX_WASH ? 3u : 2u);
    if (dur < CTRL * 4u) dur = CTRL * 4u;
    v->lane = L_SFX; v->kind = (uint8_t)(V_WASH + (kind < NSX ? kind : 0)); v->ph = 0; v->ctr = 0; v->age = ++vage;
    v->s1 = v->s2 = v->s3 = 0; v->tph = 0; v->tone_amp = 0; v->midi = 0; v->gate = 0xFFFFFFFFu; v->stage = 2; v->peak = 0; v->chaos = 0; v->breath = 0;
    switch (v->kind) {
    case V_WASH:                                    /* in over a third of its time, out over 0.6 s */
        v->peak = (int32_t)((15000u * lvl) >> 8); v->amp = 0; v->stage = 0;
        v->att = (int32_t)((uint32_t)v->peak * CTRL / (dur / 3u + CTRL)); if (v->att < 1) v->att = 1;
        v->gate = dur; v->rel_k = tau_k(600);
        break;
    case V_RISER:                                   /* climbs over its whole time, then stops dead */
        v->peak = (int32_t)((11000u * lvl) >> 8); v->amp = 0; v->stage = 0;
        v->att = (int32_t)((uint32_t)v->peak * CTRL / dur); if (v->att < 1) v->att = 1;
        v->inc = note_inc((int32_t)pitch);
        v->idx = (int32_t)(181700u * CTRL / dur);       /* ln 16 x 65536 per tick: four octaves over dur */
        v->gate = dur; v->rel_k = tau_k(15);
        break;
    case V_IMPACT:
        v->amp = (int32_t)((16000u * lvl) >> 8); v->dec_k = tau_k(700);
        v->tone_amp = (int32_t)((9000u * lvl) >> 8);
        v->inc = HZ_INC(110); v->inc_lo = HZ_INC(30); v->inc_k = (uint32_t)tau_k(120);
        break;
    case V_ZAP:
        v->amp = (int32_t)((8000u * lvl) >> 8); v->dec_k = tau_k(110);
        v->inc = note_inc((int32_t)pitch + 24); v->inc_lo = HZ_INC(60); v->inc_k = (uint32_t)tau_k(35);
        break;
    case V_CRACKLE:
        v->peak = (int32_t)((1600u * lvl) >> 8); v->amp = 0; v->stage = 0;    /* a tenth of the others' level: it tires the ear */
        v->att = v->peak * (int32_t)CTRL / 900; if (v->att < 1) v->att = 1;
        v->gate = dur; v->rel_k = tau_k(150);
        break;
    case V_SURF:                                    /* waves of brown noise: in over a quarter, out over 1.2 s */
        v->peak = (int32_t)((11000u * lvl) >> 8); v->amp = 0; v->stage = 0; v->brown = 0;
        v->att = (int32_t)((uint32_t)v->peak * CTRL / (dur / 4u + CTRL)); if (v->att < 1) v->att = 1;
        v->tph = 0xC0000000u;                       /* the first wave from its trough */
        v->gate = dur; v->rel_k = tau_k(1200);
        break;
    case V_GALE:                                    /* a wind: in over half its time, out over 0.9 s */
        v->peak = (int32_t)((20000u * lvl) >> 8); v->amp = 0; v->stage = 0;
        v->att = (int32_t)((uint32_t)v->peak * CTRL / (dur / 2u + CTRL)); if (v->att < 1) v->att = 1;
        v->tph = noise_s; v->pph1 = noise_s << 7;   /* each gale its own course */
        v->gate = dur; v->rel_k = tau_k(900);
        break;
    default:                                        /* V_BITS */
        v->amp = (int32_t)((700u * lvl) >> 8); v->dec_k = tau_k(160);    /* a tenth, as CRACKLE */
        v->inc = note_inc((int32_t)pitch + 12); v->s1 = 0x7FFF;
        break;
    }
    v->peak = v->peak ? v->peak : v->amp;
}

/* how a note is played (the generator's flags): a glide into it (70 ms when the lane's SLIDE is off), a swell over
 * most of its length, or a fade over its length: the tracker's tone portamento and volume slides */
static void shape_voice(voice_t *v, uint32_t flags, uint32_t dur, int glide)
{
    if (glide && !v->glide_k) v->glide_k = (int32_t)((CTRL * 32768u) / (3087u + CTRL));      /* tau 70 ms */
    if (flags & EV_SWELL) {
        uint32_t a = dur * 6u / 10u;
        if (a < CTRL) a = CTRL;
        v->att = (int32_t)((uint32_t)v->peak * CTRL / a); if (v->att < 1) v->att = 1;
    } else if (flags & EV_FADE) {
        v->sus = v->peak >> 3;                                          /* toward an eighth of its level */
        v->dec_k = tau_k(dur / 2u * 10u / 441u + 1u);
    }
}

/* a voice start (now): melodic notes or a drum hit, with the lane's slide memory */
/* AUTOPAN: each new note (or chord, or the hits of one moment) on the other side; notes within 10 ms share one */
static uint8_t ap_side[NLANE];
static uint32_t ap_last[NLANE];
static void fire(uint32_t lane, const uint8_t *note, uint32_t n, uint32_t flags, uint32_t dur, uint32_t lvl,
                 uint32_t kind_or_inst, int32_t original)
{
    uint32_t i;
    lane_hit_ms[lane] = fm1_ms;
    if (fx[lane].autopan > 0 && !fx[lane].aprate && smp - ap_last[lane] > 441u) {
        ap_side[lane] ^= 1u; ap_last[lane] = smp;
        if (cc_out_on && midi_chan[lane] <= 15u && lane != L_SFX) {     /* MIDI: the side before the note */
            int32_t pn = fx[lane].pan + (ap_side[lane] ? fx[lane].autopan : -fx[lane].autopan);
            pn = pn < -100 ? -100 : pn > 100 ? 100 : pn;
            mo_push(0xB0u | midi_chan[lane], 10, (uint32_t)(64 + pn * 63 / 100));
        }
    }
    if (lane == L_DRUMS) { if (note[0] < NDK) hit_ms[note[0]] = fm1_ms; start_drum(note[0], lvl, kind_or_inst, original); return; }
    if (lane == L_SFX) { start_sfx(note[0], note[1], dur, lvl); return; }
    {
        const patch_t *P = &PATCH[flags & EV_ORCH ? P_ORCH : kind_or_inst < NPATCH ? kind_or_inst : 0];
        int32_t chord = n > 1u && !(flags & EV_ARP);
        uint32_t l2 = chord ? (lvl * 115u) >> 8 : lvl;                    /* chords: quieter per voice */
        if (flags & EV_ARP) {
            uint32_t a[3];
            for (i = 0; i < n && i < 3u; i++) a[i] = note_inc(note[i]);
            start_fm(lane, P, a[0], dur, l2, 0, a, n, original ? note[0] : 0);
            for (i = 0; i < 3u; i++) last_voice->arpk[i] = (uint8_t)(original && i < n ? note[i] : 0);
            return;
        }
        for (i = 0; i < n && i < 3u; i++) {
            uint32_t inc = note_inc(note[i]), from = original ? lane_last_inc[lane][i] : 0;
            int glide = (flags & EV_GLIDE) && from && from != inc;
            start_fm(lane, P, inc, dur, l2, fx[lane].slide > 0 || glide ? from : 0, 0, 0, original ? note[i] : 0);
            shape_voice(last_voice, flags, dur, glide);
            if (original) lane_last_inc[lane][i] = inc;
        }
    }
}

static void pend_add(uint32_t when, uint32_t lane, const ev_t *e, uint32_t dur, uint32_t lvl, uint32_t kind)
{
    pend_t *p;
    if (npend >= NPEND) return;
    p = &pend[npend++];
    p->when = when; p->dur = dur; p->lane = (uint8_t)lane; p->n = e->n; p->flags = e->flags; p->lvl = (uint8_t)(lvl > 255u ? 255u : lvl);
    p->kind = (uint8_t)kind; p->note[0] = e->note[0]; p->note[1] = e->note[1]; p->note[2] = e->note[2]; p->orig = 0; p->fx = e->fx;
}

static void note_fx(uint32_t f)                 /* the effect column's porta and vibrato, for the next fire() */
{
    uint32_t c = f >> 4, v = f & 15u;
    note_porta = c == FX_UP ? (int32_t)v * 7 : c == FX_DOWN ? -(int32_t)v * 7 : 0;
    note_vib = c == FX_VIB ? (int32_t)v * 7 : 0;
}
/* an event at its step: the note now, its stutter repeats, its echo copies; your effect column first */
static void trigger(uint32_t lane, const ev_t *e0, uint32_t kind)
{
    ev_t ev = *e0, *e = &ev;
    if (fx[lane].level <= 0) return;                               /* faded out (or VOLUME 0): no voice, no MIDI, no load */
    uint32_t step = seq_step_len_q8 >> 8, total = lane == L_DRUMS ? step : e->len * step;
    uint32_t fc = e->fx >> 4, fv = e->fx & 15u;
    int32_t rt = (fx[lane].retrig > 0 && (int32_t)(e->rv * 100u >> 8) < fx[lane].retrig) || (fc == FX_RETRIG && fv);
    uint32_t seg = fc == FX_RETRIG && fv ? step * fv / 6u : rt ? step / (uint32_t)(fx[lane].rtg_rate < 1 ? 1 : fx[lane].rtg_rate) : total;
    if (fc == FX_SLIDE) e->flags |= EV_GLIDE;                      /* SLIDE: from the lane's last note */
    if (seg < 64u) seg = 64u;
    uint32_t reps = rt ? total / (seg ? seg : 1u) : 1u, dur = rt ? seg * 7u / 10u : total * 95u / 100u, k, c;
    uint32_t late = (e->st & 1u) && fx[lane].swing > 0 ? step * (uint32_t)fx[lane].swing / 300u : 0u;   /* SWING: the off 16ths */
    if (fc == FX_DELAY) late += step * fv / 16u;                    /* DELAY: later by 16ths of a step */
    if (reps > 16u) reps = 16u;
    if (!reps) reps = 1;
    if (fx[lane].gate > 0 && lane != L_DRUMS && lane != L_SFX) {                 /* GATE: cut short */
        dur = dur * (uint32_t)(100 - fx[lane].gate * 85 / 100) / 100u;
        if (dur < 441u) dur = 441u;
    }
    if (fc == FX_CUT) { uint32_t cd = fv ? step * fv / 4u : step / 8u; if (cd < dur) dur = cd; }   /* CUT: after quarters of a step */
    uint32_t base = e->flags & EV_ACC2 ? 112u : e->flags & EV_ACC1 ? 176u : 256u;   /* the accent */
    if (fc == FX_VOL) base = base * (fv + 1u) >> 4;                  /* VOL: 0..15 = 1/16 .. full */
    for (c = 0; c < 3u; c++) {                                       /* the note, then two echo copies */
        uint32_t lvl = c == 0 ? base : c == 1 ? (uint32_t)fx[lane].echo * 141u / 100u * base >> 8
                                              : (uint32_t)(fx[lane].echo * fx[lane].echo) * 77u / 10000u * base >> 8;
        uint32_t at = smp + late + c * 3u * step;
        if (c && fx[lane].echo <= 0) break;
        if (!lvl) continue;
        note_fx(e->fx);
        for (k = 0; k < reps; k++) {
            if (c == 0 && k == 0 && !late) fire(lane, e->note, e->n, e->flags, dur, lvl, kind, 1);
            else { pend_add(at + k * seg, lane, e, dur, lvl, kind); if (c == 0 && k == 0 && npend) pend[npend - 1].orig = 1; }
        }
        note_porta = note_vib = 0;
    }
}

/* ---- the step clock: fire the events of this step from the bar cache */
static void step_fire(uint32_t s)
{
    uint32_t b = s >> 4, st = s & 15u, slot = b % NSLOT, l, i;
    const bar_t *B = &bars[slot][bar_sel[slot]];
    if (B->num != (int32_t)b) return;            /* not generated (yet): a silent step */
    for (l = 0; l < NLANE; l++) {
        if (B->muted[l]) continue;
        for (i = 0; i < B->cnt[l]; i++)
            if (B->ev[l][i].st == st) {
                uint32_t kind = fx[l].inst >= 0 ? (uint32_t)fx[l].inst : B->inst[l];
                trigger(l, &B->ev[l][i], kind);
            }
    }
}

/* ---- rendering */
static void render_voice(voice_t *v, int32_t *out, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t s;
        if (v->ctr++ % CTRL == 0u) {                                     /* control tick */
            if (v->kind == V_FM) {
                if (v->stage == 0u) { v->amp += v->att; if (v->amp >= v->peak) { v->amp = v->peak; v->stage = 1; } }
                else if (v->stage == 1u) v->amp = v->sus + (((v->amp - v->sus) * v->dec_k) >> 15);
                else v->amp = (v->amp * v->rel_k) >> 15;
                if ((v->idx_step > 0 && v->idx < v->idx_end) || (v->idx_step < 0 && v->idx > v->idx_end)) v->idx += v->idx_step;
                if (v->glide_k) {
                    int32_t d = (int32_t)(v->target - v->inc);
                    v->inc += (uint32_t)(int32_t)(((int64_t)d * v->glide_k) >> 15);
                }
                if (fx[v->lane].porta || v->fx_porta) {                     /* PORTA (and the effect column's): up or down, up to an octave a second */
                    int32_t pt = fx[v->lane].porta + v->fx_porta;
                    v->inc += (uint32_t)(int32_t)(((int64_t)v->inc * pt * 21) >> 22);
                    v->target += (uint32_t)(int32_t)(((int64_t)v->target * pt * 21) >> 22);
                    if (v->inc < HZ_INC(20)) v->inc = v->target = HZ_INC(20);
                    if (v->inc > 400000000u) v->inc = v->target = 400000000u;
                }
                if (v->chaos == 1u) {
                    int32_t to = v->stage == 2u ? 0 : v->peak;
                    v->pamp += (int32_t)(((int64_t)(to - v->pamp) * (to > v->pamp ? 68 : 34)) >> 15);   /* per 32 samples: tau 350 ms in, 700 ms out */
                }
                if (v->arp_n > 1u && (v->arp_t += CTRL) >= 1852u) { v->arp_t = 0; v->arp_i = (uint8_t)((v->arp_i + 1u) % v->arp_n); v->inc = v->target = v->arp[v->arp_i]; }
            } else if (v->kind >= V_WASH) {                               /* sfx */
                switch (v->kind) {
                case V_WASH: case V_CRACKLE: case V_RISER: case V_SURF: case V_GALE:
                    if (v->stage == 0u) { v->amp += v->att; if (v->amp >= v->peak) { v->amp = v->peak; v->stage = 1; } }
                    else if (v->stage == 2u) v->amp = (v->amp * v->rel_k) >> 15;
                    if (v->kind == V_WASH) v->tph += 3000u * CTRL;                         /* the sweep: 0.03 Hz.. */
                    if (v->kind == V_SURF) v->tph += 58400u * CTRL;                        /* a wave every 1.7 s */
                    if (v->kind == V_GALE) { v->tph += 10700u * CTRL; v->pph1 += 36000u * CTRL; }   /* 0.11 and 0.37 Hz */
                    if (v->kind == V_RISER) {
                        v->inc += (v->inc >> 16) * (uint32_t)v->idx;
                        if (v->inc > 400000000u) v->inc = 400000000u;
                        v->s3 = 800 + (int32_t)((uint32_t)v->amp * 20000u / (uint32_t)(v->peak ? v->peak : 1));   /* the noise opens up */
                    }
                    break;
                case V_IMPACT:
                    v->amp = (v->amp * v->dec_k) >> 15; v->tone_amp = (v->tone_amp * 29500) >> 15;
                    v->inc = v->inc_lo + (uint32_t)(((uint64_t)(v->inc - v->inc_lo) * v->inc_k) >> 15);
                    break;
                case V_ZAP:
                    v->amp = (v->amp * v->dec_k) >> 15;
                    v->inc = v->inc_lo + (uint32_t)(((uint64_t)(v->inc - v->inc_lo) * v->inc_k) >> 15);
                    break;
                default: v->amp = (v->amp * v->dec_k) >> 15; break;
                }
            } else if (v->kind == V_CLAP) {                                /* three bursts 10 ms apart, then the tail */
                if (v->ctr < 882u + CTRL && v->ctr % 441u < CTRL) v->amp = v->peak;
                else v->amp = (v->amp * (v->ctr < 882u ? (int32_t)v->inc_k : v->dec_k)) >> 15;
            } else {
                v->amp = (v->amp * v->dec_k) >> 15;
                if (v->kind == V_TOM) v->tone_amp = (v->tone_amp * 26000) >> 15;
                if (v->kind == V_KICK) v->tone_amp = (v->tone_amp * 12000) >> 15;   /* the click: a few milliseconds */
                if (v->kind == V_RIM) v->tone_amp = (v->tone_amp * 20000) >> 15;
                if (v->kind == V_KICK || v->kind == V_KETTLE || v->kind == V_TOM)
                    v->inc = v->inc_lo + (uint32_t)(((uint64_t)(v->inc - v->inc_lo) * v->inc_k) >> 15);
                if (v->kind == V_SNARE) v->tone_amp = (v->tone_amp * 31000) >> 15;
            }
            if (v->gate != 0xFFFFFFFFu) {
                if (v->gate > CTRL) v->gate -= CTRL;
                else if (v->stage != 2u) {
                    v->gate = 0; v->stage = 2;
                    if (v->midi) { mo_push(0x80u | v->midi_ch, v->midi, 0); v->midi = 0; }
                }
            }
            if (v->amp < 4 && (v->stage == 2u || (v->kind != V_FM && v->kind < V_WASH)) && (v->chaos != 1u || v->pamp < 4)) {
                if (v->midi) { mo_push(0x80u | v->midi_ch, v->midi, 0); v->midi = 0; }
                v->kind = V_OFF; return;
            }
        }
        switch (v->kind) {
        case V_FM: {
            uint32_t inc = v->inc, off;
            {                                                           /* vibrato: the patch's after 0.35 s (0.7 %), VIBRATO's from the start, 5.6 Hz */
                int32_t vd = (v->vib && v->ctr > 15435u ? 512 : 0) + (fx[v->lane].vibr + v->fx_vib) * 20;
                if (vd) {
                    v->vph += 545400u;
                    inc += (uint32_t)((((int32_t)(inc >> 16) * (SIN[v->vph >> 22] >> 4)) >> 11) * vd);
                }
            }
            if (v->chaos == 5u) {                                       /* AH VOX: two saws a little apart, a breath, through the 'ah' formants */
                int32_t x, hp;
                v->ph += inc; v->pph1 += inc + (inc >> 7);              /* 13 cents apart: two voices */
                x = (((int32_t)(v->ph >> 16) - 32768) + ((int32_t)(v->pph1 >> 16) - 32768)) >> 2;
                x += noise() >> 4;
                v->s1 += (3494 * v->s2) >> 15;                          /* F1 ~ 750 Hz */
                hp = x - v->s1 - ((11000 * v->s2) >> 15);
                v->s2 += (3494 * hp) >> 15;
                v->s3 += (5591 * v->brown) >> 15;                       /* F2 ~ 1200 Hz */
                hp = x - v->s3 - ((11000 * v->brown) >> 15);
                v->brown += (5591 * hp) >> 15;
                s = (((v->s2 + ((v->brown * 5) >> 3)) >> 1) * v->amp) >> 15;
                break;
            }
            if (v->chaos == 3u) {                                       /* SUPERSAW: the note's saw and six around it */
                static const int16_t DT[6] = {-2180, -1230, -420, 420, 1230, 2180};   /* Q16: -3.3 % .. +3.3 % */
                int32_t q = (int32_t)(inc >> 16), sum, j;
                v->ph += inc;
                sum = ((int32_t)(v->ph >> 16) - 32768) * 3 / 2;         /* the middle one a little louder */
                for (j = 0; j < 6; j++) { v->ss[j] += inc + (uint32_t)(q * DT[j]); sum += (int32_t)(v->ss[j] >> 16) - 32768; }
                v->s1 += (((sum >> 2) - v->s1) * 21000) >> 15;          /* a soft high cut: less fizz, less aliasing */
                s = (v->s1 * v->amp) >> 15;
                break;
            }
            v->mph += (inc >> 4) * v->ratio16;
            v->ph += inc;
            if (v->cw | v->mw) {                                        /* the other waveforms */
                off = (uint32_t)(WVQ(v->mw, v->mph) * v->idx) << 6;
                s = WVQ(v->cw, v->ph + off) - v->cdc;
            } else {
                off = (uint32_t)(SIN[v->mph >> 22] * v->idx) << 6;
                s = SIN[(v->ph + off) >> 22];
            }
            if (v->square) s = s >= 0 ? 26000 : -26000;
            s = (s * v->amp) >> 15;
            if (v->breath) {                                            /* bow / breath: low-passed noise at the note's level */
                v->s1 += ((noise() - v->s1) * 9000) >> 15;
                s += (((v->s1 * v->breath) >> 8) * v->amp) >> 14;
            }
            if (v->chaos == 4u) {                                       /* ORCH HIT: the fifth, the octave and the tenth above, as brassy */
                int32_t e;
                uint32_t q;
                v->pph1 += inc + (inc >> 1);                            /* 3:2 */
                v->pph2 += inc << 1;                                    /* 2:1 */
                v->ss[0] += (inc << 1) + (inc >> 1);                    /* 5:2, the major tenth */
                q = (uint32_t)(v->idx > 0 ? v->idx : 0) << 6;
                e = SIN[(v->pph1 + (uint32_t)(SIN[v->pph1 >> 22] * (int32_t)(q >> 6)) * 64u) >> 22]
                  + SIN[(v->pph2 + (uint32_t)(SIN[v->pph2 >> 22] * (int32_t)(q >> 7)) * 64u) >> 22]
                  + (SIN[(v->ss[0] + (uint32_t)(SIN[v->ss[0] >> 22] * (int32_t)(q >> 7)) * 64u) >> 22] >> 1);
                s = ((s * 3) >> 3) + ((((e * 3) >> 3) * v->amp) >> 15);
                if (v->ss[1] < 4410u) { v->ss[1]++; s += (((noise() * (int32_t)(4410u - v->ss[1])) >> 12) * v->amp) >> 16; }   /* the attack's burst, 0.1 s */
            } else if (v->chaos == 2u) {                                /* an ensemble: two more players, a little out */
                int32_t e;
                v->pph1 += inc + (inc >> 8);
                v->pph2 += inc - (inc >> 8) - (inc >> 10);
                if (v->cw) e = WVQ(v->cw, v->pph1) + WVQ(v->cw, v->pph2) - 2 * v->cdc;   /* the players in the patch's waveform */
                else e = SIN[(v->pph1 + ((uint32_t)(int32_t)SIN[v->pph1 >> 22] << 14)) >> 22]
                       + SIN[(v->pph2 + ((uint32_t)(int32_t)SIN[v->pph2 >> 22] << 14)) >> 22];
                s = ((s * 5) >> 3) + ((((e * 3) >> 3) * v->amp) >> 15);
            } else if (v->chaos) {                                      /* the light lead under a heavy pad and noise */
                int32_t pad, br;
                v->pph1 += inc - (inc >> 8);                            /* 7 cents flat */
                v->pph2 += (inc >> 1) + (inc >> 9);                     /* an octave down, 3 cents sharp */
                pad = (SIN[(v->pph1 + ((uint32_t)(int32_t)SIN[v->pph1 >> 22] << 13)) >> 22]
                     + SIN[(v->pph2 + ((uint32_t)(int32_t)SIN[v->pph2 >> 22] << 13)) >> 22]) >> 1;
                v->brown += (noise() >> 3) - (v->brown >> 5);           /* leaky integral of white: brown */
                br = v->brown > 32767 ? 32767 : v->brown < -32767 ? -32767 : v->brown;
                s = ((s * 3) >> 3) + ((((pad * 5) >> 3) * v->pamp) >> 15) + (((br >> 2) * v->pamp) >> 15);
            }
            break; }
        case V_KICK: case V_KETTLE:
            v->ph += v->inc;
            s = (SIN[v->ph >> 22] * v->amp + noise() * v->tone_amp) >> 15;   /* + the click of a sharp kit */
            break;
        case V_TOM:                                                     /* a swept sine and a little stick noise */
            v->ph += v->inc;
            s = (SIN[v->ph >> 22] * v->amp + noise() * v->tone_amp) >> 15;
            break;
        case V_RIM: {                                                   /* two sines, inharmonic, and a click */
            v->ph += HZ_INC(1667); v->tph += HZ_INC(455);
            v->s1 += ((noise() - v->s1) * 20000) >> 15;
            s = (((SIN[v->ph >> 22] + (SIN[v->tph >> 22] >> 1)) * v->amp) >> 15) + ((v->s1 * v->tone_amp) >> 15);
            break; }
        case V_CLAP: {                                                  /* band-passed noise, wide, around 1.1 kHz */
            int32_t hp;
            v->s1 += (5137 * v->s2) >> 15;
            hp = noise() - v->s1 - ((26000 * v->s2) >> 15);
            v->s2 += (5137 * hp) >> 15;
            s = (v->s2 * v->amp) >> 14;
            break; }
        case V_SNARE: {
            /* band-pass noise (state variable, f = 2 sin(pi fc / fs) ~ fc * 4.67 in Q15), plus a 190 Hz triangle */
            int32_t f = (int32_t)v->inc * 467 / 100, x = noise(), hp;
            v->s1 += (f * v->s2) >> 15;                 /* low */
            hp = x - v->s1 - ((40960 * v->s2) >> 15);
            v->s2 += (f * hp) >> 15;                    /* band */
            v->tph += HZ_INC(190);
            {
                int32_t tri = (int32_t)(v->tph >> 16) - 32768;
                tri = (tri < 0 ? -tri : tri) * 2 - 32768;
                v->s3 += ((v->s2 - v->s3) * 11000) >> 15;              /* the hiss off: ~3 kHz */
                s = (((v->breath ? v->s2 : v->s3) * v->amp) >> 15) + ((tri * v->tone_amp) >> 15);   /* sharp: the crack left in */
            }
            break; }
        case V_HAT: case V_CRASH: case V_OHAT: {
            int32_t x = noise(), hp = x - v->s1;            /* one-pole high-pass, then a low-pass: metal, not hiss */
            v->s1 += ((x - v->s1) * (v->kind != V_CRASH ? 6000 : 9000)) >> 15;
            v->s2 += ((hp - v->s2) * (v->kind != V_CRASH ? 16000 : 11000)) >> 15;
            v->s3 += ((v->s2 - v->s3) * (v->kind != V_CRASH ? 16000 : 11000)) >> 15;   /* two poles */
            s = ((v->breath && v->kind != V_CRASH ? v->s2 : v->s3) * v->amp) >> 13;   /* sharp: the hats one pole, brighter */
            break; }
        case V_WASH: {                                                  /* two-pole low-pass noise, swept */
            int32_t c = 1200 + (((int32_t)SIN[v->tph >> 22] + 32768) * 5000 >> 16), x = noise();
            v->s1 += ((x - v->s1) * c) >> 15;
            v->s2 += ((v->s1 - v->s2) * c) >> 15;
            s = (v->s2 * 2 * v->amp) >> 15;
            break; }
        case V_RISER: {
            int32_t x = noise();
            v->s1 += ((x - v->s1) * v->s3) >> 15;
            v->ph += v->inc;
            s = ((v->s1 + (SIN[v->ph >> 22] >> 1)) * v->amp) >> 15;
            break; }
        case V_IMPACT:
            v->ph += v->inc;
            s = (SIN[v->ph >> 22] * v->amp + noise() * v->tone_amp) >> 15;
            break;
        case V_ZAP:
            v->ph += v->inc;
            s = (SIN[v->ph >> 22] * v->amp) >> 15;
            break;
        case V_CRACKLE: {
            int32_t x = noise();
            if (((noise_s >> 9) & 1023u) == 0u) v->s1 = x * 2;         /* a click, ~43 a second */
            v->s1 = (v->s1 * 26000) >> 15;
            s = ((v->s1 + (x >> 5)) * v->amp) >> 15;
            break; }
        case V_SURF: {                                                  /* brown noise, low-passed, swelling in waves */
            int32_t w = (SIN[v->tph >> 22] + 32768) >> 1, c = 900 + (w >> 3), a;   /* w 0..32767; brighter on the crest */
            v->brown += (noise() >> 3) - (v->brown >> 5);
            v->s1 += ((v->brown - v->s1) * c) >> 15;
            a = (v->amp * (6000 + ((w * 26767) >> 15))) >> 15;
            s = (v->s1 * a) >> 14;
            break; }
        case V_GALE: {                                                  /* white noise in a resonant band-pass that wanders */
            int32_t g = (SIN[v->tph >> 22] + SIN[v->pph1 >> 22] + 65536) >> 2, f, hp;   /* 0..32767 */
            f = (350 + (g * 1300 >> 15)) * 467 / 100;                   /* 350 .. 1650 Hz */
            v->s1 += (f * v->s2) >> 15;
            hp = noise() - v->s1 - ((9000 * v->s2) >> 15);              /* low damping: it whistles */
            v->s2 += (f * hp) >> 15;
            s = (((v->s2 >> 1) * v->amp) >> 15) * (8000 + (g * 24767 >> 15)) >> 15;
            break; }
        case V_BITS: {
            uint32_t was = v->ph;
            v->ph += v->inc;
            if (v->ph < was) { uint32_t bit = ((uint32_t)v->s1 ^ ((uint32_t)v->s1 >> 6)) & 1u; v->s1 = (int32_t)(((uint32_t)v->s1 >> 1) | bit << 14); }
            s = ((v->s1 & 1) ? v->amp : -v->amp);
            break; }
        default: return;
        }
        out[i] += s;
    }
}

/* the lane's effects on its bus, once per block (parameters per block, filter per sample) */
static void lane_fx(uint32_t slot, int32_t *b, uint32_t n)        /* the effects of bus slot (its lane's settings) */
{
    uint32_t l = BUS_LANE[slot];
    const lanefx_t *F = &fx[l];
    uint32_t i;
    int32_t drive = F->drive, filt = F->filt, wob = F->wob, crush = F->crush;
    if (drive > 0) {
        int32_t g = 256 + drive * 20, mk = 256 - drive * 90 / 100;
        for (i = 0; i < n; i++) {
            int32_t v = (b[i] * g) >> 8, v3;
            if (v > 32767) v = 32767; else if (v < -32767) v = -32767;
            v3 = (((v * v) >> 15) * v) >> 15;
            b[i] = ((v - v3 / 3) * mk) >> 8;
        }
    }
    if (filt != 0 || wob > 0) {
        /* cutoff as a note (Q4 sixteenths), wobble +-24 semitones, then f = 2 pi fc / fs from the phase step */
        static const uint32_t WOBF_Q8[6] = {0, 64, 128, 256, 512, 1024};   /* cycles per beat, Q8 */
        int32_t note16, lp = filt <= 0, f, damp;
        uint32_t inc;
        if (filt < 0) note16 = (130 * 16) + filt * (95 * 16) / 100;       /* -100 -> note 35 (~60 Hz) */
        else if (filt > 0) note16 = (16 * 16) + filt * (88 * 16) / 100;   /* 100 -> note 104 (~5.3 kHz) */
        else note16 = 82 * 16;                                            /* centred with wobble: ~1.8 kHz */
        if (wob > 0) {
            if (slot == l) wob_ph[l] += (uint32_t)(((uint64_t)WOBF_Q8[wob] * (uint32_t)seq_bpm * n * 1623u) >> 8);   /* 2^32/(60*44100) = 1623.1; the right bus follows */
            note16 += (SIN[wob_ph[l] >> 22] * (24 * 16)) >> 15;
        }
        if (lp && note16 >= 128 * 16) goto skip_filter;
        if (note16 < 16 * 16) note16 = 16 * 16;
        if (note16 > 118 * 16) note16 = 118 * 16;
        inc = note_inc(note16 >> 4);
        inc += (uint32_t)(((uint64_t)(note_inc((note16 >> 4) + 1) - inc) * (uint32_t)(note16 & 15)) >> 4);
        f = (int32_t)(((uint64_t)inc * 205887u) >> 32);
        if (f > 26000) f = 26000;
        damp = 65536 - F->res * 590;                                      /* 2 .. 0.2 (Q15) */
        if (damp < 6553) damp = 6553;
        {
            int32_t lo = f_low[slot], bd = f_band[slot];
            for (i = 0; i < n; i++) {
                int32_t hp;
                int32_t x = b[i];                                 /* 32-bit products: states kept within 17 bits */
                if (x > 65535) x = 65535; else if (x < -65535) x = -65535;
                lo += (f * bd) >> 15;
                hp = x - lo - ((damp >> 1) * bd >> 14);
                if (hp > 65535) hp = 65535; else if (hp < -65535) hp = -65535;
                bd += (f * hp) >> 15;
                if (bd > 65535) bd = 65535; else if (bd < -65535) bd = -65535;
                if (lo > 65535) lo = 65535; else if (lo < -65535) lo = -65535;
                b[i] = lp ? lo : hp;
            }
            f_low[slot] = lo; f_band[slot] = bd;
        }
    }
skip_filter:
    if (crush > 0) {
        int32_t sh = crush * 12 / 100, hold = 1 + crush * crush * 15 / 10000;
        for (i = 0; i < n; i++) {
            if (--crush_n[slot] <= 0) { crush_n[slot] = hold; crush_hold[slot] = (b[i] >> sh) << sh; }
            b[i] = crush_hold[slot];
        }
    }
}

static void render_span(uint32_t from, uint32_t n)
{
    uint32_t v;
    for (v = 0; v < NVOICE; v++) {
        const voice_t *x = &vc[v];
        uint32_t b = x->lane;
        if (x->kind == V_OFF) continue;
        if ((b == L_PAD || b == L_SFX) && (v & 1u)) b = b == L_PAD ? B_PADR : B_SFXR;   /* every other voice: right */
        else if (x->kind == V_HAT || x->kind == V_CRASH || x->kind == V_OHAT) b = B_HATS;
        render_voice(&vc[v], &bus[b][from], n);
    }
}

static void all_off(void)
{
    uint32_t v;
    for (v = 0; v < NVOICE; v++) {
        if (vc[v].kind != V_OFF && vc[v].midi) mo_push(0x80u | vc[v].midi_ch, vc[v].midi, 0);
        if (vc[v].kind == V_FM) { vc[v].stage = 2; vc[v].gate = 0; vc[v].midi = 0; }
    }
    npend = 0;
}

/* the keys, played live: the main loop queues key down / up, the interrupt starts and releases (only it touches
 * the voices). A melodic note sounds until its key comes up; drums and sfx are hits. */
typedef struct { uint8_t lane, note, key, on, inst, n2, n3, lvl, ext; } live_t;   /* n2, n3: a chord's other notes (0 = none); lvl Q8;
                                                                               * ext: from MIDI in (not sent back out) */
static uint32_t live_lvl = 230u, live_ext;      /* the next key's level (MIDI in: its velocity), its source */
static volatile live_t live_q[16];
static volatile uint32_t live_w, live_r;
static voice_t *live_v[27][3];
static uint32_t live_age[27][3];
static void live_push3(uint32_t lane, uint32_t note, uint32_t n2, uint32_t n3, uint32_t key, uint32_t on, uint32_t inst)
{
    live_t *e;
    if (live_w - live_r >= 16u) return;
    e = (live_t *)&live_q[live_w % 16u];
    e->lane = (uint8_t)lane; e->note = (uint8_t)note; e->key = (uint8_t)key; e->on = (uint8_t)on; e->inst = (uint8_t)inst;
    e->n2 = (uint8_t)n2; e->n3 = (uint8_t)n3; e->lvl = (uint8_t)(live_lvl > 255u ? 255u : live_lvl); e->ext = (uint8_t)live_ext;
    RING_PUBLISH();
    live_w++;
}
static void live_push(uint32_t lane, uint32_t note, uint32_t key, uint32_t on, uint32_t inst) { live_push3(lane, note, 0, 0, key, on, inst); }
static void live_run(void)
{
    while (live_r != live_w) {
        const live_t *e = (const live_t *)&live_q[live_r % 16u];
        uint32_t k = e->key < 27u ? e->key : 0u;
        if (e->on && fx[e->lane].level <= 0) { live_r++; continue; }   /* a silent lane: nothing to start */
        if (e->on) {
            if (e->lane == L_DRUMS) start_drum(e->note, e->lvl, e->inst, 0);
            else if (e->lane == L_SFX) start_sfx(e->note, (uint32_t)(62 + key_off), 44100, 230);
            else {
                const uint8_t nn[3] = {e->note, e->n2, e->n3};
                uint32_t j, lv = e->n2 ? e->lvl * 160u / 230u : e->lvl;   /* a chord: quieter per voice */
                for (j = 0; j < 3u; j++) {
                    live_v[k][j] = 0;
                    if (!nn[j]) continue;
                    start_fm(e->lane, &PATCH[e->inst < NPATCH ? e->inst : 0], note_inc(nn[j]), 0xFFFFFFF0u, lv, 0, 0, 0, e->ext ? 0u : nn[j]);
                    if (e->ext) last_voice->key = nn[j];      /* the keys still light it */
                    live_v[k][j] = last_voice; live_age[k][j] = last_voice->age;
                }
            }
            lane_hit_ms[e->lane] = fm1_ms;
        } else {
            uint32_t j;
            for (j = 0; j < 3u; j++)
                if (live_v[k][j] && live_v[k][j]->age == live_age[k][j] && live_v[k][j]->kind == V_FM && live_v[k][j]->stage != 2u) {
                    live_v[k][j]->gate = 1;                         /* released on the next control tick */
                    live_v[k][j] = 0;
                }
        }
        live_r++;
    }
}

/* hearing a note while editing: the main loop asks, the interrupt plays it (only it touches the voices) */
static volatile uint32_t aud_req, aud_lane, aud_note, aud_kind;
static void audition(uint32_t lane, uint32_t note, uint32_t kind)
{
    aud_lane = lane; aud_note = note; aud_kind = kind;
    RING_PUBLISH();
    aud_req = 1;
}

/* the pitch a voice is at against its note, in cents (PORTA and the slides: the bend) */
static const uint16_t SEMI_Q12[13] = {4096, 4340, 4598, 4871, 5161, 5468, 5793, 6137, 6502, 6889, 7298, 7732, 8192};
static int32_t cents_of(uint32_t inc, uint32_t base)
{
    uint32_t b = base >> 12, r;
    int32_t oct = 0, k;
    if (!b) return 0;
    r = inc / b;                                                    /* Q12 */
    while (r >= 8192u) { r >>= 1; oct++; }
    while (r < 4096u && oct > -8) { r <<= 1; oct--; }
    for (k = 11; k > 0 && SEMI_Q12[k] > r; k--) ;
    return oct * 1200 + k * 100 + (int32_t)((r - SEMI_Q12[k]) * 100u / (uint32_t)(SEMI_Q12[k + 1] - SEMI_Q12[k]));
}
static uint32_t clk_k;                                  /* clock ticks sent in this step (six a 16th: 24 a beat) */
static uint8_t cc11_now[NLANE] = {127, 127, 127, 127, 127, 127}, midi_blk;
static void midi_lanes(void)                            /* ISR, each block: the bends, the tremolo as expression */
{
    uint32_t l, v;
    if (!cc_out_on) return;
    midi_blk++;
    for (l = 0; l < NLANE; l++) {
        uint32_t ch = midi_chan[l];
        if (ch > 15u || l == L_DRUMS || l == L_SFX) continue;
        {                                                           /* the newest note's bend: PORTA, the slides up and down */
            const voice_t *nv = 0;
            for (v = VLO[l]; v < VHI[l]; v++) if (vc[v].kind == V_FM && vc[v].midi && vc[v].arp_n <= 1u && vc[v].stage != 2u && (!nv || (int32_t)(vc[v].age - nv->age) > 0)) nv = &vc[v];
            if (nv && (fx[l].porta || nv->fx_porta || bend_now[l] != 8192u)) {
                int32_t c = cents_of(nv->target, note_inc(nv->midi)), b = 8192 + c * 8192 / 1200;   /* the range: 12 semitones */
                b = b < 0 ? 0 : b > 16383 ? 16383 : b;
                if (b - (int32_t)bend_now[l] >= 32 || (int32_t)bend_now[l] - b >= 32) { mo_push(0xE0u | ch, (uint32_t)b & 127u, (uint32_t)b >> 7); bend_now[l] = (uint16_t)b; }
            }
        }
        if (midi_blk % 3u == l % 3u) {                              /* TREMOLO, TREMOR: expression, every 3rd block */
            int32_t e = fx[l].trem > 0 || fx[l].tremor > 0 ? trem_g[l] * 127 / 32768 : 127, d = e - cc11_now[l];
            if (d >= 4 || d <= -4 || (e == 127 && cc11_now[l] != 127) || (e == 0 && cc11_now[l])) { mo_push(0xB0u | ch, 11, (uint32_t)e); cc11_now[l] = (uint8_t)e; }
        }
    }
}
static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t t0 = fm1_ticks(), i = 0, l, k;
    uint32_t blk_step0 = seq_step, blk_acc0 = seq_acc_q8, blk_smp0 = smp;   /* where the block starts (TREMOLO, TREMOR) */
    for (l = 0; l < NBUS; l++)
        for (k = 0; k < n; k++) bus[l][k] = 0;
    if (aud_req) { uint8_t nt[3] = {(uint8_t)aud_note, (uint8_t)(62 + key_off), 0}; fire(aud_lane, nt, 1, 0, aud_lane == L_SFX ? 33075u : 9000u, 256, aud_kind, 0); aud_req = 0; }
    mm_drain();
    live_run();
    {
        int clk = clk_out_on && !clk_ext;
        if (seq_stop_req) { seq_stop_req = 0; seq_playing = 0; all_off(); if (clk) mo_push(0xFCu, 0, 0); }
        if (seq_start_req) {
            seq_start_req = 0; seq_playing = 1; seq_step = (uint32_t)seq_start_step; seq_acc_q8 = 0; npend = 0; clk_k = 0; seq_nudge_q8 = 0;
            if (clk) {                                                  /* start from the top, or continue at the position */
                if (seq_step) { mo_push(0xF2u, seq_step & 127u, seq_step >> 7 & 127u); mo_push(0xFBu, 0, 0); }
                else mo_push(0xFAu, 0, 0);
            }
            step_fire(seq_step);
        }
    }
    if (seq_playing && seq_nudge_q8) {                              /* the clock in pulls us to its beat */
        int32_t nd = seq_nudge_q8;
        seq_nudge_q8 = 0;
        if (nd < 0 && (uint32_t)-nd > seq_acc_q8) nd = -(int32_t)seq_acc_q8;
        seq_acc_q8 += (uint32_t)nd;
    }
    while (i < n) {
        uint32_t span = n - i, p, ct = seq_step_len_q8 / 6u;
        if (seq_playing && clk_out_on && !clk_ext) {                /* MIDI clock out: 24 a beat, sample-accurate */
            while (clk_k < 6u && seq_acc_q8 >= clk_k * ct) { mo_push(0xF8u, 0, 0); clk_k++; }
            if (clk_k < 6u) { uint32_t lt = (clk_k * ct - seq_acc_q8 + 255u) >> 8; if (lt < span) span = lt ? lt : 1u; }
        }
        if (seq_playing) {
            uint32_t left = (seq_step_len_q8 - seq_acc_q8 + 255u) >> 8;   /* samples to the next step */
            if (left < span) span = left ? left : 1u;
        }
        for (p = 0; p < npend; p++) {                                       /* a waiting repeat comes due */
            uint32_t d = pend[p].when - smp;
            if ((int32_t)d <= 0) { span = 0; break; }
            if (d < span) span = d;
        }
        if (span) {
            render_span(i, span);
            i += span; smp += span;
            if (seq_playing) seq_acc_q8 += span << 8;
        }
        for (p = 0; p < npend;) {                                           /* fire what is due */
            if ((int32_t)(pend[p].when - smp) <= 0) {
                note_fx(pend[p].fx);
                fire(pend[p].lane, pend[p].note, pend[p].n, pend[p].flags, pend[p].dur, pend[p].lvl, pend[p].kind, pend[p].orig);
                note_porta = note_vib = 0;
                pend[p] = pend[--npend];
            } else p++;
        }
        while (seq_playing && seq_acc_q8 >= seq_step_len_q8) {
            if (clk_out_on && !clk_ext) while (clk_k < 6u) { mo_push(0xF8u, 0, 0); clk_k++; }   /* none lost to a nudge */
            seq_acc_q8 -= seq_step_len_q8;
            seq_step++;
            clk_k = 0;
            step_fire(seq_step);
        }
    }
    for (k = 0; k < n; k++) sendb[k] = 0;
    for (l = 0; l < NBUS; l++) {
        const lanefx_t *F = &fx[BUS_LANE[l]];
        int32_t lv = F->level * 480 / 100, pn = F->pan + PAN_OFF[l], sd = F->send * 256 / 100;   /* Q8: 80 -> 1.5 */
        int32_t tl, tr, gl, gr, dl, dr;
        if (F->autopan > 0 && !F->aprate) pn += ap_side[BUS_LANE[l]] ? F->autopan : -F->autopan;   /* AUTOPAN: this note's side */
        else if (F->autopan > 0) {                                      /* or a sweep, in time: where it is as the block ends */
            uint32_t L = seq_step_len_q8 >> 8, per = AP_STEPS[F->aprate & 7] * (L ? L : 1u), t;
            t = seq_playing ? (blk_step0 % AP_STEPS[F->aprate & 7]) * L + (blk_acc0 >> 8) + n : blk_smp0 + n;
            pn += F->autopan * SIN[(t % per) * 1024u / per & 1023u] >> 15;   /* starts in the middle, going right */
        }
        if (pn < -100) pn = -100; else if (pn > 100) pn = 100;
        tl = pan_gain(pn, 0); tr = pan_gain(pn, 1);
        gl = pan_cl[l] << 16; gr = pan_cr[l] << 16;                     /* glide to the new place over the block: no clicks */
        dl = (tl - pan_cl[l]) * 65536 / (int32_t)(n ? n : 1u); dr = (tr - pan_cr[l]) * 65536 / (int32_t)(n ? n : 1u);
        pan_cl[l] = tl; pan_cr[l] = tr;
        if (F->level <= 0) { trem_g[l] = 32768; continue; }        /* faded out: nothing to hear, nothing to work */
        lane_fx(l, bus[l], n);
        if (F->trem > 0 || F->tremor > 0) {                             /* TREMOLO and TREMOR, in time with the beat */
            uint32_t L = seq_step_len_q8 >> 8, t0 = seq_playing ? (blk_step0 & 1u) * L + (blk_acc0 >> 8) : blk_smp0 % (2u * (L ? L : 1u));
            uint32_t per = F->tremor == 1 ? 2u * L : F->tremor == 2 ? L : L / 2u, depth = (uint32_t)F->trem * 327u;
            int32_t *g = &trem_g[l];
            if (!L) L = 1;
            if (!per) per = 1;
            for (k = 0; k < n; k++) {
                uint32_t t = t0 + k, target = 32768u;
                if (depth) target -= (depth * (uint32_t)(32768 - SIN[((t % (2u * L)) * 1024u / (2u * L) + 256u) & 1023u]) >> 16);   /* a cycle each 8th */
                if (F->tremor > 0 && (t % per) >= per / 2u) target = 0;
                *g += ((int32_t)target - *g) >> 5;                      /* no clicks */
                bus[l][k] = (int32_t)(((int64_t)bus[l][k] * *g) >> 15);
            }
        } else trem_g[l] = 32768;
        for (k = 0; k < n; k++) {
            int32_t s = (bus[l][k] * lv) >> 8;
            if (!(k & 3u) && (s < 0 ? -s : s) > lane_pk[BUS_LANE[l]]) lane_pk[BUS_LANE[l]] = s < 0 ? -s : s;   /* for LANES: every 4th */
            out[2u * k] += (s * (gl >> 16)) >> 8;
            out[2u * k + 1u] += (s * (gr >> 16)) >> 8;
            gl += dl; gr += dr;
            sendb[k] += (s * sd) >> 8;
        }
    }
    room(out, n);
    for (k = 0; k < 2u * n; k++) {                                           /* a soft knee above -3 dB */
        int32_t s = out[k];
        if (s > 23000) s = 23000 + ((s - 23000) >> 2); else if (s < -23000) s = -23000 + ((s + 23000) >> 2);
        if (s > 30000) s = 30000 + ((s - 30000) >> 3); else if (s < -30000) s = -30000 + ((s + 30000) >> 3);   /* and harder */
        out[k] = s;
        if ((s < 0 ? -s : s) > out_pk[k & 1u]) out_pk[k & 1u] = s < 0 ? -s : s;
    }
    for (k = 0; k < n; k += 4u) { uint32_t j = scope_w & 255u; scope_l[j] = (int16_t)(out[2u * k] >> 1); scope_r[j] = (int16_t)(out[2u * k + 1u] >> 1); scope_w++; }
    {                                                                       /* the budget in use (VOICES) */
        uint32_t v; int32_t t = 0;
        for (v = 0; v < NVOICE; v++) if (vc[v].kind != V_OFF) t += vc[v].wt;
        voice_now = t; if (t > voice_peak) voice_peak = t;
    }
    midi_lanes();
    pos_q8 = (int32_t)(seq_step * 256u + (seq_step_len_q8 >> 8 ? (seq_acc_q8 >> 8) * 256u / (seq_step_len_q8 >> 8) : 0u)); pos_ticks = clk_now();   /* for the clock in */
    vis_step = seq_step;
    vis_frac = seq_step_len_q8 ? (seq_acc_q8 >> 8) * 65536u / (seq_step_len_q8 >> 8 ? seq_step_len_q8 >> 8 : 1u) : 0u;
    isr_us_last = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
    if (isr_us_last > isr_us_max) isr_us_max = isr_us_last;
    isr_us_sum += isr_us_last; isr_n++;
}
