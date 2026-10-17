/* SPDX-License-Identifier: GPL-3.0-only */
/* Espada's sound: runs in the audio interrupt (44.1 kHz, 256 frames a call). Integers only.
 *
 *   FM voices     two operators (a modulator into a carrier's phase), an index envelope, an amplitude
 *                 envelope, slide, vibrato, the tracker arpeggio (a chord cycled every 42 ms), square option;
 *                 CHAOS layers a light saw lead, a heavy pad (two detuned oscillators, one an octave down,
 *                 fading in slowly and out late) and brown noise breathing with the pad
 *   drum voices   kick and kettle drum (a swept sine), snare (band-passed noise + a tone), hat, crash
 *   sfx voices    wash (filtered noise, a slow sweep), riser (noise opening up + a tone climbing four octaves),
 *                 impact (a falling sub + a noise burst), zap (a sine falling fast), crackle (clicks + hiss),
 *                 bits (1-bit periodic noise, the 8-bit metallic kind)
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
                                 * more detuned oscillators with the note (strings and brass as sections) */
} patch_t;
static const patch_t PATCH[] = {
    {"FM BASS", 16, 1138, 260, 180,   4, 6200,  178,  80, 0, 0},
    {"SLAP",    32,  975,  81,  60,   2, 5600,  128,  60, 0, 0},
    {"PIZZ",    16,  650,  65, 100,   2, 5900,   10, 120, 0, 0},
    {"SUB",     16,   97,  32, 200,   5, 7400,  230, 100, 0, 0},
    {"BRASS",   16,  260, 1040,  90, 30, 2000,  217, 180, 1, 0,    0, 2},
    {"STRINGS", 32,  227,  97, 600, 380, 1500,  230, 650, 1, 0,    0, 2},
    {"BELL",    56,  371,  28, 800,   2, 2060,   51, 800, 0, 0},
    {"ORGAN",   32,  162, 162, 100,  10, 1770,  255,  60, 0, 0},
    {"FLUTE",   16,  130,  65, 200,  60, 2360,  230, 150, 1, 0},
    {"CHIP",    16,    0,   0, 100,   3, 1180,  230,  50, 1, 1},
    {"RAZOR",   16,  980, 560, 350,   3, 2300,  215, 110, 1, 0},   /* 1:1, a high index: saw-like, cutting */
    {"PULSE",   32,  420, 150, 250,   2, 1500,  210,  90, 1, 1},   /* 2:1 through the square: a hollow pulse */
    {"PIANO",   16,  700,  90, 450,   1, 2600,    0, 260, 0, 0, 1100},   /* 1:1, the hammer's brightness fades, rings out */
    {"E.PIANO", 224, 120,   0, 160,   1, 2500,    0, 300, 0, 0, 1500},   /* 14:1 tine on the attack, then a sine */
    {"CHAOS",   16,  520, 380, 400,   4, 3800,  230, 500, 1, 0,    0, 1},   /* a light saw + a heavy pad + brown noise */
};
#define NPATCH (sizeof PATCH / sizeof PATCH[0])

typedef struct { const char *name; uint16_t kh, kl, kd, sf, sd, hf, hd; } kit_t;   /* Hz and ms */
static const kit_t KITS[] = {
    {"STANDARD", 150, 45, 320, 1900, 170, 8000, 40},
    {"TRACKER",  220, 60, 180, 2600, 120, 6000, 70},
    {"ELECTRO",  120, 38, 600, 1200, 220, 10000, 25},
};
#define NKIT (sizeof KITS / sizeof KITS[0])

/* ---- voices */
#define CTRL 32u                                /* control rate: envelopes every 32 samples */
enum { V_OFF = 0, V_FM, V_KICK, V_SNARE, V_HAT, V_CRASH, V_KETTLE,
       V_WASH, V_RISER, V_IMPACT, V_ZAP, V_CRACKLE, V_BITS };          /* V_WASH + SX_x = the sfx voices */
typedef struct {
    uint8_t kind, lane, stage, square;          /* stage: 0 attack, 1 decay/sustain, 2 release */
    uint8_t midi, midi_ch, arp_n, arp_i;
    uint32_t ph, mph, inc, target, ratio16;     /* carrier phase / step, modulator phase */
    int32_t amp, peak, sus, att, rel_k, dec_k;  /* Q15 levels, per-tick steps and factors (Q15) */
    int32_t idx, idx_end, idx_step;             /* FM index Q8 */
    uint32_t gate, age, ctr, arp_t;             /* samples until release, ticks alive */
    int32_t glide_k, vib;                       /* glide factor Q15 (0 = none), vibrato on */
    uint32_t vph, inc_lo, inc_k;                /* vibrato phase; drum sweep target and factor */
    uint32_t arp[3];
    int32_t s1, s2, s3;                         /* filter states (snare, hat) */
    int32_t tone_amp; uint32_t tph;             /* the snare's tone */
    uint8_t chaos;                              /* CHAOS: the pad and the noise */
    uint32_t pph1, pph2;                        /* the pad's two oscillators */
    int32_t pamp, brown;                        /* the pad's level (Q15, slow), the brown noise */
} voice_t;
#define NVOICE 30u
static voice_t vc[NVOICE];
static const uint8_t VLO[NLANE] = {0, 5, 13, 17, 26}, VHI[NLANE] = {5, 13, 17, 26, 30};   /* voices per lane */
static uint32_t vage, noise_s = 22222u;

/* ---- lane buses and effects state */
/* buses: one a lane, plus right-hand buses for the pad's and sfx's every other voice and the drums' hats and
 * crash (a tracker spreads its channels left and right; this spreads the voices) */
#define B_PADR NLANE
#define B_SFXR (NLANE + 1u)
#define B_HATS (NLANE + 2u)
#define NBUS (NLANE + 3u)
static const uint8_t BUS_LANE[NBUS] = {0, 1, 2, 3, 4, L_PAD, L_SFX, L_DRUMS};   /* whose effects a bus takes */
static int32_t bus[NBUS][HALF_FRAMES];
static int32_t sendb[HALF_FRAMES];                      /* the lanes' sends to the room */
static volatile int32_t space = 35;                     /* 0..100: the room (MAIN) */
/* where each lane sits (Q8 left, right) and how much of it goes to the room (Q8): lead a little right, pad and
 * sfx mostly in the room, bass and drums in the middle and dry */
static const uint16_t PAN_L[NBUS] = {150, 256, 256, 256, 256, 80, 90, 130}, PAN_R[NBUS] = {256, 80, 256, 256, 90, 256, 256, 256};
static const uint16_t SEND[NBUS] = {90, 150, 8, 45, 160, 150, 160, 45};

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
static int32_t f_low[NBUS], f_band[NBUS], crush_hold[NBUS], crush_n[NBUS];
static uint32_t wob_ph[NLANE];
static uint32_t lane_last_inc[NLANE][3];
static volatile uint32_t lane_hit_ms[NLANE];

/* ---- the clock */
static volatile int32_t seq_playing, seq_start_req, seq_stop_req, seq_start_step;
static volatile uint32_t seq_step_len_q8 = (661500u * 256u) / 125u;   /* samples per 16th, Q8 */
static volatile int32_t seq_bpm = 125;
static volatile uint32_t vis_step, vis_frac;    /* where the play head is (frac: 0..65535 of a step) */
static volatile uint32_t isr_us_max, isr_us_last, isr_us_sum, isr_n;
static uint32_t seq_step, seq_acc_q8, smp;      /* the step playing, samples into it (Q8), samples since boot */

/* ---- retrigger repeats and echo copies: voice starts that wait for their sample time */
typedef struct { uint32_t when, dur; uint8_t lane, n, flags, lvl, kind, slide; uint8_t note[3]; } pend_t;
#define NPEND 64u
static pend_t pend[NPEND];
static uint32_t npend;

/* ---- MIDI out (the interrupt queues, the main loop sends). Names with an esp_ prefix: usb.c is in this same
 * file and has its own mo_w / mo_r, and C merges two `static` declarations of one name into one variable. Shared,
 * every midi_send() moved the counter the main loop drains to, so it never finished (a freeze on PLAY). */
static volatile uint32_t esp_mq[64];
static volatile uint32_t esp_mq_w, esp_mq_r;
static void mo_push(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (esp_mq_w - esp_mq_r < 64u) { esp_mq[esp_mq_w % 64u] = st | d1 << 8 | d2 << 16; RING_PUBLISH(); esp_mq_w++; }
}
static const uint8_t GM_DRUM[NDK] = {36, 38, 42, 47, 49};

static void synth_init(void)
{
    uint32_t i;
    for (i = 0; i < 1024u; i++) SIN[i] = (int16_t)sine_q15(i << 22);
    for (i = 0; i < NLANE; i++) { fx[i].level = 80; fx[i].bright = 25; fx[i].rtg_rate = 1; fx[i].inst = -1; fx[i].res = 30; }
    fx[L_SFX].level = 60;
}

static inline int32_t noise(void) { noise_s = noise_s * 1664525u + 1013904223u; return (int32_t)noise_s >> 17; }
static int32_t tau_k(uint32_t ms)               /* per-tick factor (Q15) of a decay with time constant ms */
{
    uint32_t smp_tau = ms * 441u / 10u;
    if (smp_tau < CTRL * 2u) smp_tau = CTRL * 2u;
    return 32768 - (int32_t)((CTRL * 32768u) / smp_tau);
}

static voice_t *alloc_voice(uint32_t lane)
{
    uint32_t v, pick = VLO[lane], best = 0xFFFFFFFFu;
    for (v = VLO[lane]; v < VHI[lane]; v++) {
        /* a free voice, else the quietest one in its release, else the oldest */
        uint32_t score = vc[v].kind == V_OFF ? 0u
                       : vc[v].stage == 2u || (vc[v].kind != V_FM && vc[v].kind < V_WASH) ? 1u + (uint32_t)vc[v].amp
                       : 0x40000000u + (vc[v].age & 0x3FFFFFFFu);
        if (score < best) { best = score; pick = v; }
    }
    if (vc[pick].kind != V_OFF && vc[pick].midi) mo_push(0x80u | vc[pick].midi_ch, vc[pick].midi, 0);
    return &vc[pick];
}

/* start one FM voice: lvl Q8, dur in samples; from_inc: slide start (0 = none) */
static voice_t *last_voice;                             /* the voice start_fm just started (the keys hold it) */
static void start_fm(uint32_t lane, const patch_t *P, uint32_t inc, uint32_t dur, uint32_t lvl, uint32_t from_inc,
                     const uint32_t *arp, uint32_t arp_n, uint32_t midi)
{
    voice_t *v = alloc_voice(lane);
    last_voice = v;
    int32_t bright = fx[lane].bright, mult = 64 + bright * 768 / 100;   /* Q8: 25 -> 1.0 */
    uint32_t a = (uint32_t)P->a_ms * 441u / 10u, i;
    v->kind = V_FM; v->lane = (uint8_t)lane; v->stage = 0; v->square = P->square;
    v->target = inc; v->inc = from_inc ? from_inc : inc; v->ratio16 = P->ratio16;
    v->ph = 0; v->mph = 0;
    v->peak = (int32_t)((P->amp * lvl) >> 8);
    v->sus = (v->peak * P->sus) >> 8;
    v->amp = 0;
    v->att = (int32_t)((uint32_t)v->peak * CTRL / (a < CTRL ? CTRL : a));
    if (v->att < 1) v->att = 1;
    v->dec_k = tau_k(P->d_ms ? P->d_ms : 80u);
    v->rel_k = tau_k(P->r_ms / 3u + 1u);
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
    v->midi = (uint8_t)midi; v->midi_ch = (uint8_t)lane;
    v->chaos = P->chaos; v->pph1 = 0x40000000u; v->pph2 = 0; v->pamp = 0; v->brown = 0;
    if (midi) mo_push(0x90u | lane, midi, 100);
}

static void start_drum(uint32_t kind, uint32_t lvl, uint32_t kit, int32_t is_orig)
{
    voice_t *v = alloc_voice(L_DRUMS);
    const kit_t *K = &KITS[kit < NKIT ? kit : 0];
    v->lane = L_DRUMS; v->stage = 1; v->ph = 0; v->ctr = 0; v->age = ++vage; v->s1 = v->s2 = v->s3 = 0;
    v->gate = 0xFFFFFFFFu; v->midi = 0; v->chaos = 0;
    switch (kind) {
    case DK_KICK: case DK_KETTLE: {
        uint32_t lo = kind == DK_KETTLE ? note_inc(38 + key_off) : HZ_INC(K->kl);
        v->kind = kind == DK_KETTLE ? V_KETTLE : V_KICK;
        v->inc_lo = lo; v->inc = kind == DK_KETTLE ? lo + (lo * 3u >> 2) : HZ_INC(K->kh);
        v->inc_k = (uint32_t)tau_k(kind == DK_KETTLE ? 20u : 30u);
        v->amp = (int32_t)(((kind == DK_KETTLE ? 10300u : 14000u) * lvl) >> 8);
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
    default:
        v->kind = V_CRASH; v->amp = (int32_t)((2950u * lvl) >> 8); v->dec_k = tau_k(900u * 10u / 69u);
        break;
    }
    v->peak = v->amp;
    if (is_orig) { v->midi = GM_DRUM[kind < NDK ? kind : 0]; v->midi_ch = 9; mo_push(0x99u, v->midi, 100); }
}

/* an sfx sound: kind SX_x, pitch a MIDI note (riser start, bits), dur in samples (wash, riser, crackle) */
static void start_sfx(uint32_t kind, uint32_t pitch, uint32_t dur, uint32_t lvl)
{
    voice_t *v = alloc_voice(L_SFX);
    if (dur < CTRL * 4u) dur = CTRL * 4u;
    v->lane = L_SFX; v->kind = (uint8_t)(V_WASH + (kind < NSX ? kind : 0)); v->ph = 0; v->ctr = 0; v->age = ++vage;
    v->s1 = v->s2 = v->s3 = 0; v->tph = 0; v->tone_amp = 0; v->midi = 0; v->gate = 0xFFFFFFFFu; v->stage = 2; v->peak = 0; v->chaos = 0;
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
        v->peak = (int32_t)((16000u * lvl) >> 8); v->amp = 0; v->stage = 0;
        v->att = v->peak * (int32_t)CTRL / 900; if (v->att < 1) v->att = 1;
        v->gate = dur; v->rel_k = tau_k(150);
        break;
    default:                                        /* V_BITS */
        v->amp = (int32_t)((7000u * lvl) >> 8); v->dec_k = tau_k(160);
        v->inc = note_inc((int32_t)pitch + 12); v->s1 = 0x7FFF;
        break;
    }
    v->peak = v->peak ? v->peak : v->amp;
}

/* a voice start (now): melodic notes or a drum hit, with the lane's slide memory */
static void fire(uint32_t lane, const uint8_t *note, uint32_t n, uint32_t flags, uint32_t dur, uint32_t lvl,
                 uint32_t kind_or_inst, int32_t original)
{
    uint32_t i;
    lane_hit_ms[lane] = fm1_ms;
    if (lane == L_DRUMS) { start_drum(note[0], lvl, kind_or_inst, original); return; }
    if (lane == L_SFX) { start_sfx(note[0], note[1], dur, lvl); return; }
    {
        const patch_t *P = &PATCH[kind_or_inst < NPATCH ? kind_or_inst : 0];
        int32_t chord = n > 1u && !(flags & EV_ARP);
        uint32_t l2 = chord ? (lvl * 115u) >> 8 : lvl;                    /* chords: quieter per voice */
        if (flags & EV_ARP) {
            uint32_t a[3];
            for (i = 0; i < n && i < 3u; i++) a[i] = note_inc(note[i]);
            start_fm(lane, P, a[0], dur, l2, 0, a, n, original ? note[0] : 0);
            return;
        }
        for (i = 0; i < n && i < 3u; i++) {
            uint32_t inc = note_inc(note[i]), from = original ? lane_last_inc[lane][i] : 0;
            start_fm(lane, P, inc, dur, l2, fx[lane].slide > 0 ? from : 0, 0, 0, original ? note[i] : 0);
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
    p->kind = (uint8_t)kind; p->note[0] = e->note[0]; p->note[1] = e->note[1]; p->note[2] = e->note[2];
}

/* an event at its step: the note now, its stutter repeats, its echo copies */
static void trigger(uint32_t lane, const ev_t *e, uint32_t kind)
{
    uint32_t step = seq_step_len_q8 >> 8, total = lane == L_DRUMS ? step : e->len * step;
    int32_t rt = fx[lane].retrig > 0 && (int32_t)(e->rv * 100u >> 8) < fx[lane].retrig;
    uint32_t seg = rt ? step / (uint32_t)(fx[lane].rtg_rate < 1 ? 1 : fx[lane].rtg_rate) : total;
    uint32_t reps = rt ? total / (seg ? seg : 1u) : 1u, dur = rt ? seg * 7u / 10u : total * 95u / 100u, k, c;
    if (reps > 16u) reps = 16u;
    if (!reps) reps = 1;
    uint32_t base = e->flags & EV_ACC2 ? 112u : e->flags & EV_ACC1 ? 176u : 256u;   /* the accent */
    for (c = 0; c < 3u; c++) {                                       /* the note, then two echo copies */
        uint32_t lvl = c == 0 ? base : c == 1 ? (uint32_t)fx[lane].echo * 141u / 100u * base >> 8
                                              : (uint32_t)(fx[lane].echo * fx[lane].echo) * 77u / 10000u * base >> 8;
        uint32_t at = smp + c * 3u * step;
        if (c && fx[lane].echo <= 0) break;
        if (!lvl) continue;
        for (k = 0; k < reps; k++) {
            if (c == 0 && k == 0) fire(lane, e->note, e->n, e->flags, dur, lvl, kind, 1);
            else pend_add(at + k * seg, lane, e, dur, lvl, kind);
        }
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
                if (v->chaos == 1u) {
                    int32_t to = v->stage == 2u ? 0 : v->peak;
                    v->pamp += (int32_t)(((int64_t)(to - v->pamp) * (to > v->pamp ? 68 : 34)) >> 15);   /* per 32 samples: tau 350 ms in, 700 ms out */
                }
                if (v->arp_n > 1u && (v->arp_t += CTRL) >= 1852u) { v->arp_t = 0; v->arp_i = (uint8_t)((v->arp_i + 1u) % v->arp_n); v->inc = v->target = v->arp[v->arp_i]; }
            } else if (v->kind >= V_WASH) {                               /* sfx */
                switch (v->kind) {
                case V_WASH: case V_CRACKLE: case V_RISER:
                    if (v->stage == 0u) { v->amp += v->att; if (v->amp >= v->peak) { v->amp = v->peak; v->stage = 1; } }
                    else if (v->stage == 2u) v->amp = (v->amp * v->rel_k) >> 15;
                    if (v->kind == V_WASH) v->tph += 3000u * CTRL;                         /* the sweep: 0.03 Hz.. */
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
            } else {
                v->amp = (v->amp * v->dec_k) >> 15;
                if (v->kind == V_KICK || v->kind == V_KETTLE)
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
                if (v->midi) { mo_push((v->kind == V_FM ? 0x80u | v->midi_ch : 0x89u), v->midi, 0); v->midi = 0; }
                v->kind = V_OFF; return;
            }
        }
        switch (v->kind) {
        case V_FM: {
            uint32_t inc = v->inc, off;
            if (v->vib && v->ctr > 15435u) {                            /* vibrato after 0.35 s: 5.6 Hz, 0.7 % */
                v->vph += 545400u;
                inc += (uint32_t)(((int32_t)(inc >> 16) * SIN[v->vph >> 22]) >> 6);
            }
            v->mph += (inc >> 4) * v->ratio16;
            off = (uint32_t)(SIN[v->mph >> 22] * v->idx) << 6;
            v->ph += inc;
            s = SIN[(v->ph + off) >> 22];
            if (v->square) s = s >= 0 ? 26000 : -26000;
            s = (s * v->amp) >> 15;
            if (v->chaos == 2u) {                                       /* an ensemble: two more players, a little out */
                int32_t e;
                v->pph1 += inc + (inc >> 8);
                v->pph2 += inc - (inc >> 8) - (inc >> 10);
                e = SIN[(v->pph1 + ((uint32_t)(int32_t)SIN[v->pph1 >> 22] << 14)) >> 22]
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
            s = (SIN[v->ph >> 22] * v->amp) >> 15;
            break;
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
                s = ((v->s2 * v->amp) >> 15) + ((tri * v->tone_amp) >> 15);
            }
            break; }
        case V_HAT: case V_CRASH: {
            int32_t x = noise(), hp = x - v->s1;            /* one-pole high-pass */
            v->s1 += ((x - v->s1) * (v->kind == V_HAT ? 6000 : 12000)) >> 15;
            s = (hp * v->amp) >> 15;
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
        else if (x->kind == V_HAT || x->kind == V_CRASH) b = B_HATS;
        render_voice(&vc[v], &bus[b][from], n);
    }
}

static void all_off(void)
{
    uint32_t v;
    for (v = 0; v < NVOICE; v++) {
        if (vc[v].kind != V_OFF && vc[v].midi) mo_push(vc[v].kind == V_FM ? 0x80u | vc[v].midi_ch : 0x89u, vc[v].midi, 0);
        if (vc[v].kind == V_FM) { vc[v].stage = 2; vc[v].gate = 0; vc[v].midi = 0; }
    }
    npend = 0;
}

/* the keys, played live: the main loop queues key down / up, the interrupt starts and releases (only it touches
 * the voices). A melodic note sounds until its key comes up; drums and sfx are hits. */
typedef struct { uint8_t lane, note, key, on, inst; } live_t;
static volatile live_t live_q[16];
static volatile uint32_t live_w, live_r;
static voice_t *live_v[27];
static uint32_t live_age[27];
static void live_push(uint32_t lane, uint32_t note, uint32_t key, uint32_t on, uint32_t inst)
{
    live_t *e;
    if (live_w - live_r >= 16u) return;
    e = (live_t *)&live_q[live_w % 16u];
    e->lane = (uint8_t)lane; e->note = (uint8_t)note; e->key = (uint8_t)key; e->on = (uint8_t)on; e->inst = (uint8_t)inst;
    RING_PUBLISH();
    live_w++;
}
static void live_run(void)
{
    while (live_r != live_w) {
        const live_t *e = (const live_t *)&live_q[live_r % 16u];
        uint32_t k = e->key < 27u ? e->key : 0u;
        if (e->on) {
            if (e->lane == L_DRUMS) start_drum(e->note, 230, e->inst, 0);
            else if (e->lane == L_SFX) start_sfx(e->note, (uint32_t)(62 + key_off), 44100, 230);
            else {
                start_fm(e->lane, &PATCH[e->inst < NPATCH ? e->inst : 0], note_inc(e->note), 0xFFFFFFF0u, 230, 0, 0, 0, e->note);
                live_v[k] = last_voice; live_age[k] = last_voice->age;
            }
            lane_hit_ms[e->lane] = fm1_ms;
        } else if (live_v[k] && live_v[k]->age == live_age[k] && live_v[k]->kind == V_FM && live_v[k]->stage != 2u) {
            live_v[k]->gate = 1;                                    /* released on the next control tick */
            live_v[k] = 0;
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

static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t t0 = fm1_ticks(), i = 0, l, k;
    for (l = 0; l < NBUS; l++)
        for (k = 0; k < n; k++) bus[l][k] = 0;
    if (aud_req) { uint8_t nt[3] = {(uint8_t)aud_note, (uint8_t)(62 + key_off), 0}; fire(aud_lane, nt, 1, 0, aud_lane == L_SFX ? 33075u : 9000u, 256, aud_kind, 0); aud_req = 0; }
    live_run();
    if (seq_stop_req) { seq_stop_req = 0; seq_playing = 0; all_off(); }
    if (seq_start_req) {
        seq_start_req = 0; seq_playing = 1; seq_step = (uint32_t)seq_start_step; seq_acc_q8 = 0; npend = 0;
        step_fire(seq_step);
    }
    while (i < n) {
        uint32_t span = n - i, p;
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
                fire(pend[p].lane, pend[p].note, pend[p].n, pend[p].flags, pend[p].dur, pend[p].lvl, pend[p].kind, 0);
                pend[p] = pend[--npend];
            } else p++;
        }
        if (seq_playing && seq_acc_q8 >= seq_step_len_q8) {
            seq_acc_q8 -= seq_step_len_q8;
            seq_step++;
            step_fire(seq_step);
        }
    }
    for (k = 0; k < n; k++) sendb[k] = 0;
    for (l = 0; l < NBUS; l++) {
        int32_t lv = fx[BUS_LANE[l]].level * 480 / 100, pl = PAN_L[l], pr = PAN_R[l], sd = SEND[l];   /* Q8: 80 -> 1.5 */
        lane_fx(l, bus[l], n);
        for (k = 0; k < n; k++) {
            int32_t s = (bus[l][k] * lv) >> 8;
            out[2u * k] += (s * pl) >> 8;
            out[2u * k + 1u] += (s * pr) >> 8;
            sendb[k] += (s * sd) >> 8;
        }
    }
    room(out, n);
    for (k = 0; k < 2u * n; k++) {                                           /* a soft knee above -3 dB */
        int32_t s = out[k];
        if (s > 23000) s = 23000 + ((s - 23000) >> 2); else if (s < -23000) s = -23000 + ((s + 23000) >> 2);
        out[k] = s;
    }
    vis_step = seq_step;
    vis_frac = seq_step_len_q8 ? (seq_acc_q8 >> 8) * 65536u / (seq_step_len_q8 >> 8 ? seq_step_len_q8 >> 8 : 1u) : 0u;
    isr_us_last = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
    if (isr_us_last > isr_us_max) isr_us_max = isr_us_last;
    isr_us_sum += isr_us_last; isr_n++;
}
