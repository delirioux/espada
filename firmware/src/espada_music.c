/* SPDX-License-Identifier: GPL-3.0-only */
/* Espada's music, in the main loop: what each lane plays, bar by bar.
 *
 *   harmony     a chord per bar: from the profile (explicit chords, borrowed ones allowed), or from the SONG
 *               page (key, scale, a suggested progression). Lines move through a "local scale": the scale
 *               with any note a semitone off a chord tone swapped for that chord tone, so nothing rubs.
 *   styles      ten profiles: a progression, a generator and an instrument per lane, a tempo, and a way of
 *               playing (how many lead notes glide in, how many long chords swell or fade, the lead's echo)
 *   mixing      a queued profile; BLEND moves lanes to it bottom-up (drums, bass, pad, lead); LAUNCH moves
 *               every unlocked lane on the next phrase; the bass lane's profile carries the harmony
 *   density     50 = the part as designed; lower thins, higher adds notes inside the chord and its scale
 *   the arc     energy over time: BUILD from the drums up, a BREAK with one lane alone, the drop (PEAK),
 *               DOWN, and a new build
 *   sfx         follows the arc: a riser into the drop, the impact on it, washes, zaps, crackle, bits
 *   patterns    your own parts (scale steps from the chord root, so they follow the chords), GENERATE
 *   the cache   bars are generated ahead into double-buffered slots the audio interrupt reads */

/* ---- randomness: mulberry32, deterministic per bar, lane and profile */
typedef struct { uint32_t a; } rng_t;
static uint32_t rnd(rng_t *r)
{
    uint32_t t;
    r->a += 0x6D2B79F5u;
    t = r->a;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return t ^ (t >> 14);
}
static int chance(rng_t *r, int32_t pct) { return pct >= 100 || (pct > 0 && (int32_t)((rnd(r) >> 16) * 100u >> 16) < pct); }
static int32_t pickn(rng_t *r, const int8_t *a, uint32_t n) { return a[((rnd(r) >> 16) * n) >> 16]; }
#define PICK(r, ...) pickn(r, (const int8_t[]){__VA_ARGS__}, sizeof((const int8_t[]){__VA_ARGS__}))
static uint32_t hash3(int32_t b, int32_t l, int32_t p) { return ((uint32_t)b * 2654435761u) ^ ((uint32_t)l * 40503u) ^ ((uint32_t)p * 97u) ^ 0x5eedu; }
static int32_t fdiv(int32_t a, int32_t b) { int32_t q = a / b; return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q; }
static int32_t fmodp(int32_t a, int32_t b) { int32_t m = a % b; return m < 0 ? m + b : m; }

/* ---- harmony */
enum { Q_MAJ, Q_MIN, Q_5, Q_S4, Q_DIM, Q_AUG };
static const int8_t QUAL[6][3] = {{0, 4, 7}, {0, 3, 7}, {0, 7, 12}, {0, 5, 7}, {0, 3, 6}, {0, 4, 8}};
static const int8_t QUAL_N[6] = {3, 3, 2, 3, 3, 3};
typedef struct { int32_t root, nt, tones[3], nls, ls[12]; } harm_t;
static void harm_make(harm_t *H, const int8_t *scale, int32_t nsc, int32_t root, const int8_t *tones, int32_t nt)
{
    int32_t i, j, pcs[3], have[12] = {0};
    H->root = root; H->nt = nt; H->nls = 0;
    for (i = 0; i < nt; i++) { H->tones[i] = tones[i]; pcs[i] = (root + tones[i]) % 12; }
    for (i = 0; i < nsc; i++) {
        int32_t pc = scale[i], clash = 0;
        for (j = 0; j < nt; j++) { int32_t d = fmodp(pc - pcs[j], 12); if (d == 1 || d == 11) clash = 1; }
        if (!clash) have[pc] = 1;
    }
    for (j = 0; j < nt; j++) have[pcs[j]] = 1;
    for (i = 0; i < 12; i++) if (have[i]) H->ls[H->nls++] = i;
}
/* the tools a generator uses: chord tones, scale steps, moving and snapping, around a base note (the tonic) */
typedef struct { const harm_t *H; int32_t base, ri; } tools_t;
static void tools_make(tools_t *T, const harm_t *H, int32_t base)
{
    int32_t i;
    T->H = H; T->base = base; T->ri = 0;
    for (i = 0; i < H->nls; i++) if (H->ls[i] == H->root % 12) T->ri = i;
}
static int32_t ct(const tools_t *T, int32_t i)
{
    int32_t nt = T->H->nt;
    return T->base + T->H->root + T->H->tones[fmodp(i, nt)] + 12 * fdiv(i, nt);
}
static int32_t stp(const tools_t *T, int32_t k)
{
    int32_t n = T->H->nls, j = T->ri + k;
    return T->base + T->H->ls[fmodp(j, n)] + 12 * fdiv(j, n);
}
static int32_t move(const tools_t *T, int32_t note, int32_t k)
{
    int32_t n = T->H->nls, rel = note - T->base, o = fdiv(rel, 12), pc = fmodp(rel, 12), i, j;
    for (i = 0; i < n && T->H->ls[i] < pc; i++) ;
    if (i == n) { i = 0; o++; }
    j = i + k;
    return T->base + T->H->ls[fmodp(j, n)] + 12 * (o + fdiv(j, n));
}
static int32_t snap(const tools_t *T, int32_t note)
{
    int32_t i, best = ct(T, 0);
    for (i = -4; i <= 10; i++) { int32_t c = ct(T, i), d = c - note, e = best - note; if ((d < 0 ? -d : d) < (e < 0 ? -e : e)) best = c; }
    return best;
}

/* ---- profiles */
enum { S_DRIVE, S_PEDAL, S_OCTAVE,                       /* bass */
       S_EPIC, S_STAB, S_RIFF, S_MELODY,                 /* lead */
       S_STRINGS, S_SUS, S_LUSH, S_ARP, S_CHOP,          /* pad */
       S_DEPIC, S_DTECH, S_DFUSION, S_DOLD,              /* drums */
       S_SYMLEAD, S_ORCH, S_CELLO, S_DORCH,              /* the symphony: lead, strings, cellos, percussion */
       S_ROOTS, S_GALLOP, S_SLAP, S_ACID,                /* bass: TROPIC, STEEL, FUNK, RAVE */
       S_SKANK, S_PCHORD, S_CLAV,                         /* pad: TROPIC, STEEL, FUNK */
       S_DTROPIC, S_DSTEEL, S_DDRIFT, S_DFUNK, S_DRAVE }; /* drums */
static const int8_t AEO[7] = {0, 2, 3, 5, 7, 8, 10}, ION[7] = {0, 2, 4, 5, 7, 9, 11}, DOR[7] = {0, 2, 3, 5, 7, 9, 10},
                    MXL[7] = {0, 2, 4, 5, 7, 9, 10};
typedef struct {
    const char *name;
    const int8_t *scale;
    int8_t prog[8][2];          /* root above the tonic, quality */
    uint8_t st[NLANE];          /* style per lane (LEAD, PAD, BASS, DRUMS; the sfx lane has its own generator) */
    uint8_t inst[NLANE];        /* suggested patch per lane, kit for the drums (sfx: unused) */
    uint8_t hr;                 /* bars a chord (0 = 1) */
    uint8_t bpm;                /* its tempo (LAUNCH sets it) */
    uint8_t glide, swell, echo; /* how it plays: % of lead notes that glide in, % of long chords that swell or
                                 * fade, the echo it adds to the lead */
} prof_t;
/* ten styles of 90s tracker music, from a study of their modules: chords (sus chords everywhere), drum grids,
 * tempos, and how the notes are played (glides, swells, an echo channel) */
static const prof_t PROF[] = {
    {"EPIC", AEO, {{0, Q_MIN}, {8, Q_MAJ}, {10, Q_MAJ}, {0, Q_MIN}, {0, Q_MIN}, {8, Q_MAJ}, {5, Q_MAJ}, {0, Q_MAJ}},
     {S_EPIC, S_STRINGS, S_DRIVE, S_DEPIC}, {10, 5, 2, 1}, 0, 125, 15, 35, 25},
    {"HYPERSPACE", AEO, {{0, Q_5}, {0, Q_5}, {10, Q_5}, {5, Q_5}, {7, Q_5}, {5, Q_5}, {3, Q_5}, {10, Q_5}},
     {S_STAB, S_CHOP, S_DRIVE, S_DTECH}, {11, 7, 0, 2}, 0, 130, 20, 0, 30},
    {"FUSION", ION, {{0, Q_MAJ}, {9, Q_MIN}, {0, Q_MAJ}, {9, Q_MIN}, {9, Q_MAJ}, {1, Q_MAJ}, {9, Q_MAJ}, {5, Q_MAJ}},
     {S_RIFF, S_LUSH, S_PEDAL, S_DFUSION}, {9, 13, 1, 0}, 0, 125, 25, 25, 20},
    {"OLDSKOOL", AEO, {{5, Q_MIN}, {3, Q_MAJ}, {1, Q_MAJ}, {5, Q_MIN}, {0, Q_MIN}, {8, Q_MAJ}, {0, Q_MIN}, {3, Q_MAJ}},
     {S_MELODY, S_ARP, S_OCTAVE, S_DOLD}, {8, 9, 0, 1}, 0, 125, 10, 0, 35},
    /* the orchestra: minor, two bars a chord, a dominant before the return */
    {"SYMPHONY", AEO, {{0, Q_MIN}, {8, Q_MAJ}, {10, Q_MAJ}, {0, Q_MIN}, {5, Q_MIN}, {8, Q_MAJ}, {7, Q_MAJ}, {0, Q_MIN}},
     {S_SYMLEAD, S_ORCH, S_CELLO, S_DORCH}, {15, 5, 16, 0}, 2, 110, 5, 55, 0},
    /* sunny: mixolydian, I and bVII, a sus4 into the turn; off-beat chords, a deep bass with rests, congas */
    {"TROPIC", MXL, {{0, Q_MAJ}, {0, Q_S4}, {10, Q_MAJ}, {5, Q_MAJ}, {0, Q_MAJ}, {10, Q_MAJ}, {5, Q_MAJ}, {7, Q_S4}},
     {S_MELODY, S_SKANK, S_ROOTS, S_DTROPIC}, {8, 13, 3, 1}, 0, 110, 15, 0, 25},
    /* heavy: power chords in minor, a galloping bass, double kick, a gliding saw lead */
    {"STEEL", AEO, {{0, Q_5}, {0, Q_5}, {8, Q_5}, {10, Q_5}, {0, Q_5}, {3, Q_5}, {5, Q_5}, {7, Q_5}},
     {S_MELODY, S_PCHORD, S_GALLOP, S_DSTEEL}, {10, 10, 0, 0}, 0, 140, 40, 0, 0},
    /* floating: sus chords resolving, two bars a chord, swelling strings, a breathy lead in long lines, no snare */
    {"DRIFT", AEO, {{0, Q_S4}, {0, Q_MIN}, {8, Q_MAJ}, {3, Q_MAJ}, {10, Q_S4}, {10, Q_MAJ}, {7, Q_S4}, {7, Q_MIN}},
     {S_EPIC, S_LUSH, S_PEDAL, S_DDRIFT}, {8, 5, 3, 2}, 2, 96, 30, 70, 50},
    /* funky: a major vamp with a diminished passing chord, a slapped syncopated bass, clav stabs, kick off the beat */
    {"FUNK", ION, {{0, Q_MAJ}, {0, Q_MAJ}, {5, Q_MAJ}, {0, Q_MAJ}, {6, Q_DIM}, {5, Q_MAJ}, {7, Q_S4}, {7, Q_MAJ}},
     {S_RIFF, S_CLAV, S_SLAP, S_DFUNK}, {11, 13, 1, 1}, 0, 112, 10, 0, 0},
    /* the rave: four on the floor, an acid bass that slides, organ stabs, brass chord hits */
    {"RAVE", AEO, {{0, Q_MIN}, {0, Q_MIN}, {8, Q_MAJ}, {8, Q_MAJ}, {3, Q_MAJ}, {3, Q_MAJ}, {10, Q_MAJ}, {7, Q_S4}},
     {S_STAB, S_SUS, S_ACID, S_DRAVE}, {4, 7, 0, 2}, 0, 138, 20, 0, 35},
};
#define NPROF (int32_t)(sizeof PROF / sizeof PROF[0])

/* ---- the SONG page: key, scale, suggested progressions (scale degrees, chords stacked in the scale) */
typedef struct { const char *name; int8_t pcs[7]; int8_t n; const int8_t *chord; } scale_t;
static const int8_t PHR[7] = {0, 1, 3, 5, 7, 8, 10}, LYD[7] = {0, 2, 4, 6, 7, 9, 11}, MIX[7] = {0, 2, 4, 5, 7, 9, 10},
                    LOC[7] = {0, 1, 3, 5, 6, 8, 10}, HMI[7] = {0, 2, 3, 5, 7, 8, 11}, MMI[7] = {0, 2, 3, 5, 7, 9, 11};
static const scale_t SCALES[] = {
    {"AUTO", {0}, 0, 0},
    {"MAJOR", {0, 2, 4, 5, 7, 9, 11}, 7, ION}, {"MINOR", {0, 2, 3, 5, 7, 8, 10}, 7, AEO},
    {"DORIAN", {0, 2, 3, 5, 7, 9, 10}, 7, DOR}, {"PHRYGIAN", {0, 1, 3, 5, 7, 8, 10}, 7, PHR},
    {"LYDIAN", {0, 2, 4, 6, 7, 9, 11}, 7, LYD}, {"MIXOLYDIAN", {0, 2, 4, 5, 7, 9, 10}, 7, MIX},
    {"LOCRIAN", {0, 1, 3, 5, 6, 8, 10}, 7, LOC}, {"HARM MINOR", {0, 2, 3, 5, 7, 8, 11}, 7, HMI},
    {"MEL MINOR", {0, 2, 3, 5, 7, 9, 11}, 7, MMI}, {"MAJ PENTA", {0, 2, 4, 7, 9}, 5, ION},
    {"MIN PENTA", {0, 3, 5, 7, 10}, 5, AEO}, {"BLUES", {0, 3, 5, 6, 7, 10}, 6, DOR},
};
#define NSCALE (int32_t)(sizeof SCALES / sizeof SCALES[0])
static const int8_t PROGS_MAJ[9][4] = {{0, 4, 5, 3}, {0, 5, 3, 4}, {5, 3, 0, 4}, {0, 3, 5, 4}, {1, 4, 0, 0}, {0, 3, 0, 4}, {0, 2, 3, 4}, {0, 3, 1, 4}, {0, 6, 3, 0}};
static const int8_t PROGS_MIN[9][4] = {{0, 5, 2, 6}, {0, 6, 5, 6}, {0, 3, 4, 0}, {0, 5, 6, 0}, {0, 3, 6, 2}, {0, 2, 6, 3}, {3, 2, 1, 0}, {0, 4, 5, 2}, {0, 6, 3, 0}};
#define NPROG 9
static int32_t key = 2, scale_sel = 0, prog_sel = 0, bpc = 1;   /* D, AUTO, from the profile, 1 bar a chord */
static int scale_is_major(int32_t s) { int32_t i; if (!s) return 0; for (i = 0; i < 7; i++) if (SCALES[s].chord[i] == 4) return 1; return 0; }
static const int8_t *prog_of(int32_t sel) { return scale_is_major(scale_sel) ? PROGS_MAJ[sel] : PROGS_MIN[sel]; }
static void chord_of(int32_t s, int32_t deg, int32_t *root, int8_t *tones)
{
    const int8_t *c = SCALES[s].chord;
    int32_t r = c[deg % 7] + 12 * (deg / 7), a = c[(deg + 2) % 7] + 12 * ((deg + 2) / 7), b = c[(deg + 4) % 7] + 12 * ((deg + 4) / 7);
    *root = r % 12; tones[0] = 0; tones[1] = (int8_t)(a - r); tones[2] = (int8_t)(b - r);
}

/* ---- lanes and mixing */
#define EMPTY (-128)
typedef struct { int8_t k[32]; uint8_t len[32]; uint8_t kinds[32]; uint8_t on, bars; } upat_t;
typedef struct {
    uint8_t prof, lock, dens; int8_t oct; upat_t up;
    uint8_t mel, rhy, pit, chg;     /* MELODY 0..99 (which idea), RHYTHM, PITCH, CHANGE 0..100 */
    uint8_t mute;                   /* ENV: faded out */
} lane_t;
static lane_t ln[NLANE];
static const int32_t LBASE[NLANE] = {62, 50, 38, 0, 62}, LLO[NLANE] = {58, 45, 24, 0, 0}, LHI[NLANE] = {98, 86, 64, 0, 0};
static const int32_t LORDER[NLANE] = {3, 2, 1, 0, 4};           /* blend order: drums first; sfx follows the bass */
static int32_t target = 2, blend = 0, launch_bar = -1;           /* FUSION queued, as it plays */

static int32_t eff_prof(int32_t l, int32_t b)
{
    if (l == L_SFX) l = L_BASS;                                     /* its flavour: the profile with the harmony */
    if (ln[l].lock) return ln[l].prof;
    if (launch_bar >= 0 && b >= launch_bar) return target;
    if (LORDER[l] < blend) return target;
    return ln[l].prof;
}
/* the profile's chord at bar b: its harmonic rhythm (bars a chord), and every other time round a B section (the
 * progression's pairs swapped: 5 6 1 2 7 8 3 4), so 8 chords make a 16-chord form */
static int32_t prof_chord(const prof_t *P, int32_t b)
{
    static const int8_t BSEC[8] = {4, 5, 0, 1, 6, 7, 2, 3};
    int32_t hr = P->hr ? P->hr : 1, k = fdiv(b, hr), i = fmodp(k, 8);
    return fmodp(fdiv(k, 8), 2) ? BSEC[i] : i;
}
static void harm_at(int32_t b, harm_t *H)
{
    if (prog_sel > 0 && scale_sel > 0) {
        int32_t root; int8_t tones[3];
        const int8_t *P = prog_of(prog_sel - 1);
        chord_of(scale_sel, P[(b / bpc) % 4], &root, tones);
        harm_make(H, SCALES[scale_sel].pcs, SCALES[scale_sel].n, root, tones, 3);
    } else {
        const prof_t *P = &PROF[eff_prof(L_BASS, b)];
        const int8_t *c = P->prog[prof_chord(P, b)];
        harm_make(H, P->scale, 7, c[0], QUAL[(int)c[1]], QUAL_N[(int)c[1]]);
    }
}

/* ---- the arc: energy (Q8, 256 = full) over bars */
enum { A_OFF, A_BUILD, A_BREAK, A_PEAK, A_DOWN };
typedef struct { int32_t mode, from, e0, until, lane; } seg_t;
typedef struct { int32_t mode, e, lane, until, drop; } arc_t;
static int32_t arc_on = 0, build_ph = 6, peak_ph = 4, auto_drop = 1;   /* the arc is off until you turn it on */
static int32_t energy;                                          /* -50..50: added to every lane's density (MAIN) */
static int32_t brk_from = -1, brk_until = -1, brk_lane;        /* DROP without the arc: a break, then the drop */
static seg_t segs[16] = {{A_BUILD, 0, 0, -1, L_PAD}};
static int32_t nsegs = 1;
static const int32_t ENTER[NLANE] = {159, 108, 56, 0, 0};       /* lead, pad, bass, drums (sfx: always on) */
static int next_implicit(const seg_t *s, seg_t *o)
{
    o->until = -1; o->lane = L_PAD; o->e0 = 230;
    if (s->mode == A_PEAK) { o->mode = A_DOWN; o->from = s->from + peak_ph * 4; return 1; }
    if (s->mode == A_DOWN) { o->mode = A_BUILD; o->from = s->from + 4; o->e0 = 31; return 1; }
    if (s->mode == A_BREAK) { if (s->until < 0) return 0; o->mode = A_PEAK; o->from = s->until; return 1; }
    if (s->mode == A_BUILD && auto_drop) {
        int32_t need = (230 - s->e0) * build_ph * 4, n = (need + 255) / 256, reach = s->from + (n < 1 ? 1 : n), P = (reach + 3) / 4 * 4;
        o->mode = A_BREAK; o->from = P + 3; o->until = P + 4; o->lane = L_PAD; return 1;
    }
    return 0;
}
static arc_t arc_at(int32_t b)
{
    arc_t a = {A_OFF, 154, 0, -1, 0};
    seg_t s, nx;
    int32_t i, k;
    if (!arc_on) {                                              /* only the breaks DROP makes */
        if (brk_from >= 0 && b >= brk_from && (brk_until < 0 || b < brk_until)) { a.mode = A_BREAK; a.lane = brk_lane; a.until = brk_until; }
        else if (brk_from >= 0 && b == brk_until) a.drop = 1;
        return a;
    }
    s = segs[0];
    for (i = 0; i < nsegs; i++) if (segs[i].from <= b) s = segs[i];
    if (s.from > b) { a.mode = A_BUILD; a.e = 0; return a; }
    for (k = 0; k < 64 && next_implicit(&s, &nx) && nx.from <= b; k++) s = nx;
    a.mode = s.mode; a.lane = s.lane; a.until = s.until;
    if (s.mode == A_BUILD) { a.e = s.e0 + (b - s.from) * 256 / (build_ph * 4); if (a.e > 230) a.e = 230; }
    else if (s.mode == A_BREAK) a.e = s.e0;
    else if (s.mode == A_PEAK) { a.e = 256; a.drop = b == s.from; }
    else { a.e = 256 - (b - s.from + 1) * 56; if (a.e < 31) a.e = 31; }
    return a;
}
static int lane_active(const arc_t *A, int32_t l)
{
    if (A->mode == A_OFF || l == L_SFX) return 1;                  /* sfx: the riser plays in the break */
    if (A->mode == A_BREAK) return l == A->lane;
    return A->e >= ENTER[l];
}

/* ---- the form: INTRO (PLAY: one of pad, drums or bass alone, the other lanes in one by one, two bars apart),
 * INTER (a tap on REC: the marked lane alone for two bars, then the others back one by one, a bar apart) and END
 * (PLAY again: the lanes fade out one by one, a bar apart, down to bass or drums alone, which fades last; then it
 * stops). Ranks are drawn at random each time. */
static int32_t intro_from = -1, intro_rank[NLANE];
static int32_t inter_from = -1, inter_rank[NLANE];             /* rank 0: the lane alone */
static int32_t end_from = -1, end_bar[NLANE];                    /* the bar each lane fades out in */
#define INTRO_BARS 2
#define END_LEN 8                                               /* bars from END to the stop */
static rng_t form_rng = {0x7b3d91u};
static void shuffle(int32_t *a, int32_t n)
{
    int32_t i;
    for (i = n - 1; i > 0; i--) { int32_t j = (int32_t)((rnd(&form_rng) >> 16) * (uint32_t)(i + 1) >> 16), t = a[i]; a[i] = a[j]; a[j] = t; }
}
static void form_reset(void) { intro_from = inter_from = end_from = -1; }
static void form_intro(int32_t from)
{
    int32_t base[3] = {L_PAD, L_DRUMS, L_BASS}, i;
    form_rng.a ^= fm1_ms * 2654435761u;
    shuffle(base, 3);
    for (i = 0; i < 3; i++) intro_rank[base[i]] = i;
    intro_rank[L_LEAD] = 3; intro_rank[L_SFX] = 3;               /* the lead (and the sfx) last */
    intro_from = from;
}
static void form_inter(int32_t lane, int32_t from)
{
    int32_t rest[NLANE], n = 0, l;
    form_rng.a ^= fm1_ms * 40503u;
    for (l = 0; l < (int32_t)NLANE; l++) if (l != lane) rest[n++] = l;
    shuffle(rest, n);
    inter_rank[lane] = 0;
    for (l = 0; l < n; l++) inter_rank[rest[l]] = l + 1;
    inter_from = from;
}
static void form_end(int32_t from)
{
    int32_t first[3] = {L_LEAD, L_SFX, L_PAD}, last = chance(&form_rng, 50) ? L_BASS : L_DRUMS, i;
    form_rng.a ^= fm1_ms * 97u;
    shuffle(first, 3);
    for (i = 0; i < 3; i++) end_bar[first[i]] = from + i;
    end_bar[last == L_BASS ? L_DRUMS : L_BASS] = from + 3;
    end_bar[last] = from + 6;                                   /* alone for two bars, then it fades too */
    end_from = from;
}
/* does lane l play in bar b (INTRO and INTER; END fades, see fx_apply) */
static int form_plays(int32_t l, int32_t b)
{
    if (intro_from >= 0 && b >= intro_from && b < intro_from + INTRO_BARS * intro_rank[l]) return 0;
    if (inter_from >= 0 && b >= inter_from && inter_rank[l] && b < inter_from + 1 + inter_rank[l]) return 0;
    return 1;
}
static int form_gone(int32_t l, int32_t b) { return end_from >= 0 && b >= end_bar[l]; }
static const char *form_name(int32_t b)
{
    int32_t l, on = 0;
    if (end_from >= 0) return "END";
    for (l = 0; l < (int32_t)NLANE; l++) on += form_plays(l, b);
    if (on == (int32_t)NLANE) return 0;
    return inter_from >= 0 && b >= inter_from ? "INTER" : "INTRO";
}

/* ---- generating: events into a small buffer */
typedef struct { ev_t ev[MAXEV]; uint32_t n; } gen_t;
static ev_t *g_add(gen_t *G, int32_t st, int32_t len, int32_t n, int32_t a, int32_t b, int32_t c, uint32_t flags)
{
    ev_t *e;
    if (G->n >= MAXEV || st < 0 || st > 15) return 0;
    e = &G->ev[G->n++];
    e->st = (uint8_t)st; e->len = (uint8_t)(len < 1 ? 1 : len > 16 ? 16 : len); e->n = (uint8_t)n; e->flags = (uint8_t)flags; e->rv = 0;
    e->note[0] = (uint8_t)(a < 0 ? 0 : a > 127 ? 127 : a); e->note[1] = (uint8_t)(b < 0 ? 0 : b > 127 ? 127 : b); e->note[2] = (uint8_t)(c < 0 ? 0 : c > 127 ? 127 : c);
    return e;
}
#define NOTE(G, st, len, n) g_add(G, st, len, 1, n, 0, 0, 0)
#define DRUM(G, st, k) g_add(G, st, 1, 1, k, 0, 0, 0)
#define SFX(G, st, len, k, pitch) g_add(G, st, len, 1, k, pitch, 0, 0)
static void g_sort(gen_t *G)
{
    uint32_t i, j;
    for (i = 1; i < G->n; i++) { ev_t e = G->ev[i]; for (j = i; j > 0 && G->ev[j - 1].st > e.st; j--) G->ev[j] = G->ev[j - 1]; G->ev[j] = e; }
}
static void g_lens(gen_t *G, int32_t cap)                     /* each note lasts until the next one */
{
    uint32_t i, j;
    g_sort(G);
    for (i = 0; i < G->n; i++) {
        int32_t nx = 16;
        for (j = i + 1; j < G->n; j++) if (G->ev[j].st > G->ev[i].st) { nx = G->ev[j].st; break; }
        G->ev[i].len = (uint8_t)((nx - G->ev[i].st) > cap ? cap : nx - G->ev[i].st);
        if (!G->ev[i].len) G->ev[i].len = 1;
    }
}
static void hits(gen_t *G, rng_t *r, int32_t k, const int8_t *pct)    /* a drum on each step with its chance */
{
    int32_t s;
    for (s = 0; s < 16; s++) if (pct[s] && chance(r, pct[s])) DRUM(G, s, k);
}

/* ---- the generators' context: the lane's knobs and its bar keys (CHANGE folds the bar number: low, the material
 * comes back soon; high, it keeps moving) */
static const lane_t *GL;
static int32_t g_style;                         /* the style generating now (densify asks) */
static int32_t g_bk, g_ik;                      /* the bar key (all lanes), the idea key (the lead's motif) */
static void keys_for(const lane_t *L, int32_t b)
{
    int32_t c = L->chg;
    g_bk = c < 10 ? b % 8 : c < 40 ? b % 16 : c < 70 ? b % 32 : b;
    g_ik = c < 10 ? 0 : c < 40 ? b / 64 : c < 70 ? b / 32 : b / 16;
}

/* a lead in long arcs: a 16-bar section, two 8-bar periods. In a period the motif (one bar: a rhythm and a contour
 * of chord tones) is stated (bar 1), answered upside down (2), sequenced a chord tone up (3) and closed open on the
 * third or fifth (4, a half cadence, held); then stated again (5), driven to the period's highest notes (6, 7) and
 * closed on the root (8, held). The second period climbs a step higher. Every beat is a chord tone; the notes between
 * run by scale steps into the next beat's tone. RHYTHM picks slow, middle or busy rhythms (slow: a tier slower),
 * PITCH the steps of the contour and the register, CHANGE how often the idea is new and how much a bar varies. */
static void lead_long(gen_t *G, const tools_t *T, int32_t b, int32_t p, int slow)
{
    static const int8_t RH[3][4][10] = {
        {{0, 8, -1}, {0, 6, 8, 12, -1}, {0, 4, 8, 12, -1}, {0, 8, 12, -1}},
        {{0, 4, 6, 8, 12, -1}, {0, 3, 6, 8, 12, 14, -1}, {0, 4, 8, 10, 12, -1}, {0, 2, 4, 8, 12, -1}},
        {{0, 2, 4, 6, 8, 10, 12, 14, -1}, {0, 2, 3, 4, 6, 8, 10, 11, 12, 14}, {0, 3, 6, 8, 10, 12, 14, -1}, {0, 1, 2, 4, 6, 8, 9, 10, 12, 14}}};
    static const int8_t ARC[8] = {0, 0, 1, 0, 0, 1, 2, 0};          /* the period's shape: up to bar 7 */
    const lane_t *L = GL;
    rng_t m = {hash3(g_ik, 31 + L->mel * 7, p)}, v = {hash3(g_bk, 37 + L->mel, p)};
    int32_t tier = (L->rhy < 34 ? 0 : L->rhy < 67 ? 1 : 2) - slow, pos = b % 8, per = (b / 8) % 2, i, n = 0, prev = -1000;
    int32_t on[11], c[11], step = L->pit < 34 ? 1 : L->pit < 67 ? 2 : 3, reg = L->pit > 80 ? 1 : 0, lift;
    const int8_t *rh;
    if (tier < 0) tier = 0;
    rh = RH[tier][(rnd(&m) >> 16) * 4u >> 16];
    for (i = 0; i < 10 && rh[i] >= 0; i++) on[n++] = rh[i];
    c[0] = PICK(&m, 2, 3, 3, 4) - 1;                                /* the motif's contour, in chord tones */
    for (i = 1; i < n; i++) {
        int32_t d = (int32_t)((rnd(&m) >> 16) * (uint32_t)(2 * step + 1) >> 16) - step;
        c[i] = c[i - 1] + d;
        if (c[i] < 0) c[i] = 0;
        if (c[i] > 4) c[i] = 4;
    }
    lift = ARC[pos] + per + reg;
    if (pos == 1 || pos == 5) for (i = 1; i < n; i++) c[i] = 2 * c[0] - c[i] < 0 ? 0 : 2 * c[0] - c[i];   /* the answer: upside down */
    if (pos == 2 || pos == 6) for (i = 0; i < n; i++) c[i] += 1;    /* the sequence: a chord tone up */
    if (pos != 3 && pos != 7 && n > 2 && chance(&v, L->chg / 3)) c[n - 1 - (int32_t)((rnd(&v) >> 16) * 2u >> 16)] += PICK(&v, -1, 1);
    if (pos == 3 || pos == 7) {                                     /* the cadence: half the motif, then the landing */
        int32_t land = slow || chance(&m, 50) ? 8 : 12, k = 0;
        for (i = 0; i < n && on[i] < land; i++) k++;
        n = k;
        on[n] = land; c[n] = pos == 3 ? PICK(&m, 1, 2) : (chance(&m, 70) ? 0 : 3); n++;
        lift = per + reg;
    }
    for (i = 0; i < n; i++) {
        int32_t note, st = on[i];
        if (pos == 7 && i == n - 1) note = (c[i] || reg ? ct(T, 0) + 12 : ct(T, 0));   /* the close: the root (or its octave; power chords too) */
        else if (st % 4 == 0 || i == 0) note = ct(T, c[i] + lift);  /* on the beat: a chord tone */
        else {                                                      /* between: a scale run into the next beat's tone */
            int32_t j = i + 1, k, nx;
            while (j < n && on[j] % 4) j++;
            k = j - i;
            nx = j < n ? (pos == 7 && j == n - 1 ? (c[j] || reg ? ct(T, 0) + 12 : ct(T, 0)) : ct(T, c[j] + lift)) : 0;
            if (j < n) note = move(T, nx, prev > nx ? k : -k);
            else note = move(T, prev, PICK(&v, -1, 1));
        }
        NOTE(G, st, 1, note);
        prev = note;
    }
    g_lens(G, pos == 3 || pos == 7 ? 16 : slow ? 16 : 4);
}

static void generate(int32_t style, gen_t *G, rng_t *r, const tools_t *T, int32_t b, int32_t p)
{
    int32_t s, i;
    G->n = 0;
    switch (style) {
    case S_DRIVE:               /* busy 16th/8th bass on the root, octave pops, a step out at the bar's end */
        for (s = 0; s < 16; s++) if (s == 0 || chance(r, s % 4 == 0 ? 95 : s % 2 == 0 ? 78 : 36)) NOTE(G, s, 1, ct(T, 0));
        g_lens(G, 2);
        for (i = 0; i < (int32_t)G->n; i++) {
            ev_t *e = &G->ev[i]; int32_t n = ct(T, 0); uint32_t x = (rnd(r) >> 16) * 100u >> 16;
            if ((e->st & 1) && x < 18) n += 12; else if (x < 8) n = ct(T, 1);
            if (e->st >= 14 && chance(r, 30)) n = stp(T, chance(r, 50) ? -1 : 1);
            e->note[0] = (uint8_t)n;
        }
        break;
    case S_PEDAL:               /* a held root, then repeated 8ths */
        NOTE(G, 0, 6, ct(T, 0));
        for (s = 6; s <= 14; s += 2) if (chance(r, 80)) NOTE(G, s, 2, ct(T, 0) + (s == 14 && chance(r, 40) ? 12 : 0));
        break;
    case S_OCTAVE:              /* octave-jumping 8ths */
        for (i = 0; i < 8; i++) NOTE(G, i * 2, 2, i == 6 && chance(r, 30) ? ct(T, 1) : ct(T, 0) + (i % 2 ? 12 : 0));
        break;
    case S_EPIC:                /* long lines on the chords */
        lead_long(G, T, b, p, 1);
        break;
    case S_STAB: {              /* syncopated chord stabs */
        static const int8_t PT[3][6] = {{0, 3, 6, 10, -1, -1}, {2, 6, 11, 14, -1, -1}, {0, 3, 8, 11, 14, -1}};
        const int8_t *pt = PT[(rnd(r) >> 16) * 3u >> 16];
        for (i = 0; i < 6 && pt[i] >= 0; i++)
            g_add(G, pt[i], (i + 1 < 6 && pt[i + 1] >= 0) ? 1 : 2, 3, ct(T, 0), ct(T, 1), ct(T, 2), 0);
        break; }
    case S_RIFF: {              /* a 2-beat 16th riff, the same for the whole profile, carried through every chord */
        rng_t m = {hash3(777, 0, p)};
        int32_t ms[8], mk[8], nm = 0, o = PICK(&m, 2, 4), h;
        for (s = 0; s < 8; s++) {
            if (s == 0 || chance(&m, 80)) { ms[nm] = s; mk[nm] = o; nm++; }
            if (!chance(&m, 37)) { o += PICK(&m, -1, 1, 1, 2, -2, 3, -3); if (o < 0) o = 0; if (o > 9) o = 9; }
        }
        for (h = 0; h < 16; h += 8)
            for (i = 0; i < nm; i++) {
                int32_t nx = i + 1 < nm ? ms[i + 1] : 8, k = mk[i] + (h && i == nm - 1 && chance(r, 30) ? 1 : 0);
                int32_t st = h + ms[i], note = stp(T, k);
                NOTE(G, st, (nx - ms[i]) > 2 ? 2 : nx - ms[i], st % 4 == 0 ? snap(T, note) : note);   /* beats on the chord */
            }
        break; }
    case S_MELODY:              /* 8ths and 16ths on the chords */
        lead_long(G, T, b, p, 0);
        break;
    case S_STRINGS:
        if (chance(r, 40)) { g_add(G, 0, 8, 3, ct(T, 0), ct(T, 1), ct(T, 2), 0); g_add(G, 8, 8, 3, ct(T, 0), ct(T, 1), ct(T, 2), 0); }
        else g_add(G, 0, 16, 3, ct(T, 0), ct(T, 1), ct(T, 2), 0);
        break;
    case S_SUS: {
        int32_t four = chance(r, 50);
        g_add(G, 0, 2, 3, ct(T, 0), ct(T, 0) + 5, ct(T, 0) + 7, 0);
        g_add(G, 6, 2, 3, ct(T, 0), ct(T, 0) + 5, ct(T, 0) + 7, 0);
        if (four) g_add(G, 10, 2, 3, ct(T, 0), ct(T, 0) + 5, ct(T, 0) + 7, 0);
        g_add(G, 12, 2, 3, ct(T, 0), ct(T, 0) + 5, ct(T, 0) + 7, 0);
        break; }
    case S_LUSH:
        g_add(G, 0, 8, 3, ct(T, 0), ct(T, 1), ct(T, 2), 0);
        g_add(G, 8, 8, 3, ct(T, 1), ct(T, 2), ct(T, 3), 0);
        break;
    case S_ARP:
        g_add(G, 0, 16, 3, ct(T, 0), ct(T, 1), ct(T, 2), EV_ARP);
        break;
    case S_SYMLEAD:             /* the symphony's lead: long arcs, legato */
        lead_long(G, T, b, p, 1);
        break;
    case S_ORCH: {              /* strings: open voicing (root, fifth, tenth); on a chord's first bar a suspension (the
                                 * fourth) resolving half way; on its last the inner voice moves up a step; swells */
        int32_t hr = PROF[p].hr ? PROF[p].hr : 1, first = b % hr == 0, last = b % hr == hr - 1;
        int32_t third = T->H->nt == 3 && (T->H->tones[1] == 3 || T->H->tones[1] == 4), open = GL->pit >= 34;
        int32_t lo = ct(T, 0), mid = open ? ct(T, 2) : ct(T, 1), hi = open ? ct(T, 4) : ct(T, 2);
        uint32_t acc = b % 16 < 2 ? EV_ACC2 : b % 8 < 4 ? EV_ACC1 : 0;  /* softer as a section starts, full at its height */
        if (first && third) { g_add(G, 0, 8, 3, lo, mid, open ? ct(T, 0) + 17 : ct(T, 0) + 5, acc); g_add(G, 8, 8, 3, lo, mid, hi, acc); }
        else if (last && hr > 1) { g_add(G, 0, 8, 3, lo, mid, hi, acc); g_add(G, 8, 8, 3, lo, move(T, mid, 1), hi, acc); }
        else g_add(G, 0, 16, 3, lo, mid, hi, acc);
        break; }
    case S_CELLO: {             /* cellos and basses: the root held (an octave under it when it is high enough); on a
                                 * chord's last bar a walk down by step into the next; RHYTHM high: quarters in octaves */
        int32_t hr = PROF[p].hr ? PROF[p].hr : 1, last = b % hr == hr - 1, root = ct(T, 0);
        if (GL->rhy > 66) { for (s = 0; s < 16; s += 4) NOTE(G, s, 4, s % 8 ? root + 12 : root); break; }
        if (last) { g_add(G, 0, 8, root - 12 >= 28 ? 2 : 1, root, root - 12, 0, 0); NOTE(G, 8, 4, move(T, root, -1)); NOTE(G, 12, 4, move(T, root, -2)); }
        else g_add(G, 0, 16, root - 12 >= 28 ? 2 : 1, root, root - 12, 0, 0);
        break; }
    case S_DORCH:               /* orchestral percussion: timpani on the one, a roll into the phrase, the bass drum
                                 * every other bar, a cymbal on the phrase; PITCH: timpani on three too */
        DRUM(G, 0, DK_KETTLE);
        if (GL->pit > 50 && b % 2) DRUM(G, 8, DK_KETTLE);
        if (b % 2 == 0) { ev_t *e = DRUM(G, 0, DK_KICK); if (e) e->flags = EV_ACC1; }
        if (b % 8 == 0) DRUM(G, 0, DK_CRASH);
        if (b % 4 == 3) for (s = 12; s < 16; s++) { ev_t *e = DRUM(G, s, DK_KETTLE); if (e) e->flags = s == 15 ? 0 : s < 14 ? EV_ACC2 : EV_ACC1; }
        break;
    case S_CHOP: {              /* a chopped pad (a tracker re-strikes a long sample by offset): the chord every 16th, accented */
        static const uint8_t AC[3][16] = {{0, 2, 1, 2, 0, 2, 1, 2, 0, 2, 1, 2, 0, 2, 1, 1},
                                          {0, 2, 2, 1, 2, 2, 1, 2, 0, 2, 2, 1, 2, 1, 2, 1},
                                          {0, 3, 1, 2, 0, 3, 1, 2, 0, 3, 1, 2, 0, 2, 1, 2}};
        rng_t m = {hash3(b / 4, 41, p)};
        const uint8_t *a = AC[(rnd(&m) >> 16) * 3u >> 16];
        for (s = 0; s < 16; s++)
            if (a[s] < 3) g_add(G, s, 1, 3, ct(T, 0), ct(T, 1), ct(T, 2), a[s] == 1 ? EV_ACC1 : a[s] == 2 ? EV_ACC2 : 0);
        break; }
    case S_ROOTS:               /* deep and roomy: the root held, a rest, the fifth, the root again, a step into the turn */
        NOTE(G, 0, 3, ct(T, 0));
        if (chance(r, 70)) NOTE(G, 6, 2, ct(T, 1) - (ct(T, 1) - ct(T, 0) > 5 ? 12 : 0));
        NOTE(G, 8, 3, ct(T, 0));
        if (chance(r, 60)) NOTE(G, 11, 1, ct(T, 0) + 12);
        if (chance(r, 55)) NOTE(G, 14, 2, stp(T, PICK(r, -1, 1, 2)));
        break;
    case S_GALLOP: {            /* the gallop: an 8th and two 16ths, on the root, an octave on the bar's last beat */
        static const int8_t GP[12] = {0, 2, 3, 4, 6, 7, 8, 10, 11, 12, 14, 15};
        for (i = 0; i < 12; i++) NOTE(G, GP[i], 1, GP[i] >= 12 && b % 2 && chance(r, 40) ? ct(T, 0) + 12 : ct(T, 0));
        break; }
    case S_SLAP: {              /* slapped and popped: the root on the one, octaves off the beat, ghosts, the seventh */
        static const int8_t SP[3][8] = {{0, 3, 6, 7, 10, 11, 14, -1}, {0, 2, 6, 8, 10, 13, 14, -1}, {0, 3, 4, 7, 10, 12, 15, -1}};
        rng_t m = {hash3(b / 2, 53, p)};
        const int8_t *sp = SP[(rnd(&m) >> 16) * 3u >> 16];
        for (i = 0; i < 8 && sp[i] >= 0; i++) {
            int32_t st = sp[i], n = st == 0 ? ct(T, 0) : (st % 4 == 3 || st % 4 == 2) && chance(r, 55) ? ct(T, 0) + 12
                       : chance(r, 25) ? stp(T, -1) : chance(r, 30) ? ct(T, 1) : ct(T, 0);
            ev_t *e = NOTE(G, st, 1, n);
            if (e && st % 2 && chance(r, 30)) e->flags = EV_ACC2;           /* a ghost note */
        }
        break; }
    case S_ACID: {              /* acid: 16ths on the root with octave jumps and accents; some notes slide into the next */
        rng_t m = {hash3(b / 4, 59, p)};
        for (s = 0; s < 16; s++) {
            int on = s % 4 == 0 || chance(&m, 62);
            if (!on) continue;
            {
                int32_t n = ct(T, 0) + (chance(&m, 30) ? 12 : 0) + (chance(&m, 10) ? 7 : 0);
                ev_t *e = NOTE(G, s, 1, n);
                if (e) e->flags = (uint8_t)((s % 4 ? EV_ACC1 : 0) | (chance(&m, 22) ? EV_GLIDE : 0));
            }
        }
        break; }
    case S_SKANK:               /* the off-beat chord, short; a doubled stroke now and then */
        for (s = 2; s < 16; s += 4) {
            g_add(G, s, 1, 3, ct(T, 1), ct(T, 2), ct(T, 3), 0);
            if (chance(r, 20)) g_add(G, s + 1, 1, 3, ct(T, 1), ct(T, 2), ct(T, 3), EV_ACC2);
        }
        break;
    case S_PCHORD:               /* power chords, root fifth octave: held half bars, or chugged 8ths at the height */
        if (chance(r, 60)) { g_add(G, 0, 8, 3, ct(T, 0), ct(T, 0) + 7, ct(T, 0) + 12, 0); g_add(G, 8, 8, 3, ct(T, 0), ct(T, 0) + 7, ct(T, 0) + 12, 0); }
        else for (s = 0; s < 16; s += 2) g_add(G, s, 2, 3, ct(T, 0), ct(T, 0) + 7, ct(T, 0) + 12, s % 4 ? EV_ACC1 : 0);
        break;
    case S_CLAV: {              /* clav stabs on syncopated 16ths, the chord's upper notes */
        static const int8_t CP[3][7] = {{1, 3, 6, 9, 11, 14, -1}, {0, 3, 5, 8, 11, 13, -1}, {2, 4, 7, 10, 12, 15, -1}};
        rng_t m = {hash3(b / 2, 61, p)};
        const int8_t *cp = CP[(rnd(&m) >> 16) * 3u >> 16];
        for (i = 0; i < 7 && cp[i] >= 0; i++) g_add(G, cp[i], 1, 3, ct(T, 1), ct(T, 2), ct(T, 3), i % 2 ? EV_ACC1 : 0);
        break; }
    case S_DTROPIC: {           /* the one and the three, rim shots on two and four with a push, congas, shaker 8ths */
        static const int8_t K[16] = {100, 0, 30, 20, 0, 0, 30, 0, 90, 0, 40, 0, 0, 0, 25, 0}, SN[16] = {0, 0, 0, 0, 90, 0, 0, 45, 0, 35, 0, 0, 90, 0, 25, 30},
                            H[16] = {50, 0, 85, 0, 50, 0, 85, 0, 50, 0, 85, 0, 50, 0, 85, 0}, TT[16] = {0, 0, 0, 45, 0, 0, 55, 0, 0, 0, 0, 45, 0, 0, 55, 0};
        hits(G, r, DK_KICK, K); hits(G, r, DK_SNARE, SN); hits(G, r, DK_HAT, H); hits(G, r, DK_KETTLE, TT);
        break; }
    case S_DSTEEL: {            /* double kick on the gallop, the snare on two and four, the ride on 8ths */
        static const int8_t K[16] = {100, 0, 0, 85, 90, 0, 0, 85, 100, 0, 0, 85, 90, 0, 0, 85}, SN[16] = {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 20},
                            H[16] = {90, 0, 90, 0, 90, 0, 90, 0, 90, 0, 90, 0, 90, 0, 90, 0};
        hits(G, r, DK_KICK, K); hits(G, r, DK_SNARE, SN); hits(G, r, DK_HAT, H);
        if (b % 4 == 0) DRUM(G, 0, DK_CRASH);
        break; }
    case S_DDRIFT: {            /* no snare: a soft heartbeat kick, sparse hats, a low drum into the phrase */
        static const int8_t K[16] = {80, 0, 0, 0, 0, 0, 40, 0, 0, 0, 30, 0, 0, 0, 0, 0}, H[16] = {0, 0, 30, 0, 0, 0, 30, 0, 0, 0, 30, 0, 0, 0, 30, 0};
        for (s = 0; s < 16; s++) if (K[s] && chance(r, K[s])) { ev_t *e = DRUM(G, s, DK_KICK); if (e && s) e->flags = EV_ACC1; }
        for (s = 0; s < 16; s++) if (H[s] && chance(r, H[s])) { ev_t *e = DRUM(G, s, DK_HAT); if (e) e->flags = EV_ACC2; }
        if (b % 4 == 3) DRUM(G, 12, DK_KETTLE);
        break; }
    case S_DFUNK: {             /* the kick on the one and off the beat, the snare on two and four with ghosts, 16th hats */
        static const int8_t K[16] = {100, 0, 0, 50, 0, 0, 0, 30, 0, 0, 75, 0, 0, 25, 0, 0}, SN[16] = {0, 0, 0, 0, 0, 0, 0, 35, 0, 40, 0, 0, 0, 0, 0, 35},
                            H[16] = {85, 55, 85, 55, 85, 55, 85, 55, 85, 55, 85, 55, 85, 55, 85, 55};
        hits(G, r, DK_KICK, K); hits(G, r, DK_HAT, H);
        DRUM(G, 4, DK_SNARE); DRUM(G, 12, DK_SNARE);
        for (s = 0; s < 16; s++) if (SN[s] && chance(r, SN[s])) { ev_t *e = DRUM(G, s, DK_SNARE); if (e) e->flags = EV_ACC2; }
        break; }
    case S_DRAVE: {             /* four on the floor, a clap on two and four, the open hat on the off-beats */
        static const int8_t H[16] = {0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0}, H2[16] = {0, 40, 0, 40, 0, 40, 0, 40, 0, 40, 0, 40, 0, 40, 0, 40};
        for (s = 0; s < 16; s += 4) DRUM(G, s, DK_KICK);
        DRUM(G, 4, DK_SNARE); DRUM(G, 12, DK_SNARE);
        hits(G, r, DK_HAT, H);
        for (s = 0; s < 16; s++) if (H2[s] && chance(r, H2[s])) { ev_t *e = DRUM(G, s, DK_HAT); if (e) e->flags = EV_ACC2; }
        if (b % 8 == 0) DRUM(G, 0, DK_CRASH);
        break; }
    case S_DEPIC: {
        static const int8_t K[16] = {100, 0, 20, 0, 0, 0, 20, 0, 100, 0, 20, 0, 36, 0, 0, 0}, TT[16] = {60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 45, 0, 40, 25},
                            SN[16] = {0, 0, 0, 0, 92, 0, 0, 20, 0, 0, 0, 0, 92, 0, 35, 20}, H[16] = {75, 38, 75, 38, 75, 38, 75, 38, 75, 38, 75, 38, 75, 38, 75, 38};
        hits(G, r, DK_KICK, K); hits(G, r, DK_KETTLE, TT); hits(G, r, DK_SNARE, SN); hits(G, r, DK_HAT, H);
        if (b % 4 == 0) DRUM(G, 0, DK_CRASH);
        break; }
    case S_DTECH: {
        static const int8_t K[16] = {100, 0, 25, 0, 50, 0, 25, 0, 100, 0, 25, 0, 75, 0, 25, 0}, SN[16] = {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0},
                            H[16] = {95, 60, 95, 60, 95, 60, 95, 60, 95, 60, 95, 60, 95, 60, 95, 60};
        hits(G, r, DK_KICK, K); hits(G, r, DK_SNARE, SN); hits(G, r, DK_HAT, H);
        if (b % 8 == 0) DRUM(G, 0, DK_CRASH);
        break; }
    case S_DFUSION: {
        static const int8_t K[16] = {100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0}, SN[16] = {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 30},
                            H[16] = {85, 85, 85, 85, 85, 85, 85, 0, 85, 85, 85, 85, 85, 85, 85, 0};
        hits(G, r, DK_KICK, K); hits(G, r, DK_SNARE, SN); hits(G, r, DK_HAT, H);
        if (b % 4 == 0) DRUM(G, 0, DK_CRASH);
        break; }
    default: {
        static const int8_t K[16] = {100, 0, 16, 0, 34, 0, 16, 0, 100, 0, 16, 0, 34, 0, 16, 0}, SN[16] = {0, 0, 0, 0, 100, 0, 0, 18, 0, 0, 0, 0, 100, 0, 18, 18},
                            H[16] = {40, 0, 40, 0, 40, 0, 40, 0, 40, 0, 40, 0, 40, 0, 40, 0};
        hits(G, r, DK_KICK, K); hits(G, r, DK_SNARE, SN); hits(G, r, DK_HAT, H);
        break; }
    }
}

/* the sfx lane: AUTO follows the arc and the profile; one chosen sound plays its own pattern. dens 50 = as designed */
static void gen_sfx(gen_t *G, rng_t *r, const tools_t *T, int32_t b, int32_t p, int32_t dens, const arc_t *A, int next_drop)
{
    static const int8_t OFF[4] = {2, 6, 10, 14};
    int32_t only = fxu[L_SFX].inst, root = ct(T, 0), x = dens * 2, i, s;
    G->n = 0;
    if (only < 0) {
        int32_t e = A->mode == A_OFF ? 154 : A->e;
        int drop = A->drop || (A->mode == A_OFF && b > 0 && b % 8 == 0), pre = next_drop || (A->mode == A_OFF && b % 8 == 7);
        if (drop) SFX(G, 0, 2, SX_IMPACT, root);
        if (pre) { SFX(G, 0, 16, SX_RISER, root); return; }        /* the bar before the drop: the riser, alone */
        switch (p) {
        case 0:                                                     /* EPIC: washes, a few zaps at the top */
            if (b % 4 == 0 && !drop && chance(r, x)) SFX(G, 0, 16, SX_WASH, root);
            if (e > 200) for (i = 0; i < 4; i++) if (chance(r, x * 15 / 100)) SFX(G, OFF[i] + 1, 1, SX_ZAP, root + 12);
            break;
        case 1:                                                     /* HYPERSPACE: zaps, bits at the top */
            for (i = 0; i < 4; i++) if (chance(r, x * e / 256 * 40 / 100)) SFX(G, OFF[i], 1, SX_ZAP, root + PICK(r, 0, 7, 12));
            if (e > 200) for (s = 1; s < 16; s += 2) if (chance(r, x * 30 / 100)) SFX(G, s, 1, SX_BITS, root);
            break;
        case 2:                                                     /* FUSION: a soft wash a phrase */
            if (b % 4 == 0 && !drop && chance(r, x * 80 / 100)) SFX(G, 0, 16, SX_WASH, root);
            if (e > 200 && chance(r, x * 20 / 100)) SFX(G, 14, 1, SX_ZAP, root + 12);
            break;
        case 4:                                                     /* SYMPHONY: a cymbal swell into each phrase */
            if (b % 4 == 3 && !drop && chance(r, x)) SFX(G, 8, 8, SX_WASH, root);
            break;
        case 5:                                                     /* TROPIC: waves, a long wash every other phrase */
            if (b % 8 == 0 && !drop && chance(r, x)) SFX(G, 0, 16, SX_WASH, root);
            break;
        case 6:                                                     /* STEEL: an impact on the phrase, bits at the top */
            if (b % 4 == 0 && !drop && chance(r, x * 60 / 100)) SFX(G, 0, 2, SX_IMPACT, root);
            if (e > 200) for (i = 0; i < 4; i++) if (chance(r, x * 20 / 100)) SFX(G, OFF[i], 1, SX_BITS, root);
            break;
        case 7:                                                     /* DRIFT: long washes, a rare zap far off */
            if (b % 2 == 0 && !drop && chance(r, x)) SFX(G, 0, 16, SX_WASH, root);
            if (chance(r, x * 10 / 100)) SFX(G, 10, 1, SX_ZAP, root + 24);
            break;
        case 8:                                                     /* FUNK: vinyl crackle under it all */
            if (chance(r, x)) SFX(G, 0, 16, SX_CRACKLE, root);
            break;
        case 9:                                                     /* RAVE: zaps on the off-beats, a riser every phrase */
            for (i = 0; i < 4; i++) if (chance(r, x * 35 / 100)) SFX(G, OFF[i], 1, SX_ZAP, root + PICK(r, 12, 19, 24));
            if (b % 4 == 3 && !drop) SFX(G, 8, 8, SX_RISER, root);
            break;
        default:                                                    /* OLDSKOOL: crackle under everything, bits */
            if (chance(r, x)) SFX(G, 0, 16, SX_CRACKLE, root);
            for (i = 0; i < 4; i++) if (chance(r, x * e / 256 * 30 / 100)) SFX(G, OFF[i] + 1, 1, SX_BITS, root);
            break;
        }
        g_sort(G);
        return;
    }
    switch (only) {                                                 /* one sound */
    case SX_WASH: case SX_CRACKLE: if (chance(r, x)) SFX(G, 0, 16, only, root); break;
    case SX_RISER: if (next_drop || b % 4 == 3) SFX(G, 0, 16, SX_RISER, root); break;
    case SX_IMPACT: if (A->drop || b % 4 == 0) SFX(G, 0, 2, SX_IMPACT, root); break;
    case SX_ZAP: for (i = 0; i < 4; i++) if (chance(r, x * 45 / 100)) SFX(G, OFF[i], 1, SX_ZAP, root + PICK(r, 0, 7, 12)); break;
    default: for (s = 0; s < 16; s++) if (chance(r, s % 2 ? x * 25 / 100 : x * 50 / 100)) SFX(G, s, 1, SX_BITS, root); break;
    }
}

/* your own pattern for this bar, following the chord */
static void user_bar(int32_t l, gen_t *G, const tools_t *T, int32_t b)
{
    const upat_t *U = &ln[l].up;
    int32_t off = fmodp(b, U->bars ? U->bars : 1) * 16, s, k;
    G->n = 0;
    for (s = 0; s < 16; s++) {
        if (l == L_DRUMS) { for (k = 0; k < NDK; k++) if (U->kinds[off + s] >> k & 1u) DRUM(G, s, k); continue; }
        if (l == L_SFX) {
            for (k = 0; k < NSX; k++) if (U->kinds[off + s] >> k & 1u)
                SFX(G, s, k == SX_WASH || k == SX_RISER || k == SX_CRACKLE ? 16 - s : 1, k, ct(T, 0));
            continue;
        }
        if (U->k[off + s] == EMPTY) continue;
        k = U->k[off + s];
        if (l == L_PAD) g_add(G, s, U->len[off + s] > 16 - s ? 16 - s : U->len[off + s], 3, stp(T, k), stp(T, k + 2), stp(T, k + 4), 0);
        else NOTE(G, s, U->len[off + s] > 16 - s ? 16 - s : U->len[off + s], stp(T, k));
    }
}

/* density: 50 = as designed; lower thins it out; higher adds notes (in the chord and its scale) */
static void densify(int32_t l, gen_t *G, rng_t *r, const tools_t *T, int32_t b, int32_t dens)
{
    int32_t x = (dens - 50) * 2, s;
    uint32_t i, j;
    if (x < 0) {                                    /* thin: notes between the beats go first */
        for (i = 0, j = 0; i < G->n; i++) if (G->ev[i].st == 0 || chance(r, G->ev[i].st % 4 == 0 ? 100 + x / 2 : 100 + x)) G->ev[j++] = G->ev[i];
        G->n = j;
        return;
    }
    if (!x) return;
    if (l == L_DRUMS && g_style == S_DORCH) {         /* more: a march snare, longer rolls */
        if (x > 30) { DRUM(G, 4, DK_SNARE); DRUM(G, 12, DK_SNARE); if (chance(r, x)) DRUM(G, 14, DK_SNARE); }
        if (b % 4 == 3) for (s = 8; s < 12; s++) if (chance(r, x)) { ev_t *e = DRUM(G, s, DK_KETTLE); if (e) e->flags = EV_ACC2; }
        g_sort(G);
        return;
    }
    if (l == L_PAD && g_style == S_ORCH) return;
    if (l == L_DRUMS && g_style == S_DDRIFT) {          /* more, but never a snare: hats and a second kick */
        for (s = 2; s < 16; s += 2) if (chance(r, x * 50 / 100)) { ev_t *e = DRUM(G, s, DK_HAT); if (e) e->flags = EV_ACC2; }
        if (chance(r, x * 40 / 100)) { ev_t *e = DRUM(G, 8, DK_KICK); if (e) e->flags = EV_ACC1; }
        g_sort(G);
        return;
    }
    if (l == L_DRUMS) {
        uint32_t has[NDK] = {0};
        static const int8_t KS[7] = {2, 3, 6, 7, 10, 11, 14}, GS[6] = {1, 3, 5, 9, 11, 13};
        for (i = 0; i < G->n; i++) has[G->ev[i].note[0]] |= 1u << G->ev[i].st;
#define ADD(st, k, pr) do { if (!(has[k] >> (st) & 1u) && chance(r, pr)) { DRUM(G, st, k); has[k] |= 1u << (st); } } while (0)
        for (s = 0; s < 16; s++) ADD(s, DK_HAT, x * 90 / 100);                       /* 16th hats */
        for (i = 0; i < 7; i++) ADD(KS[i], DK_KICK, x * 35 / 100);                  /* syncopated kicks */
        for (i = 0; i < 6; i++) ADD(GS[i], DK_SNARE, x * 25 / 100);                 /* ghost snares */
        if (b % 4 == 3) { for (s = 12; s < 16; s++) ADD(s, DK_SNARE, x * 80 / 100); ADD(14, DK_KETTLE, x * 50 / 100); }   /* a fill into the phrase */
#undef ADD
        g_sort(G);
        return;
    }
    if (l == L_PAD) {                                                                /* re-strike long chords in a rhythm */
        gen_t O; O.n = 0;
        for (i = 0; i < G->n; i++) {
            ev_t *e = &G->ev[i];
            if ((e->flags & EV_ARP) || e->len < 8 || !chance(r, x)) { if (O.n < MAXEV) O.ev[O.n++] = *e; continue; }
            {
                int32_t stepw = x > 60 ? 2 : 4, end = e->st + e->len;
                for (s = e->st; s < end; s += stepw) { ev_t *c = g_add(&O, s, end - s < stepw ? end - s : stepw, e->n, e->note[0], e->note[1], e->note[2], 0); (void)c; }
            }
        }
        *G = O;
        return;
    }
    if (l == L_LEAD) {                                  /* more: a 16th leading into a beat note, never a changed note */
        uint32_t on = 0, n0 = G->n;
        for (i = 0; i < n0; i++) on |= 1u << G->ev[i].st;
        for (i = 0; i < n0; i++) {
            ev_t *e = &G->ev[i];
            int32_t s0 = e->st - 1, up = 0, pst = -1;
            uint32_t k;
            if (e->st % 4 || s0 < 0 || on >> s0 & 1u || !chance(r, x * 60 / 100)) continue;
            for (k = 0; k < n0; k++) if (G->ev[k].st < s0 && G->ev[k].st > pst) { pst = G->ev[k].st; up = G->ev[k].note[0] > e->note[0]; }
            if (G->n < MAXEV) { NOTE(G, s0, 1, move(T, e->note[0], up ? 1 : -1)); on |= 1u << s0; }   /* from the side it comes from */
        }
        g_sort(G);                                      /* a note ends where the next begins, no later */
        for (i = 0; i + 1 < G->n; i++) if (G->ev[i + 1].st > G->ev[i].st && G->ev[i].st + G->ev[i].len > G->ev[i + 1].st) G->ev[i].len = (uint8_t)(G->ev[i + 1].st - G->ev[i].st);
        return;
    }
    {                                                                                /* bass */
        uint32_t on = 0;
        for (i = 0; i < G->n; i++) {
            ev_t *e = &G->ev[i];
            on |= 1u << e->st;
            if (e->n != 1 || (e->st == 0 && l == L_BASS)) continue;               /* the bass keeps the root on the one */
            if (chance(r, x * 45 / 100))
                e->note[0] = (uint8_t)(l == L_BASS ? ct(T, PICK(r, 1, 2, 3, -1)) : snap(T, move(T, e->note[0], PICK(r, -1, 1, 2, -2))));
        }
        for (s = 1; s < 16; s++) {
            const ev_t *prev = 0;
            int32_t n;
            if (on >> s & 1u || !chance(r, s % 2 ? x * 40 / 100 : x * 70 / 100)) continue;
            for (i = 0; i < G->n; i++) if (G->ev[i].st < s && (!prev || G->ev[i].st > prev->st)) prev = &G->ev[i];
            if (!prev || prev->n != 1) continue;
            n = move(T, prev->note[0], PICK(r, -1, 1, 1, -2, 2));
            if (s % 4 == 0) n = snap(T, n);
            NOTE(G, s, 1, n);
            on |= 1u << s;
        }
        g_sort(G);
        for (i = 0; i < G->n; i++) {
            if (G->ev[i].n != 1) continue;
            for (j = i + 1; j < G->n; j++) if (G->ev[j].st > G->ev[i].st) {
                int32_t d = G->ev[j].st - G->ev[i].st;
                if (G->ev[i].len > d) G->ev[i].len = (uint8_t)d;
                break;
            }
        }
    }
}

/* PITCH on the other lanes: the bass jumps octaves more (or never), the pad's chords open up (the middle note an
 * octave higher), the drums bring the timpani in on the off-beats */
static void pitch_shape(int32_t l, gen_t *G, rng_t *r, const tools_t *T)
{
    int32_t pit = GL->pit, x = pit - 50;
    uint32_t i;
    if (l == L_BASS) {
        for (i = 0; i < G->n; i++) {
            ev_t *e = &G->ev[i];
            if (e->n != 1 || e->st == 0) continue;
            if (x > 10 && chance(r, x)) e->note[0] = (uint8_t)(e->note[0] + 12 > 70 ? e->note[0] : e->note[0] + 12);
            else if (x < -10 && e->note[0] >= (uint8_t)(ct(T, 0) + 12)) e->note[0] = (uint8_t)(e->note[0] - 12);
        }
    } else if (l == L_PAD && g_style != S_ORCH && pit >= 67) {
        for (i = 0; i < G->n; i++) if (G->ev[i].n == 3 && !(G->ev[i].flags & EV_ARP)) G->ev[i].note[1] = (uint8_t)(G->ev[i].note[1] + 12);
    } else if (l == L_DRUMS && x > 10) {
        static const int8_t OFF[4] = {3, 7, 11, 14};
        for (i = 0; i < 4u; i++) if (chance(r, x)) { ev_t *e = DRUM(G, OFF[i], DK_KETTLE); if (e) e->flags = EV_ACC1; }
        g_sort(G);
    }
}

/* how the style plays: lead notes glide in from the last one (not over a leap wider than a fifth), long chords swell
 * in or fade out; the tracker's tone portamento and volume slides */
static void play_shape(int32_t l, gen_t *G, rng_t *r, int32_t p)
{
    const prof_t *P = &PROF[p];
    uint32_t i;
    int32_t prev = -1;
    if (l == L_LEAD && P->glide)
        for (i = 0; i < G->n; i++) {
            ev_t *e = &G->ev[i];
            if (e->n != 1) { prev = -1; continue; }
            if (prev >= 0 && e->note[0] != prev && (e->note[0] > prev ? e->note[0] - prev : prev - e->note[0]) <= 7 && chance(r, P->glide))
                e->flags |= EV_GLIDE;
            prev = e->note[0];
        }
    if ((l == L_PAD || l == L_BASS) && P->swell)
        for (i = 0; i < G->n; i++) {
            ev_t *e = &G->ev[i];
            if (e->len < 8 || (e->flags & EV_ARP) || !chance(r, l == L_BASS ? P->swell / 2 : P->swell)) continue;
            e->flags |= chance(r, 65) ? EV_SWELL : EV_FADE;
        }
}

/* ---- one bar, all lanes */
static void gen_bar(int32_t b, bar_t *B)
{
    static gen_t G;
    harm_t H;
    arc_t A = arc_at(b);
    int32_t l;
    harm_at(b, &H);
    B->crash_on_one = (uint8_t)A.drop;
    int next_drop = arc_at(b + 1).drop;
    for (l = 0; l < (int32_t)NLANE; l++) {
        int32_t p = eff_prof(l, b), dens = ln[l].dens;
        rng_t r;
        tools_t T;
        uint32_t i;
        GL = &ln[l];
        keys_for(GL, b);
        g_style = -1;
        r.a = hash3(g_bk, l, p) ^ ((uint32_t)GL->mel * 2654435761u);
        tools_make(&T, &H, LBASE[l] + ln[l].oct * 12 + key_off);
        B->prof[l] = (uint8_t)p; B->inst[l] = PROF[p].inst[l]; B->user[l] = ln[l].up.on; B->cnt[l] = 0;
        B->muted[l] = (uint8_t)(!lane_active(&A, l) || !form_plays(l, b));
        if (B->muted[l]) continue;
        if (ln[l].up.on) user_bar(l, &G, &T, b);
        else if (l == L_SFX) gen_sfx(&G, &r, &T, b, p, dens, &A, next_drop);
        else { g_style = PROF[p].st[l]; generate(g_style, &G, &r, &T, b, p); }
        if (A.mode != A_OFF) dens += (A.e - 154) * 80 / 256;
        dens += energy;
        if (l != L_LEAD) dens += (GL->rhy - 50) * 4 / 5;            /* RHYTHM: busier or sparser (the lead: its rhythms) */
        if (dens < 0) dens = 0;
        if (dens > 100) dens = 100;
        if (l != L_SFX) densify(l, &G, &r, &T, b, dens);
        if (!ln[l].up.on) pitch_shape(l, &G, &r, &T);
        if (!HITS(l)) play_shape(l, &G, &r, p);
        if (A.drop && l == L_DRUMS) {
            int has_c = 0, has_k = 0;
            for (i = 0; i < G.n; i++) if (G.ev[i].st == 0) { if (G.ev[i].note[0] == DK_CRASH) has_c = 1; if (G.ev[i].note[0] == DK_KICK) has_k = 1; }
            if (!has_c) DRUM(&G, 0, DK_CRASH);
            if (!has_k) DRUM(&G, 0, DK_KICK);
        }
        for (i = 0; i < G.n; i++) { G.ev[i].rv = (uint8_t)(rnd(&r) >> 24); B->ev[l][i] = G.ev[i]; }
        B->cnt[l] = (uint8_t)G.n;
    }
}

/* ---- the cache: bars cb-1 .. cb+3, regenerated when stale; the interrupt reads the active buffer */
static uint8_t stale[NSLOT];
static int32_t frozen_step;
static int32_t cur_step(void) { return seq_playing ? (int32_t)vis_step : frozen_step; }
static int32_t cur_bar(void) { return cur_step() >> 4; }
static void invalidate_from(int32_t from)
{
    uint32_t s;
    for (s = 0; s < NSLOT; s++) if (bars[s][bar_sel[s]].num >= from) stale[s] = 1;
}
static void invalidate(void) { invalidate_from(cur_bar()); }            /* changes land at once: the rest of this bar is written again */
static void invalidate_now(void) { invalidate_from(cur_bar()); }
static const bar_t *bar_get(int32_t b)
{
    const bar_t *B;
    if (b < 0) return 0;
    B = &bars[b % NSLOT][bar_sel[b % NSLOT]];
    return B->num == b ? B : 0;
}
static int bars_update(void)                    /* one bar per call at most: 1 = did work */
{
    int32_t cb = cur_bar(), b;
    for (b = cb < 1 ? 0 : cb - 1; b <= cb + 3; b++) {
        uint32_t slot = (uint32_t)b % NSLOT, sel = bar_sel[slot];
        if (bars[slot][sel].num != b || stale[slot]) {
            bar_t *O = &bars[slot][sel ^ 1u];
            gen_bar(b, O);
            O->num = b;
            RING_PUBLISH();
            bar_sel[slot] = (uint8_t)(sel ^ 1u);
            stale[slot] = 0;
            return 1;
        }
    }
    return 0;
}
static void music_init(void)
{
    uint32_t s, l, i;
    for (s = 0; s < NSLOT; s++) { bars[s][0].num = -1; bars[s][1].num = -1; }
    for (l = 0; l < NLANE; l++) {
        ln[l].prof = 2; ln[l].lock = 0;                 /* it starts with FUSION */
        ln[l].dens = 50; ln[l].oct = 0;
        ln[l].mel = 0; ln[l].rhy = 50; ln[l].pit = 50; ln[l].chg = 40; ln[l].mute = 0;
        ln[l].up.on = 0; ln[l].up.bars = 1;
        for (i = 0; i < 32u; i++) { ln[l].up.k[i] = EMPTY; ln[l].up.len[i] = 1; ln[l].up.kinds[i] = 0; }
    }
}
static int32_t launch_tempo = 1;                                /* LAUNCH brings the style's tempo (SEL + SEQ: keep yours) */
static void set_bpm(int32_t b);
static void commit_launch(void)
{
    int32_t l;
    for (l = 0; l < (int32_t)NLANE; l++) if (!ln[l].lock) ln[l].prof = (uint8_t)target;
    if (launch_tempo) set_bpm(PROF[target].bpm);
    blend = 0; launch_bar = -1;
}

/* ---- DROP: the break starts at once (the rest of this bar is written again). Hold = the drop the moment you let
 * go (a crash and an impact then, the bar written again); tap = INTER (the form, above). The lane selected when DROP
 * goes down plays alone. */
static int32_t drop_down_ms, drop_seg = -1;
static void arc_restart(int32_t from) { segs[0].mode = A_BUILD; segs[0].from = from; segs[0].e0 = 0; segs[0].until = -1; nsegs = 1; }
static void drop_press(int32_t lane)
{
    int32_t nb = cur_bar(), i, j;
    arc_t A = arc_at(cur_bar());
    if (!arc_on) { brk_from = nb; brk_until = -1; brk_lane = lane; drop_seg = 0; drop_down_ms = (int32_t)fm1_ms; invalidate(); return; }
    for (i = 0, j = 0; i < nsegs; i++) if (segs[i].from < nb) segs[j++] = segs[i];
    nsegs = j;
    if (nsegs >= 16) nsegs = 15;
    segs[nsegs].mode = A_BREAK; segs[nsegs].from = nb; segs[nsegs].e0 = A.e; segs[nsegs].until = -1; segs[nsegs].lane = lane;
    drop_seg = nsegs++;
    drop_down_ms = (int32_t)fm1_ms;
    invalidate();
}
static void drop_hit(void);                                     /* the crash and the impact, now (the UI's) */
static int32_t drop_tapped;                                     /* the last release was a tap: an INTER (the UI says so) */
static void drop_release(void)
{
    seg_t *s;
    int tap = (int32_t)fm1_ms - drop_down_ms < 350, now = 0;
    if (drop_seg < 0) return;
    drop_tapped = tap;
    if (tap) {                                                  /* a tap: INTER, from the bar the break began in */
        int32_t from = arc_on ? segs[drop_seg].from : brk_from, lane = arc_on ? segs[drop_seg].lane : brk_lane;
        if (arc_on) { if (drop_seg == nsegs - 1) nsegs--; else segs[drop_seg].until = segs[drop_seg].from; }
        else { brk_from = brk_until = -1; }
        form_inter(lane, from);
        drop_seg = -1;
        invalidate();
        return;
    }
    if (!arc_on) { brk_until = cur_bar() > brk_from ? cur_bar() : brk_from + 1; now = brk_until == cur_bar(); }   /* hold: now */
    else { s = &segs[drop_seg]; s->until = cur_bar() > s->from ? cur_bar() : s->from + 1; now = s->until == cur_bar(); }
    drop_seg = -1;
    invalidate();
    if (now) drop_hit();
}

/* ---- VARY: each 4-bar phrase gives a lane a character on top of your settings (as far as its VARY reaches):
 * clean, echo, slide, bite (brightness and drive), filter (a low-pass with resonance), stutter (retrigger) */
static const char *const VARYN[6] = {"CLEAN", "ECHO", "SLIDE", "BITE", "FILTER", "STUTTER"};
static int32_t vary_char[NLANE];
static int32_t clamp100(int32_t v) { return v < 0 ? 0 : v > 100 ? 100 : v; }
static int32_t fade_q8[NLANE] = {256, 256, 256, 256, 256};    /* ENV: each lane's fade, out or in over a bar */
static uint32_t fade_ms;
static void fx_apply(void)
{
    int32_t l, ph = cur_bar() / 4, dt = (int32_t)(fm1_ms - fade_ms), bar_ms = 240000 / (seq_bpm > 0 ? seq_bpm : 120);
    fade_ms = fm1_ms;
    if (dt > 100) dt = 100;
    for (l = 0; l < (int32_t)NLANE; l++) {
        const lanefx_t *U = &fxu[l];
        lanefx_t *F = &fx[l];
        int32_t v = U->vary, c = v ? (int32_t)((hash3(ph, l, 4242) >> 8) % 6u) : 0;
        int32_t filt = U->filt, res = U->res, wob = U->wob, bright = U->bright, drive = U->drive, slide = U->slide;
        int32_t retrig = U->retrig, rate = U->rtg_rate, echo = U->echo + (l == L_LEAD ? PROF[ln[L_LEAD].prof].echo : 0);   /* the style's echo channel */
        if (HITS(l) && (c == 2 || c == 3)) c = 1;                   /* no slide or brightness on hits: echo instead */
        switch (c) {
        case 1: echo += 60 * v / 100; break;
        case 2: slide += 50 * v / 100; break;
        case 3: bright += 40 * v / 100; drive += 45 * v / 100; break;
        case 4: if (filt == 0) filt = -(20 + 40 * v / 100); res += 40 * v / 100; if (!wob && v >= 50) wob = 3; break;
        case 5: retrig += 45 * v / 100; if (rate < 2) rate = 2; break;
        default: break;
        }
        F->filt = filt < -100 ? -100 : filt > 100 ? 100 : filt; F->res = clamp100(res); F->wob = wob;
        F->bright = clamp100(bright); F->drive = clamp100(drive); F->slide = clamp100(slide);
        F->retrig = clamp100(retrig); F->rtg_rate = rate; F->crush = U->crush; F->echo = clamp100(echo);
        {
            int32_t to = ln[l].mute || form_gone(l, cur_bar()) ? 0 : 256, st = dt * 256 / bar_ms + 1;   /* ENV, END */
            fade_q8[l] = fade_q8[l] < to ? (fade_q8[l] + st > to ? to : fade_q8[l] + st) : (fade_q8[l] - st < to ? to : fade_q8[l] - st);
        }
        F->level = U->level * fade_q8[l] >> 8; F->inst = U->inst; F->vary = v; F->send = U->send;
        vary_char[l] = c;
    }
}

/* ---- your patterns: GENERATE writes one that fits (empty lane) or varies the one there (15..30 %);
 * every result is remembered per lane, so the same pattern never comes back */
static uint32_t seen[NLANE][64], nseen[NLANE];
static rng_t ui_rng = {0x1234567u};
static uint32_t pat_hash(int32_t l)
{
    const upat_t *U = &ln[l].up;
    uint32_t h = 2166136261u, i, n = U->bars * 16u;
    for (i = 0; i < n; i++) { h = (h ^ (uint8_t)U->k[i]) * 16777619u; h = (h ^ U->len[i]) * 16777619u; h = (h ^ U->kinds[i]) * 16777619u; }
    return h ^ U->bars;
}
static int pat_seen(int32_t l, uint32_t h) { uint32_t i, n = nseen[l] < 64u ? nseen[l] : 64u; for (i = 0; i < n; i++) if (seen[l][i] == h) return 1; return 0; }
static void pat_remember(int32_t l, uint32_t h) { if (!pat_seen(l, h)) seen[l][nseen[l]++ % 64u] = h; }
static int pat_empty(int32_t l) { uint32_t i; for (i = 0; i < ln[l].up.bars * 16u; i++) if (ln[l].up.k[i] != EMPTY || ln[l].up.kinds[i]) return 0; return 1; }
static int32_t note_to_k(const tools_t *T, int32_t note)
{
    int32_t k, best = 0;
    for (k = -14; k <= 21; k++) { int32_t d = stp(T, k) - note, e = stp(T, best) - note; if ((d < 0 ? -d : d) < (e < 0 ? -e : e)) best = k; }
    return best;
}
static int32_t fill_style = -1;                                 /* EDIT's FILL: -1 = the lane's own style */
static void pat_fresh(int32_t l)
{
    upat_t *U = &ln[l].up;
    uint32_t seed = rnd(&ui_rng) ^ fm1_ms, i;
    int32_t bi, p = fill_style >= 0 ? fill_style : ln[l].prof;
    static gen_t G;
    for (i = 0; i < 32u; i++) { U->k[i] = EMPTY; U->len[i] = 1; U->kinds[i] = 0; }
    for (bi = 0; bi < U->bars; bi++) {
        harm_t H; tools_t T; rng_t r = {seed + (uint32_t)bi * 7919u};
        harm_at(bi, &H);
        tools_make(&T, &H, LBASE[l] + ln[l].oct * 12 + key_off);
        GL = &ln[l]; keys_for(GL, bi);
        if (l == L_SFX) { arc_t A = arc_at(bi); gen_sfx(&G, &r, &T, bi, p, ln[l].dens, &A, arc_at(bi + 1).drop); }
        else generate(PROF[p].st[l], &G, &r, &T, bi, PROF[p].st[l] == S_RIFF ? (int32_t)(seed & 1023u) + 4 : p);
        for (i = 0; i < G.n; i++) {
            uint32_t s = (uint32_t)bi * 16u + G.ev[i].st;
            if (HITS(l)) U->kinds[s] |= (uint8_t)(1u << G.ev[i].note[0]);
            else { U->k[s] = (int8_t)note_to_k(&T, G.ev[i].note[0]); U->len[s] = G.ev[i].len; }
        }
    }
}
static void pat_vary(int32_t l)
{
    upat_t *U = &ln[l].up;
    int32_t n = U->bars * 16, used = 0, i, c, changes;
    for (i = 0; i < n; i++) if (U->k[i] != EMPTY || U->kinds[i]) used++;
    changes = (used < 4 ? 4 : used) * (15 + (int32_t)((rnd(&ui_rng) >> 16) * 15u >> 16)) / 100;
    if (changes < 1) changes = 1;
    for (c = 0; c < changes; c++) {
        int32_t s = (int32_t)((rnd(&ui_rng) >> 16) * (uint32_t)n >> 16), op = (int32_t)((rnd(&ui_rng) >> 16) * 100u >> 16);
        if (HITS(l)) {
            static const int8_t K[6] = {DK_HAT, DK_HAT, DK_KICK, DK_SNARE, DK_SNARE, DK_KETTLE}, X[6] = {SX_ZAP, SX_ZAP, SX_BITS, SX_BITS, SX_CRACKLE, SX_IMPACT};
            int32_t k = (l == L_SFX ? X : K)[(rnd(&ui_rng) >> 16) * 6u >> 16];
            if (l == L_DRUMS && s % 16 == 0 && k == DK_KICK) continue;   /* keep the kick on the one */
            U->kinds[s] ^= (uint8_t)(1u << k);
            continue;
        }
        if (op < 45) {                                              /* move a note */
            for (i = 0; i < n && U->k[(s + i) % n] == EMPTY; i++) ;
            if (i < n) U->k[(s + i) % n] = (int8_t)(U->k[(s + i) % n] + PICK(&ui_rng, -2, -1, 1, 2));
        } else if (op < 65 && U->k[s] == EMPTY) {                   /* add one next to a note */
            int32_t d;
            for (d = 1; d < n; d++) {
                if (U->k[(s + n - d) % n] != EMPTY) { U->k[s] = (int8_t)(U->k[(s + n - d) % n] + PICK(&ui_rng, -1, 1)); U->len[s] = 1; break; }
                if (U->k[(s + d) % n] != EMPTY) { U->k[s] = (int8_t)(U->k[(s + d) % n] + PICK(&ui_rng, -1, 1)); U->len[s] = 1; break; }
            }
        } else if (op < 80 && used > 3 && s % 16 && U->k[s] != EMPTY) { U->k[s] = EMPTY; used--; }   /* drop one */
        else if (U->k[s] != EMPTY) {                                /* longer or shorter */
            int32_t v = U->len[s] + PICK(&ui_rng, -1, 1, 2);
            U->len[s] = (uint8_t)(v < 1 ? 1 : v > 8 ? 8 : v);
        }
    }
}
static int32_t pat_generate(int32_t l, int32_t fresh)           /* returns the number of patterns seen, 0 = none new */
{
    upat_t *U = &ln[l].up, keep = *U;
    int32_t tries, was_empty = pat_empty(l);
    pat_remember(l, pat_hash(l));
    for (tries = 0; tries < 40; tries++) {
        uint32_t h;
        *U = keep;
        if (fresh || was_empty || fill_style >= 0) pat_fresh(l); else pat_vary(l);
        h = pat_hash(l);
        if (!pat_seen(l, h) && !pat_empty(l)) { pat_remember(l, h); U->on = 1; invalidate_now(); return (int32_t)nseen[l]; }
    }
    *U = keep;
    return 0;
}
/* COPY: another lane's pattern into this one. Between melodic lanes the scale steps carry over (so a lead line
 * becomes a bass line in the same chords); notes into drums become kicks on the beat and hats between; drums into
 * notes become the root (kick), the fifth (snare) and the octave (the rest). */
static void pat_copy(int32_t from, int32_t to)
{
    upat_t S, *U = &ln[to].up;
    int32_t i;
    for (i = 0; i < 32; i++) { S.k[i] = ln[from].up.k[i]; S.len[i] = ln[from].up.len[i]; S.kinds[i] = ln[from].up.kinds[i]; }
    S.on = ln[from].up.on; S.bars = ln[from].up.bars;
    for (i = 0; i < 32; i++) { U->k[i] = S.k[i]; U->len[i] = S.len[i]; U->kinds[i] = S.kinds[i]; }
    U->bars = S.bars;
    if (HITS(from) != HITS(to))
        for (i = 0; i < 32; i++) {
            if (HITS(to)) {
                uint8_t k = 0;
                if (S.k[i] != EMPTY) k = (uint8_t)(to == L_DRUMS ? 1u << (i % 4 == 0 ? DK_KICK : DK_HAT) : 1u << (i % 4 == 0 ? SX_ZAP : SX_BITS));
                U->kinds[i] = k; U->k[i] = EMPTY; U->len[i] = 1;
            } else {
                uint8_t k = S.kinds[i];
                U->kinds[i] = 0; U->len[i] = 1;
                U->k[i] = !k ? EMPTY : from == L_DRUMS && (k & (1u << DK_KICK)) ? 0 : from == L_DRUMS && (k & (1u << DK_SNARE)) ? 4 : 7;
            }
        }
    else if (HITS(to) && from != to)                                /* drums and sfx: their own kinds */
        for (i = 0; i < 32; i++) U->kinds[i] = S.kinds[i] ? (uint8_t)(to == L_DRUMS ? 1u << (i % 4 == 0 ? DK_KICK : DK_HAT) : 1u << SX_BITS) : 0;
    U->on = 1;
    invalidate_now();
}
static void pat_grab(int32_t l)                                 /* what the lane plays in this bar becomes the pattern */
{
    upat_t *U = &ln[l].up;
    int32_t cb = cur_bar(), i;
    const bar_t *B = bar_get(cb);
    harm_t H; tools_t T;
    if (!B) return;
    harm_at(cb, &H);
    tools_make(&T, &H, LBASE[l] + ln[l].oct * 12 + key_off);
    for (i = 0; i < 32; i++) { U->k[i] = EMPTY; U->len[i] = 1; U->kinds[i] = 0; }
    for (i = 0; i < B->cnt[l]; i++) {
        const ev_t *e = &B->ev[l][i];
        if (HITS(l)) U->kinds[e->st] |= (uint8_t)(1u << e->note[0]);
        else { U->k[e->st] = (int8_t)note_to_k(&T, e->note[0]); U->len[e->st] = e->len; }
    }
    U->bars = 1; U->on = 1;
    invalidate_now();
}
