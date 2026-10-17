/* SPDX-License-Identifier: GPL-3.0-only */
/* Espada's music, in the main loop: what each lane plays, bar by bar.
 *
 *   harmony     a chord per bar: from the profile (explicit chords, borrowed ones allowed), or from the SONG
 *               page (key, scale, a suggested progression). Lines move through a "local scale": the scale
 *               with any note a semitone off a chord tone swapped for that chord tone, so nothing rubs.
 *   profiles    EPIC, HYPERSPACE, FUSION, OLDSKOOL: a progression and a style per lane
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
       S_STRINGS, S_SUS, S_LUSH, S_ARP,                  /* pad */
       S_DEPIC, S_DTECH, S_DFUSION, S_DOLD };            /* drums */
static const int8_t AEO[7] = {0, 2, 3, 5, 7, 8, 10}, ION[7] = {0, 2, 4, 5, 7, 9, 11}, DOR[7] = {0, 2, 3, 5, 7, 9, 10};
typedef struct {
    const char *name;
    const int8_t *scale;
    int8_t prog[8][2];          /* root above the tonic, quality */
    uint8_t st[NLANE];          /* style per lane (LEAD, PAD, BASS, DRUMS; the sfx lane has its own generator) */
    uint8_t inst[NLANE];        /* suggested patch per lane, kit for the drums (sfx: unused) */
} prof_t;
/* from a study of 90s tracker music: orchestral minor, techno minor, melodic major with a chromatic turn,
 * the 4-channel sound with arpeggio chords */
static const prof_t PROF[] = {
    {"EPIC", AEO, {{0, Q_MIN}, {8, Q_MAJ}, {10, Q_MAJ}, {0, Q_MIN}, {0, Q_MIN}, {8, Q_MAJ}, {5, Q_MAJ}, {0, Q_MAJ}},
     {S_EPIC, S_STRINGS, S_DRIVE, S_DEPIC}, {4, 5, 2, 1}},
    {"HYPERSPACE", AEO, {{0, Q_5}, {0, Q_5}, {10, Q_5}, {5, Q_5}, {7, Q_5}, {5, Q_5}, {3, Q_5}, {10, Q_5}},
     {S_STAB, S_SUS, S_DRIVE, S_DTECH}, {4, 7, 0, 2}},
    {"FUSION", ION, {{0, Q_MAJ}, {9, Q_MIN}, {0, Q_MAJ}, {9, Q_MIN}, {9, Q_MAJ}, {1, Q_MAJ}, {9, Q_MAJ}, {5, Q_MAJ}},
     {S_RIFF, S_LUSH, S_PEDAL, S_DFUSION}, {9, 5, 1, 0}},
    {"OLDSKOOL", AEO, {{5, Q_MIN}, {3, Q_MAJ}, {1, Q_MAJ}, {5, Q_MIN}, {0, Q_MIN}, {8, Q_MAJ}, {0, Q_MIN}, {3, Q_MAJ}},
     {S_MELODY, S_ARP, S_OCTAVE, S_DOLD}, {8, 9, 0, 1}},
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
typedef struct { uint8_t prof, lock, dens; int8_t oct; upat_t up; } lane_t;
static lane_t ln[NLANE];
static const int32_t LBASE[NLANE] = {62, 50, 38, 0, 62}, LLO[NLANE] = {58, 45, 24, 0, 0}, LHI[NLANE] = {98, 86, 64, 0, 0};
static const int32_t LORDER[NLANE] = {3, 2, 1, 0, 4};           /* blend order: drums first; sfx follows the bass */
static int32_t target = 1, blend = 0, launch_bar = -1;

static int32_t eff_prof(int32_t l, int32_t b)
{
    if (l == L_SFX) l = L_BASS;                                     /* its flavour: the profile with the harmony */
    if (ln[l].lock) return ln[l].prof;
    if (launch_bar >= 0 && b >= launch_bar) return target;
    if (LORDER[l] < blend) return target;
    return ln[l].prof;
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
        const int8_t *c = P->prog[fmodp(b, 8)];
        harm_make(H, P->scale, 7, c[0], QUAL[(int)c[1]], QUAL_N[(int)c[1]]);
    }
}

/* ---- the arc: energy (Q8, 256 = full) over bars */
enum { A_OFF, A_BUILD, A_BREAK, A_PEAK, A_DOWN };
typedef struct { int32_t mode, from, e0, until, lane; } seg_t;
typedef struct { int32_t mode, e, lane, until, drop; } arc_t;
static int32_t arc_on = 1, build_ph = 6, peak_ph = 4, auto_drop = 1;
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
    if (!arc_on) return a;
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
    case S_EPIC: {              /* long stepwise lines, chord tones on the strong beats */
        static const int8_t RH[4][5] = {{0, 4, 6, 8, -1}, {0, 8, 12, -1, -1}, {0, 6, 8, 14, -1}, {0, 4, 8, 12, -1}};
        const int8_t *rh = RH[(rnd(r) >> 16) * 4u >> 16];
        int32_t n = ct(T, PICK(r, 2, 3, 4));
        for (i = 0; i < 5 && rh[i] >= 0; i++) {
            if (i) { n = move(T, n, PICK(r, -1, -1, 1, 1, -2, 2)); if (rh[i] % 8 == 0) n = snap(T, n); }
            NOTE(G, rh[i], 1, n);
        }
        g_lens(G, 16);
        break; }
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
                NOTE(G, h + ms[i], (nx - ms[i]) > 2 ? 2 : nx - ms[i], stp(T, k));
            }
        break; }
    case S_MELODY: {            /* 8ths with 16th fills */
        static const int8_t POS[9] = {0, 2, 4, 6, 7, 8, 10, 12, 14};
        int32_t n = ct(T, PICK(r, 0, 1, 2)), first = 1;
        for (i = 0; i < 9; i++) {
            if (POS[i] % 4 != 0 && !chance(r, 70)) continue;
            if (!first) { n = move(T, n, PICK(r, -2, -1, -1, 1, 1, 2)); if (POS[i] % 4 == 0) n = snap(T, n); }
            first = 0;
            NOTE(G, POS[i], 1, n);
        }
        g_lens(G, 16);
        break; }
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
    int32_t only = fx[L_SFX].inst, root = ct(T, 0), x = dens * 2, i, s;
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
    if (x < 0) {
        for (i = 0, j = 0; i < G->n; i++) if (G->ev[i].st == 0 || chance(r, 100 + x)) G->ev[j++] = G->ev[i];
        G->n = j;
        return;
    }
    if (!x) return;
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
    {                                                                                /* bass and lead */
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
        rng_t r = {hash3(b, l, p)};
        tools_t T;
        uint32_t i;
        tools_make(&T, &H, LBASE[l] + ln[l].oct * 12 + key_off);
        B->prof[l] = (uint8_t)p; B->inst[l] = PROF[p].inst[l]; B->user[l] = ln[l].up.on; B->cnt[l] = 0;
        B->muted[l] = (uint8_t)!lane_active(&A, l);
        if (B->muted[l]) continue;
        if (ln[l].up.on) user_bar(l, &G, &T, b);
        else if (l == L_SFX) gen_sfx(&G, &r, &T, b, p, dens, &A, next_drop);
        else generate(PROF[p].st[l], &G, &r, &T, b, p);
        if (A.mode != A_OFF) { dens += (A.e - 154) * 80 / 256; if (dens < 0) dens = 0; if (dens > 100) dens = 100; }
        if (l != L_SFX) densify(l, &G, &r, &T, b, dens);
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
static void invalidate(void) { invalidate_from(cur_bar() + 1); }        /* changes land on the next bar */
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
        ln[l].prof = 0; ln[l].lock = 0; ln[l].dens = 50; ln[l].oct = 0;
        ln[l].up.on = 0; ln[l].up.bars = 1;
        for (i = 0; i < 32u; i++) { ln[l].up.k[i] = EMPTY; ln[l].up.len[i] = 1; ln[l].up.kinds[i] = 0; }
    }
}
static void commit_launch(void)
{
    int32_t l;
    for (l = 0; l < (int32_t)NLANE; l++) if (!ln[l].lock) ln[l].prof = (uint8_t)target;
    blend = 0; launch_bar = -1;
}

/* ---- DROP: tap = a break until the next phrase, then the drop; hold = a break while held, the drop on the
 * bar after release. The lane selected when DROP goes down plays alone in the break. */
static int32_t drop_down_ms, drop_seg = -1;
static void arc_restart(int32_t from) { segs[0].mode = A_BUILD; segs[0].from = from; segs[0].e0 = 0; segs[0].until = -1; nsegs = 1; }
static void drop_press(int32_t lane)
{
    int32_t nb = cur_bar() + 1, i, j;
    arc_t A = arc_at(cur_bar());
    if (!arc_on) return;
    for (i = 0, j = 0; i < nsegs; i++) if (segs[i].from < nb) segs[j++] = segs[i];
    nsegs = j;
    if (nsegs >= 16) nsegs = 15;
    segs[nsegs].mode = A_BREAK; segs[nsegs].from = nb; segs[nsegs].e0 = A.e; segs[nsegs].until = -1; segs[nsegs].lane = lane;
    drop_seg = nsegs++;
    drop_down_ms = (int32_t)fm1_ms;
    invalidate();
}
static void drop_release(void)
{
    seg_t *s;
    if (drop_seg < 0) return;
    s = &segs[drop_seg];
    if ((int32_t)fm1_ms - drop_down_ms < 350) s->until = (s->from + 1 + 3) / 4 * 4;          /* tap */
    else { int32_t nb = cur_bar() + 1; s->until = nb > s->from + 1 ? nb : s->from + 1; }      /* hold */
    drop_seg = -1;
    invalidate();
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
static void pat_fresh(int32_t l)
{
    upat_t *U = &ln[l].up;
    uint32_t seed = rnd(&ui_rng) ^ fm1_ms, i;
    int32_t bi, p = ln[l].prof;
    static gen_t G;
    for (i = 0; i < 32u; i++) { U->k[i] = EMPTY; U->len[i] = 1; U->kinds[i] = 0; }
    for (bi = 0; bi < U->bars; bi++) {
        harm_t H; tools_t T; rng_t r = {seed + (uint32_t)bi * 7919u};
        harm_at(bi, &H);
        tools_make(&T, &H, LBASE[l] + ln[l].oct * 12 + key_off);
        if (l == L_SFX) { arc_t A = arc_at(bi); gen_sfx(&G, &r, &T, bi, eff_prof(L_SFX, bi), ln[l].dens, &A, arc_at(bi + 1).drop); }
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
        if (fresh || was_empty) pat_fresh(l); else pat_vary(l);
        h = pat_hash(l);
        if (!pat_seen(l, h) && !pat_empty(l)) { pat_remember(l, h); U->on = 1; invalidate_now(); return (int32_t)nseen[l]; }
    }
    *U = keep;
    return 0;
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
