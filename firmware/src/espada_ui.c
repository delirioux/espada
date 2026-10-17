/* SPDX-License-Identifier: GPL-3.0-only */
/* Espada's screen and panel.
 *
 * The screen (240 x 240): a header (name, profile, arc phase, bar, tempo, the arc's energy along the
 * timeline), four lanes where notes scroll past a fixed "now" line, and a footer. The lanes are drawn
 * into a strip canvas each frame (two canvases: one is drawn while the other goes out over SPI).
 *
 * Pages (the encoders change with them; SELECT is the lane on every page but SONG):
 *   PLAY (HOME)  SELECT lane  ALG profile  K1 blend  K2 density  K3 octave  K4 instrument  PRESETS tempo
 *   FX (FX)      SELECT lane  ALG filter   K1 reso   K2 wobble   K3 bright  K4 drive       PRESETS level
 *   TRK (ARP)    SELECT lane  ALG slide    K1 retrig K2 rate     K3 crush   K4 echo        PRESETS level
 *   EDIT (EDIT)  SELECT lane  ALG step     K1 length K2 bars     K3 use     (keys enter notes)
 *   SONG (GLO)   SELECT key   ALG scale    K1 progression  K2 bars/chord  K3 build  K4 peak  PRESETS auto drop
 * Buttons: PLAY start/stop (SEL+PLAY: from the top), SEQ launch, REC drop (tap or hold; EDIT: grab,
 * SEL+REC: clear), ENV lock (EDIT: tie), LFO generate (SEL+LFO: fresh), OCT-/OCT+ octave, SAVE,
 * SEL held = encoders in steps of 10; SEL tapped = the encoder legend on / off. Keys: the key of the music
 * (EDIT: white keys = scale steps, black keys = rest; drums: K S H T C on the first five white keys). */

#define RGBC(h) RGB(((h) >> 16) & 255, ((h) >> 8) & 255, (h) & 255)
#define SW(c) ((uint16_t)(((c) >> 8) | ((c) << 8)))
static const uint16_t LCOL[NLANE] = {RGBC(0x5fe3e8), RGBC(0xb49bff), RGBC(0xffb04a), RGBC(0xf0e07a)};
static const char *const LNAME[NLANE] = {"LEAD", "PAD", "BASS", "DRUMS"};
#define C_BG RGBC(0x020404)
#define C_FOOT RGBC(0x0b1414)
#define C_CYAN RGBC(0x3fe0e0)
#define C_CREAM RGBC(0xefe6c8)
#define C_MUTE RGBC(0x8fa3a0)
#define C_LIGHT RGBC(0xc9d6d3)
#define C_DIMG RGBC(0x5d7370)
#define C_GRID RGBC(0x16302f)

#define NOWX 60
#define PPS 4
#define TOP 20
#define LH 43
#define GAP 2
#define FOOT_Y 200

/* ---- the strip canvas */
#define STRIP_H 46u
static uint16_t cvb[2][240u * STRIP_H] __attribute__((section(".pool")));
static uint16_t *cvp;
static int32_t cy0, chh;
static uint32_t cvn;
static void cs_begin(int32_t y0, int32_t h, uint16_t bg)
{
    uint32_t i, n = 240u * (uint32_t)h;
    uint16_t s = SW(bg);
    cvp = cvb[cvn++ & 1u]; cy0 = y0; chh = h;
    for (i = 0; i < n; i++) cvp[i] = s;
}
static void cs_end(void) { lcd_blit(0, (uint32_t)cy0, 240, (uint32_t)chh, cvp); }
static void fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t i, j, x1 = x + w, y1 = y + h;
    uint16_t s = SW(c);
    if (x < 0) x = 0;
    if (x1 > 240) x1 = 240;
    if (y < cy0) y = cy0;
    if (y1 > cy0 + chh) y1 = cy0 + chh;
    for (j = y; j < y1; j++) { uint16_t *p = cvp + (j - cy0) * 240; for (i = x; i < x1; i++) p[i] = s; }
}
static uint16_t cscale(uint16_t c, int32_t q8)                  /* a colour times q8/256 */
{
    int32_t r = (c >> 11) * q8 >> 8, g = (c >> 5 & 63) * q8 >> 8, b = (c & 31) * q8 >> 8;
    return (uint16_t)((r > 31 ? 31 : r) << 11 | (g > 63 ? 63 : g) << 5 | (b > 31 ? 31 : b));
}
static void darken(int32_t x, int32_t y, int32_t w, int32_t h, int32_t q8)
{
    int32_t i, j, x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (x1 > 240) x1 = 240;
    if (y < cy0) y = cy0;
    if (y1 > cy0 + chh) y1 = cy0 + chh;
    for (j = y; j < y1; j++) { uint16_t *p = cvp + (j - cy0) * 240; for (i = x; i < x1; i++) p[i] = SW(cscale(SW(p[i]), q8)); }
}
static void outline(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    fill(x, y, w, 1, c); fill(x, y + h - 1, w, 1, c); fill(x, y, 1, h, c); fill(x + w - 1, y, 1, h, c);
}
static void line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy, k;
    for (k = 0; k < 400; k++) {
        fill(x0, y0, 1, 1, c);
        if (x0 == x1 && y0 == y1) break;
        if (2 * e >= dy) { e += dy; x0 += sx; }
        if (2 * e <= dx) { e += dx; y0 += sy; }
    }
}
/* text in the platform's Terminus fonts (gfx.c): sc 1 = FONT_S (8 px a character, capitals 10 px high),
 * 2 = FONT_L (16 px a character, capitals 20 px; no small letters). y is the top of the capitals. */
static int32_t text(int32_t x, int32_t y, const char *s, uint16_t c, int32_t sc)
{
    const felucca_font_t *f = sc > 1 ? &FONT_L : &FONT_S;
    int32_t top = y - (sc > 1 ? 4 : 2);
    uint16_t sw = SW(c);
    for (; *s; s++) {
        uint32_t gi = glyph(f, (uint8_t)*s), w = f->bw[gi], bpr = (w + 1u) / 2u, gx, gy;
        const uint8_t *gd = f->data + f->off[gi];
        for (gy = 0; gy < f->h; gy++) {
            int32_t yy = top + (int32_t)gy;
            uint16_t *row;
            if (yy < cy0 || yy >= cy0 + chh) continue;
            row = cvp + (yy - cy0) * 240;
            for (gx = 0; gx < w; gx++) {
                uint32_t v = gd[gy * bpr + gx / 2u];
                int32_t xx = x - (int32_t)f->pad + (int32_t)gx;
                v = (gx & 1u) ? (v & 15u) : (v >> 4);
                if (v >= 8u && xx >= 0 && xx < 240) row[xx] = sw;
            }
        }
        x += f->adv[gi];
    }
    return x;
}
static int32_t tw(const char *s, int32_t sc) { int32_t n = 0; while (s[n]) n++; return n ? n * (sc > 1 ? 16 : 8) - (sc > 1 ? 4 : 2) : 0; }
static void textc(int32_t cx, int32_t y, const char *s, uint16_t c, int32_t sc) { text(cx - tw(s, sc) / 2, y, s, c, sc); }
static void textr(int32_t rx, int32_t y, const char *s, uint16_t c, int32_t sc) { text(rx - tw(s, sc), y, s, c, sc); }

/* ---- strings */
static char *s_cat(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *s_int(char *d, int32_t v)
{
    char t[12]; int32_t n = 0; uint32_t u = v < 0 ? (uint32_t)-v : (uint32_t)v;
    if (v < 0) *d++ = '-';
    do { t[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    while (n) *d++ = t[--n];
    *d = 0; return d;
}
static char *s_pct(char *d, int32_t v) { d = s_int(d, v); return s_cat(d, "%"); }
static char *s_sign(char *d, int32_t v) { if (v >= 0) *d++ = '+'; return s_int(d, v); }

/* ---- UI state */
enum { PG_PLAY, PG_FX, PG_TRK, PG_EDIT, PG_SONG, NPG };
static int32_t page = PG_PLAY, sel = L_BASS, legend_on = 1, debug_on;
static int32_t cursor, entry_len = 1, entry_oct;
static char toast_s[48];
static uint32_t toast_until;
static int32_t save_pending, gen_count[NLANE];
static void toast(const char *s) { uint32_t i; for (i = 0; i < 47u && s[i]; i++) toast_s[i] = s[i]; toast_s[i] = 0; toast_until = fm1_ms + 1600u; }
static const char *const NN[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const char *const WOBN[6] = {"OFF", "1/1", "1/2", "1/4", "1/8", "1/16"};
static const char *const DKN[NDK] = {"K", "S", "H", "T", "C"};
static void set_key(int32_t k) { key = fmodp(k, 12); key_off = key - 2 > 5 ? key - 14 : key - 2; invalidate(); }
static void set_bpm(int32_t b)
{
    if (b < 60) b = 60;
    if (b > 200) b = 200;
    seq_bpm = b;
    seq_step_len_q8 = 169344000u / (uint32_t)b;             /* 44100 * 60 / 4 * 256 / bpm */
}
static const char *inst_name(int32_t l, int32_t inst) { return l == L_DRUMS ? KITS[inst].name : PATCH[inst].name; }

/* ---- encoder functions per page */
typedef void (*turn_fn)(int32_t d);
typedef void (*val_fn)(char *o);
typedef struct { const char *name; turn_fn turn; val_fn val; } enc_t;
static int32_t clampi(int32_t v, int32_t a, int32_t b) { return v < a ? a : v > b ? b : v; }

static void t_lane(int32_t d) { sel = fmodp(sel + (d > 0 ? 1 : -1), NLANE); cursor = clampi(cursor, 0, ln[sel].up.bars * 16 - 1); }
static void v_lane(char *o) { s_cat(o, LNAME[sel]); }
static void t_prof(int32_t d) { target = fmodp(target + (d > 0 ? 1 : -1), NPROF); invalidate(); }
static void v_prof(char *o) { s_cat(o, PROF[target].name); }
static void t_blend(int32_t d) { blend = clampi(blend + (d > 0 ? 1 : -1), 0, 4); invalidate(); }
static void v_blend(char *o) { s_pct(o, blend * 25); }
static void t_dens(int32_t d) { ln[sel].dens = (uint8_t)clampi(ln[sel].dens + d, 0, 100); invalidate(); }
static void v_dens(char *o) { s_pct(o, ln[sel].dens); }
static void t_oct(int32_t d) { if (sel == L_DRUMS) return; ln[sel].oct = (int8_t)clampi(ln[sel].oct + (d > 0 ? 1 : -1), -1, 1); invalidate(); }
static void v_oct(char *o) { if (sel == L_DRUMS) s_cat(o, "--"); else s_sign(o, ln[sel].oct); }
static void t_inst(int32_t d)
{
    int32_t n = sel == L_DRUMS ? (int32_t)NKIT : (int32_t)NPATCH, v = fx[sel].inst + 1;   /* 0 = AUTO */
    v = fmodp(v + (d > 0 ? 1 : -1), n + 1);
    fx[sel].inst = v - 1;
}
static void v_inst(char *o)
{
    if (fx[sel].inst < 0) { o = s_cat(o, "A:"); s_cat(o, inst_name(sel, PROF[ln[sel].prof].inst[sel])); }
    else s_cat(o, inst_name(sel, fx[sel].inst));
}
static void t_tempo(int32_t d) { set_bpm(seq_bpm + d); }
static void v_tempo(char *o) { o = s_int(o, seq_bpm); s_cat(o, " BPM"); }
static void v_filt(char *o) { int32_t f = fx[sel].filt; if (!f) s_cat(o, "OFF"); else { o = s_cat(o, f < 0 ? "LP " : "HP "); s_pct(o, f < 0 ? -f : f); } }
static void t_filt(int32_t d) { fx[sel].filt = clampi(fx[sel].filt + d, -100, 100); }
static void t_res(int32_t d) { fx[sel].res = clampi(fx[sel].res + d, 0, 100); }
static void v_res(char *o) { s_pct(o, fx[sel].res); }
static void t_wob(int32_t d) { fx[sel].wob = clampi(fx[sel].wob + (d > 0 ? 1 : -1), 0, 5); }
static void v_wob(char *o) { s_cat(o, WOBN[fx[sel].wob]); }
static void t_bright(int32_t d) { if (sel != L_DRUMS) fx[sel].bright = clampi(fx[sel].bright + d, 0, 100); }
static void v_bright(char *o) { if (sel == L_DRUMS) s_cat(o, "--"); else s_pct(o, fx[sel].bright); }
static void t_drive(int32_t d) { fx[sel].drive = clampi(fx[sel].drive + d, 0, 100); }
static void v_drive(char *o) { s_pct(o, fx[sel].drive); }
static void t_level(int32_t d) { fx[sel].level = clampi(fx[sel].level + d, 0, 100); }
static void v_level(char *o) { s_pct(o, fx[sel].level); }
static void t_slide(int32_t d) { if (sel != L_DRUMS) fx[sel].slide = clampi(fx[sel].slide + d, 0, 100); }
static void v_slide(char *o) { if (sel == L_DRUMS) s_cat(o, "--"); else s_pct(o, fx[sel].slide); }
static void t_rtg(int32_t d) { fx[sel].retrig = clampi(fx[sel].retrig + d, 0, 100); }
static void v_rtg(char *o) { s_pct(o, fx[sel].retrig); }
static void t_rate(int32_t d) { fx[sel].rtg_rate = clampi(fx[sel].rtg_rate + (d > 0 ? 1 : -1), 1, 3); }
static void v_rate(char *o) { o = s_cat(o, "X"); s_int(o, fx[sel].rtg_rate); }
static void t_crush(int32_t d) { fx[sel].crush = clampi(fx[sel].crush + d, 0, 100); }
static void v_crush(char *o) { s_pct(o, fx[sel].crush); }
static void t_echo(int32_t d) { fx[sel].echo = clampi(fx[sel].echo + d, 0, 100); }
static void v_echo(char *o) { s_pct(o, fx[sel].echo); }
static void t_step(int32_t d) { int32_t n = ln[sel].up.bars * 16; cursor = fmodp(cursor + d, n); }
static void v_step(char *o) { o = s_int(o, cursor + 1); o = s_cat(o, "/"); s_int(o, ln[sel].up.bars * 16); }
static void t_len(int32_t d) { entry_len = clampi(entry_len + (d > 0 ? 1 : -1), 1, 16); }
static void v_len(char *o) { s_int(o, entry_len); }
static void t_bars(int32_t d) { ln[sel].up.bars = (uint8_t)(d > 0 ? 2 : 1); cursor = clampi(cursor, 0, ln[sel].up.bars * 16 - 1); invalidate_now(); }
static void v_bars(char *o) { s_int(o, ln[sel].up.bars); }
static void t_use(int32_t d) { ln[sel].up.on = (uint8_t)(d > 0); invalidate_now(); }
static void v_use(char *o) { s_cat(o, ln[sel].up.on ? "YOUR PATTERN" : "THE PROFILE"); }
static void t_key(int32_t d) { set_key(key + (d > 0 ? 1 : -1)); }
static void v_key(char *o) { s_cat(o, NN[key]); }
static void t_scale(int32_t d)
{
    int32_t was = scale_is_major(scale_sel);
    scale_sel = fmodp(scale_sel + (d > 0 ? 1 : -1), NSCALE);
    if (!scale_sel) prog_sel = 0;
    else if (!prog_sel || scale_is_major(scale_sel) != was) prog_sel = 1;   /* the first suggestion that fits */
    invalidate();
}
static void v_scale(char *o) { s_cat(o, scale_sel ? SCALES[scale_sel].name : "FROM PROFILE"); }
static void numeral(char *o, int32_t root, const int8_t *t)
{
    static const char *const NUM[12] = {"I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII"};
    const char *n = NUM[root % 12];
    int32_t minor = t[1] == 3;
    for (; *n; n++) *o++ = (char)(minor && (*n == 'I' || *n == 'V') ? *n + 32 : *n);
    if (t[1] == 7) *o++ = '5';
    else if (t[2] == 6) *o++ = 'o';
    else if (t[2] == 8) *o++ = '+';
    *o = 0;
}
static void prog_text(char *o, int32_t ps)
{
    const int8_t *P = prog_of(ps);
    int32_t i, root; int8_t t[3];
    for (i = 0; i < 4; i++) { chord_of(scale_sel, P[i], &root, t); numeral(o, root, t); while (*o) o++; if (i < 3) *o++ = ' '; }
    *o = 0;
}
static void t_prog(int32_t d)
{
    if (!scale_sel) scale_sel = 2;                          /* a progression needs a scale: start from minor */
    prog_sel = fmodp(prog_sel + (d > 0 ? 1 : -1), NPROG + 1);
    if (!prog_sel) scale_sel = 0;
    invalidate();
}
static void v_prog(char *o) { if (!prog_sel) s_cat(o, "FROM PROFILE"); else prog_text(o, prog_sel - 1); }
static void t_bpc(int32_t d) { bpc = clampi(bpc + (d > 0 ? 1 : -1), 1, 4); invalidate(); }
static void v_bpc(char *o) { s_int(o, bpc); }
static void t_build(int32_t d)
{
    int32_t was = arc_on, v = arc_on ? build_ph : 1;
    v = clampi(v + (d > 0 ? 1 : -1), 1, 16);
    if (v < 2) arc_on = 0; else { arc_on = 1; build_ph = v; if (!was) arc_restart(cur_bar() + 1); }
    invalidate();
}
static void v_build(char *o) { if (!arc_on) s_cat(o, "ARC OFF"); else { o = s_int(o, build_ph); s_cat(o, " PHRASES"); } }
static void t_peak(int32_t d) { peak_ph = clampi(peak_ph + (d > 0 ? 1 : -1), 1, 8); invalidate(); }
static void v_peak(char *o) { o = s_int(o, peak_ph); s_cat(o, " PHRASES"); }
static void t_auto(int32_t d) { auto_drop = d > 0; invalidate(); }
static void v_auto(char *o) { s_cat(o, auto_drop ? "ON" : "OFF"); }

static const enc_t ENC[NPG][7] = {
    {{"LANE", t_lane, v_lane}, {"PROFILE", t_prof, v_prof}, {"BLEND", t_blend, v_blend}, {"DENSITY", t_dens, v_dens},
     {"OCTAVE", t_oct, v_oct}, {"INSTRUMENT", t_inst, v_inst}, {"TEMPO", t_tempo, v_tempo}},
    {{"LANE", t_lane, v_lane}, {"FILTER", t_filt, v_filt}, {"RESO", t_res, v_res}, {"WOBBLE", t_wob, v_wob},
     {"BRIGHT", t_bright, v_bright}, {"DRIVE", t_drive, v_drive}, {"LEVEL", t_level, v_level}},
    {{"LANE", t_lane, v_lane}, {"SLIDE", t_slide, v_slide}, {"RETRIG", t_rtg, v_rtg}, {"RTG RATE", t_rate, v_rate},
     {"CRUSH", t_crush, v_crush}, {"ECHO", t_echo, v_echo}, {"LEVEL", t_level, v_level}},
    {{"LANE", t_lane, v_lane}, {"STEP", t_step, v_step}, {"LENGTH", t_len, v_len}, {"BARS", t_bars, v_bars},
     {"PATTERN", t_use, v_use}, {0, 0, 0}, {0, 0, 0}},
    {{"KEY", t_key, v_key}, {"SCALE", t_scale, v_scale}, {"CHORDS", t_prog, v_prog}, {"BARS/CHORD", t_bpc, v_bpc},
     {"BUILD", t_build, v_build}, {"PEAK", t_peak, v_peak}, {"AUTO DROP", t_auto, v_auto}},
};
static const char *const ENCN[7] = {"SEL", "ALG", "K1", "K2", "K3", "K4", "PRE"};
static const char *const PGN[NPG] = {"PLAY", "FX", "TRK", "EDIT", "SONG"};
static const uint16_t PGC[NPG] = {RGBC(0x3fe0e0), RGBC(0x3fe0e0), RGBC(0xefe6c8), RGBC(0xffb04a), RGBC(0xb49bff)};

/* ---- drawing: the lanes */
static int32_t pos16;                                       /* the play head, in steps Q16 */
static int32_t sx(int32_t step_q16) { return NOWX + (int32_t)(((int64_t)(int32_t)((uint32_t)step_q16 - (uint32_t)pos16) * PPS) >> 16); }
static int rt_on(int32_t l, const ev_t *e) { return fx[l].retrig > 0 && (int32_t)(e->rv * 100u >> 8) < fx[l].retrig; }
static void shape(int32_t x, int32_t y, int32_t w, int32_t h, int past, int ghost, int on, uint16_t col)
{
    if (on && seq_playing) fill(x, y - 1, w, h + 2, C_WHITE);
    else if (ghost && !past) outline(x, y, w, h, col);
    else fill(x, y, w, h, past ? cscale(col, 77) : col);
}
static int32_t note_y(int32_t l, int32_t y, int32_t n)
{
    int32_t yy = y + LH - 6 - (n - LLO[l]) * (LH - 16) / (LHI[l] - LLO[l]);
    if (fx[l].crush > 0) { int32_t q = 1 + fx[l].crush * 5 / 100; yy = (yy + q / 2) / q * q; }
    return yy;
}
static void draw_lane(int32_t l)
{
    int32_t y = TOP + l * (LH + GAP), r, b, b0, b1, prev_y = -1000, flash = (int32_t)(fm1_ms - lane_hit_ms[l]) < 110 && seq_playing;
    uint16_t col = LCOL[l];
    const bar_t *Bc = bar_get(cur_bar());
    cs_begin(y, LH + GAP, C_BG);
    for (r = 0; r < LH; r++) {                                      /* the copper bar */
        int32_t a = (l == sel ? 51 : 26) + flash * 56, tri = 256 - (r > 22 ? r - 22 : 22 - r) * 256 / 22;
        fill(0, y + r, 240, 1, cscale(col, a * tri >> 8));
    }
    if (l == sel) outline(0, y, 240, LH, cscale(col, 180));
    b0 = (pos16 >> 16) / 16 - 1; if (b0 < 0) b0 = 0;
    b1 = ((pos16 >> 16) + (240 - NOWX) / PPS) / 16 + 1;
    for (b = b0; b <= b1; b++) {                                    /* bar and phrase lines */
        int32_t x = sx(b * 16 << 16);
        if (b == launch_bar) { int32_t k; for (k = 0; k < LH; k += 4) fill(x, y + k, 1, 2, C_WHITE); }
        else fill(x, y, 1, LH, cscale(RGBC(0x9fb3b0), b % 4 == 0 ? 82 : 31));
    }
    for (b = b0; b <= b1; b++) {
        const bar_t *B = bar_get(b);
        uint32_t i;
        int ghost;
        if (!B) continue;
        ghost = B->prof[l] != ln[l].prof && !B->user[l];
        for (i = 0; i < B->cnt[l]; i++) {
            const ev_t *e = &B->ev[l][i];
            int32_t x = sx((b * 16 + e->st) << 16), rt = rt_on(l, e), k;
            if (x > 240 || x < -64) continue;
            if (l == L_DRUMS) {
                int32_t kind = e->note[0], row = kind == DK_SNARE ? 1 : kind == DK_HAT || kind == DK_CRASH ? 2 : 0, yy = y + LH - 10 - row * 10;
                int32_t w = kind == DK_KETTLE || kind == DK_CRASH ? 6 : 3;
                if (fx[l].echo > 0) for (k = 1; k <= 2; k++) outline(x + k * 3 * PPS, yy, w, 5, cscale(col, k == 1 ? fx[l].echo * 140 / 100 : fx[l].echo * fx[l].echo * 90 / 10000));
                if (rt) for (k = 0; k < fx[l].rtg_rate; k++) { int32_t sw = PPS / fx[l].rtg_rate; shape(x + k * sw, yy, sw > 1 ? sw - 1 : 1, 5, x + k * sw < NOWX, ghost, 0, col); }
                else shape(x, yy, w, 5, x + w < NOWX, ghost, x <= NOWX && x + w + 3 > NOWX, col);
            } else {
                int32_t w = e->len * PPS - 1, nn;
                if (w < 2) w = 2;
                for (nn = 0; nn < e->n; nn++) {
                    int32_t yy = note_y(l, y, e->note[nn]), on = x <= NOWX && x + w > NOWX;
                    if (fx[l].echo > 0) for (k = 1; k <= 2; k++) outline(x + k * 3 * PPS, yy, w < 3 * PPS ? w : 3 * PPS, 3, cscale(col, k == 1 ? fx[l].echo * 140 / 100 : fx[l].echo * fx[l].echo * 90 / 10000));
                    if (e->flags & EV_ARP) { for (k = 0; k < w; k += 3) shape(x + k, yy, 2, 3, x + k + 2 < NOWX, ghost, on, col); }
                    else if (rt) { int32_t sw = PPS / fx[l].rtg_rate; if (sw < 1) sw = 1; for (k = 0; k < w; k += sw) shape(x + k, yy, sw > 1 ? sw - 1 : 1, 3, x + k < NOWX, ghost, on, col); }
                    else shape(x, yy, w, 3, x + w < NOWX, ghost, on, col);
                    if (nn == 0 && !(e->flags & EV_ARP)) {
                        if (fx[l].slide > 0 && prev_y > -1000 && (prev_y - yy > 1 || yy - prev_y > 1)) {
                            int32_t gl = fx[l].slide * 132 * PPS / (int32_t)(seq_step_len_q8 >> 8);   /* up to 0.3 s */
                            if (gl > w) gl = w;
                            line(x, prev_y + 1, x + gl, yy + 1, x < NOWX ? cscale(col, 77) : col);
                        }
                        prev_y = yy;
                    }
                }
            }
        }
    }
    if (fx[l].filt != 0 || fx[l].wob > 0) {                         /* the filter: shade what it removes */
        int32_t f = fx[l].filt, lp = f <= 0, note16 = f < 0 ? 130 * 16 + f * 95 * 16 / 100 : f > 0 ? 16 * 16 + f * 88 * 16 / 100 : 82 * 16, yc, lo, hi;
        if (fx[l].wob > 0 && seq_playing) note16 += (SIN[wob_ph[l] >> 22] * (24 * 16)) >> 15;
        lo = l == L_DRUMS ? 30 : LLO[l]; hi = l == L_DRUMS ? 120 : LHI[l];
        yc = y + LH - 6 - ((note16 >> 4) - lo) * (LH - 16) / (hi - lo);
        if (yc < y) yc = y;
        if (yc > y + LH) yc = y + LH;
        if (lp) darken(0, y, 240, yc - y, 97); else darken(0, yc, 240, y + LH - yc, 97);
        fill(0, yc - fx[l].res / 50, 240, 1 + fx[l].res / 50, cscale(col, 115 + fx[l].res * 140 / 100));
        { int32_t ty = clampi(lp ? yc + 3 : yc - 13, y + 2, y + LH - 12); fill(213, ty - 2, 20, 14, C_BG); text(215, ty, lp ? "LP" : "HP", col, 1); }
    }
    if (Bc && Bc->muted[l]) darken(0, y, NOWX, LH, 140);
    fill(1, y + 1, tw(LNAME[l], 1) + 5, 14, C_BG);                 /* the label column: name */
    text(3, y + 3, LNAME[l], col, 1);
    {                                                               /* profile (or USR) and FX */
        char t[4] = {PROF[ln[l].prof].name[0], PROF[ln[l].prof].name[1], PROF[ln[l].prof].name[2], 0};
        fill(1, y + 15, 27, 14, C_BG);
        text(3, y + 17, ln[l].up.on ? "USR" : t, ln[l].up.on ? C_CREAM : C_MUTE, 1);
        if (fx[l].filt || fx[l].wob || fx[l].drive || fx[l].bright != 25 || fx[l].slide || fx[l].retrig || fx[l].crush || fx[l].echo) { fill(30, y + 15, 19, 14, col); text(32, y + 17, "FX", C_BG, 1); }
    }
    if (ln[l].lock) { fill(1, y + 29, 27, 13, C_CREAM); text(3, y + 31, "LCK", C_BG, 1); }
    if (!ln[l].lock && LORDER[l] < blend && target != ln[l].prof) fill(236, y + 2, 2, LH - 4, C_WHITE);
    fill(NOWX, y, 1, LH + GAP, cscale(C_WHITE, 218));               /* now */
    if (l == 0) { fill(NOWX - 3, y, 7, 1, C_WHITE); fill(NOWX - 2, y + 1, 5, 1, C_WHITE); fill(NOWX - 1, y + 2, 3, 1, C_WHITE); }
}

/* ---- drawing: the EDIT page (a step grid) and the SONG page, in strips over the lane area */
static void draw_edit_area(void)
{
    const upat_t *U = &ln[sel].up;
    int32_t n = U->bars * 16, x0 = 16, cwq = (220 << 8) / n, top = TOP + 2, h = 4 * (LH + GAP) - 4;
    int32_t rows = sel == L_DRUMS ? NDK : 18, rh = h / rows, i, s;
    uint16_t col = LCOL[sel];
#define CX(s) (x0 + (((s) * cwq) >> 8))
    for (i = 0; i < rows; i++) {
        int32_t yy = top + i * rh, k = 14 - i - 3, strong = sel == L_DRUMS || fmodp(k, 7) == 0;
        char t[3] = {0, 0, 0};
        fill(x0, yy, 236 - x0, rh - 1, strong ? C_GRID : RGBC(0x0a1414));
        if (sel == L_DRUMS) t[0] = DKN[i][0]; else t[0] = (char)('1' + fmodp(k, 7));
        if (strong) textc(8, yy + (rh - 10) / 2, t, col, 1);
    }
    for (s = 0; s <= n; s++) fill(CX(s), top, 1, h, s % 16 == 0 ? RGBC(0x9fb3b0) : s % 4 == 0 ? RGBC(0x3b5150) : RGBC(0x1b2928));
    fill(CX(cursor), top, CX(cursor + 1) - CX(cursor), h, RGBC(0x2a3a3a));
    outline(CX(cursor), top, CX(cursor + (sel == L_DRUMS ? 1 : entry_len)) - CX(cursor) + 1, h, C_WHITE);
    for (s = 0; s < n; s++) {
        if (sel == L_DRUMS) { for (i = 0; i < NDK; i++) if (U->kinds[s] >> i & 1u) fill(CX(s) + 1, top + i * rh + 2, CX(s + 1) - CX(s) - 2, rh - 5, col); }
        else if (U->k[s] != EMPTY) {
            i = 14 - U->k[s] - 3;
            if (i >= 0 && i < rows) fill(CX(s) + 1, top + i * rh + 2, CX(s + (U->len[s] > n - s ? n - s : U->len[s])) - CX(s) - 2, rh - 4, col);
        }
    }
    if (seq_playing && U->on) fill(CX((pos16 >> 16) % n) + (((pos16 & 0xFFFF) * (cwq >> 8)) >> 16), top, 1, h, C_CYAN);
#undef CX
}
static void draw_song_area(void)
{
    char t[48];
    int32_t auto_ = prog_sel == 0, i, n, cur, per, bw;
    int32_t roots[8]; int8_t tones[8][3];
    text(6, 26, NN[key], C_CYAN, 2);
    t[0] = 0;
    if (auto_) { char *o = s_cat(t, "PROFILE "); s_cat(o, PROF[ln[L_BASS].prof].name); } else s_cat(t, SCALES[scale_sel].name);
    text(48, 26, t, C_CREAM, 1);
    t[0] = 0;
    if (auto_) s_cat(t, "ITS OWN CHORDS"); else { char *o = s_int(t, bpc); s_cat(o, bpc == 1 ? " BAR A CHORD" : " BARS A CHORD"); }
    text(48, 40, t, C_MUTE, 1);
    {                                                       /* the suggestions, the chosen one in the middle */
        int32_t j;
        for (j = -1; j <= 1; j++) {
            int32_t k = prog_sel + j, yy = 58 + (j + 1) * 16;
            if (k < 0 || k > (auto_ ? 0 : NPROG)) continue;
            if (k == 0) s_cat(t, "FROM THE PROFILE"); else prog_text(t, k - 1);
            if (!j) { fill(4, yy, 232, 15, C_GRID); fill(4, yy, 3, 15, C_CYAN); }
            text(12, yy + 3, t, j ? C_DIMG : C_WHITE, 1);
        }
    }
    if (auto_) { const prof_t *P = &PROF[ln[L_BASS].prof]; n = 8; for (i = 0; i < 8; i++) { int32_t q = P->prog[i][1], k; roots[i] = P->prog[i][0]; for (k = 0; k < 3; k++) tones[i][k] = k < QUAL_N[q] ? QUAL[q][k] : 0; if (QUAL_N[q] == 2) tones[i][2] = 7, tones[i][1] = 7; } }
    else { const int8_t *P = prog_of(prog_sel - 1); n = 4; for (i = 0; i < 4; i++) chord_of(scale_sel, P[i], &roots[i], tones[i]); }
    cur = auto_ ? fmodp(cur_bar(), n) : (cur_bar() / bpc) % n;
    per = n > 4 ? 4 : n; bw = (232 - (per - 1) * 4) / per;
    for (i = 0; i < n; i++) {
        int32_t x = 4 + (i % per) * (bw + 4), yy = 110 + (i / per) * 34, on = seq_playing && i == cur;
        char *o = t;
        o = s_cat(o, NN[(key + roots[i]) % 12]);
        s_cat(o, tones[i][1] == 7 ? "5" : tones[i][2] == 6 ? "dim" : tones[i][2] == 8 ? "+" : tones[i][1] == 3 ? "m" : "");
        fill(x, yy, bw, 31, on ? C_CYAN : RGBC(0x1b2928));
        textc(x + bw / 2, yy + 4, t, on ? C_BG : C_CREAM, 1);
        numeral(t, roots[i], tones[i]);
        textc(x + bw / 2, yy + 17, t, on ? C_BG : C_MUTE, 1);
    }
}
static void draw_legend(void)
{
    int32_t i, x0 = 4, y0 = 24, w = 232, h = 172;
    fill(x0, y0, w, h, RGBC(0x0b1414));
    outline(x0, y0, w, h, PGC[page]);
    text(x0 + 8, y0 + 7, PGN[page], PGC[page], 2);
    textr(x0 + w - 8, y0 + 13, "SEL HIDES", C_DIMG, 1);
    for (i = 0; i < 7; i++) {
        char v[40];
        int32_t ry = y0 + 38 + i * 19, vx;
        if (!ENC[page][i].name) continue;
        v[0] = 0; ENC[page][i].val(v);
        vx = x0 + w - 8 - tw(v, 1);
        text(x0 + 8, ry, ENCN[i], C_MUTE, 1);
        if (x0 + 40 + tw(ENC[page][i].name, 1) < vx - 6) text(x0 + 40, ry, ENC[page][i].name, C_WHITE, 1);
        text(vx, ry, v, C_CREAM, 1);
    }
}

/* ---- header and footer */
static const char *const MODEN[5] = {"", "BUILD ", "BREAK ", "PEAK ", "DOWN "};
static const uint16_t MODEC[5] = {RGBC(0x8fa3a0), RGBC(0x3fe0e0), C_WHITE, RGBC(0xffb04a), RGBC(0xb49bff)};
static void draw_header(void)
{
    char t[40], *o;
    int32_t cb = cur_bar(), x;
    arc_t A = arc_at(cb);
    int32_t cw = tw(PGN[page], 1) + 6, xr;
    cs_begin(0, TOP, C_BG);
    fill(0, 0, cw, 15, PGC[page]);
    text(3, 3, PGN[page], C_BG, 1);
    o = s_cat(t, MODEN[A.mode]); o = s_int(o, cb / 4 + 1); o = s_cat(o, "."); o = s_int(o, cb % 4 + 1); o = s_cat(o, " "); s_int(o, seq_bpm);
    xr = 238 - tw(t, 1);
    text(xr, 3, t, MODEC[A.mode], 1);
    if (!debug_on && cw + 6 + tw(PROF[ln[L_BASS].prof].name, 1) < xr - 6) text(cw + 6, 3, PROF[ln[L_BASS].prof].name, C_CREAM, 1);
    if (arc_on && page != PG_EDIT && page != PG_SONG) {
        int32_t lastb = -99999; arc_t M = A;
        for (x = 0; x < 240; x += 2) {
            int32_t bb = ((pos16 >> 16) + (x - NOWX) / PPS) / 16, hgt;
            if ((pos16 >> 16) + (x - NOWX) / PPS < 0) continue;
            if (bb != lastb) { M = arc_at(bb); lastb = bb; }
            hgt = 1 + M.e * 3 / 256;
            fill(x, 20 - hgt, 2, hgt, x < NOWX ? cscale(MODEC[M.mode], 90) : MODEC[M.mode]);
        }
    }
    if (debug_on) {
        o = s_cat(t, "ISR "); o = s_int(o, (int32_t)isr_us_max); s_cat(o, "US");
        text(cw + 6, 3, t, C_WHITE, 1);
    }
    cs_end();
}
static uint32_t foot_hash;
static void draw_footer(int force)
{
    char a[56], b[56], *o;
    uint16_t ca = C_LIGHT, cb_ = C_LIGHT;
    int32_t cb = cur_bar(), boxes = page == PG_PLAY;
    arc_t A = arc_at(cb);
    uint32_t h = 2166136261u, i;
    a[0] = b[0] = 0;
    switch (page) {
    case PG_PLAY: o = s_cat(a, "NEXT "); s_cat(o, PROF[target].name); ca = target == ln[L_BASS].prof && !blend ? C_MUTE : C_CYAN; break;
    case PG_FX: o = s_cat(a, LNAME[sel]); o = s_cat(o, "  "); v_filt(o); while (*o) o++; o = s_cat(o, "  RES "); s_pct(o, fx[sel].res);
        o = s_cat(b, "WOB "); o = s_cat(o, WOBN[fx[sel].wob]); o = s_cat(o, "  BRT "); v_bright(o); while (*o) o++; o = s_cat(o, "  DRV "); s_pct(o, fx[sel].drive); break;
    case PG_TRK: o = s_cat(a, LNAME[sel]); o = s_cat(o, "  SLIDE "); v_slide(o); while (*o) o++; o = s_cat(o, "  RTG "); o = s_pct(o, fx[sel].retrig); o = s_cat(o, " X"); s_int(o, fx[sel].rtg_rate);
        o = s_cat(b, "CRUSH "); o = s_pct(o, fx[sel].crush); o = s_cat(o, "  ECHO "); s_pct(o, fx[sel].echo); break;
    case PG_EDIT: o = s_cat(a, LNAME[sel]); o = s_cat(o, "  STEP "); v_step(o); while (*o) o++;
        if (sel != L_DRUMS) { o = s_cat(o, "  LEN "); o = s_int(o, entry_len); o = s_cat(o, " O"); s_sign(o, entry_oct); }
        o = s_cat(b, ln[sel].up.on ? "YOUR PATTERN" : "PROFILE PLAYS"); o = s_cat(o, "  BARS "); s_int(o, ln[sel].up.bars); cb_ = ln[sel].up.on ? C_CREAM : C_MUTE; break;
    default: o = s_cat(a, "KEY "); o = s_cat(o, NN[key]); o = s_cat(o, "  "); v_scale(o); while (*o) o++;
        if (prog_sel) { o = s_cat(o, "  "); o = s_int(o, bpc); s_cat(o, " B/CH"); }
        o = s_cat(b, "ARC "); if (!arc_on) s_cat(o, "OFF"); else { o = s_cat(o, "BUILD "); o = s_int(o, build_ph); o = s_cat(o, " PEAK "); o = s_int(o, peak_ph); s_cat(o, auto_drop ? " AUTO" : ""); }
        cb_ = arc_on ? C_CYAN : C_MUTE; break;
    }
    if (page == PG_PLAY) {
        if (launch_bar >= 0) {
            int32_t left = launch_bar - cb; o = s_cat(b, "LAUNCH "); o = s_cat(o, PROF[target].name); o = s_cat(o, " IN "); o = s_int(o, left < 0 ? 0 : left); s_cat(o, left == 1 ? " BAR" : " BARS");
            cb_ = (pos16 >> 17) % 2 ? C_WHITE : C_CYAN;
        } else if (A.mode == A_BREAK || (arc_on && arc_at(cb + 1).mode == A_BREAK)) {
            arc_t N = A.mode == A_BREAK ? A : arc_at(cb + 1);
            if (N.until >= 0) { int32_t left = N.until - cb; o = s_cat(b, A.mode == A_BREAK ? "BREAK - DROP IN " : "BREAK NEXT - DROP IN "); o = s_int(o, left); s_cat(o, left == 1 ? " BAR" : " BARS"); }
            else s_cat(b, "BREAK - LET GO OF REC TO DROP");
            cb_ = (pos16 >> 17) % 2 ? C_WHITE : RGBC(0xffb04a);
        } else {
            o = s_cat(b, LNAME[sel]); o = s_cat(o, " "); v_inst(o); while (*o) o++; o = s_cat(o, " D"); o = s_pct(o, ln[sel].dens);
            if (sel != L_DRUMS) { o = s_cat(o, " O"); o = s_sign(o, ln[sel].oct); }
            if (ln[sel].lock) s_cat(o, " LCK");
            cb_ = C_LIGHT;
        }
    }
    if (fm1_ms < toast_until) { s_cat(b, ""); { char *d = b; const char *s = toast_s; while ((*d++ = *s++)) ; } cb_ = C_WHITE; }
    for (i = 0; a[i]; i++) h = (h ^ (uint8_t)a[i]) * 16777619u;
    for (i = 0; b[i]; i++) h = (h ^ (uint8_t)b[i]) * 16777619u;
    h ^= (uint32_t)blend * 31u + (uint32_t)page * 977u + ca + cb_ * 7u;
    if (!force && h == foot_hash) return;
    foot_hash = h;
    cs_begin(FOOT_Y, 240 - FOOT_Y, C_FOOT);
    text(4, FOOT_Y + 6, a, ca, 1);
    if (boxes) {                                                    /* the blend: drums, bass, pad, lead */
        static const char *const BX[4] = {"D", "B", "P", "L"};
        for (i = 0; i < 4u; i++) { int32_t x = 150 + (int32_t)i * 22, on = (int32_t)i < blend; fill(x, FOOT_Y + 3, 20, 15, on ? C_CYAN : RGBC(0x1d2b2b)); textc(x + 10, FOOT_Y + 6, BX[i], on ? C_BG : C_MUTE, 1); }
    }
    text(4, FOOT_Y + 24, b, cb_, 1);
    cs_end();
}

/* ---- the frame */
static uint32_t frames, fps_t0, fps;
static int32_t last_drawn_pos = -1, area_dirty = 1, legend_was = -1;
static void draw_frame(int force)
{
    int32_t l, legend = legend_on;
    pos16 = cur_step() * 65536 + (seq_playing ? (int32_t)vis_frac : 0);
    if (legend != legend_was) { legend_was = legend; force = 1; }
    if (!force && pos16 == last_drawn_pos && !area_dirty && !seq_playing) { draw_footer(0); return; }
    last_drawn_pos = pos16;
    draw_header();
    platform_poll();
    if (page == PG_EDIT || page == PG_SONG) {
        for (l = 0; l < 4; l++) {
            cs_begin(TOP + l * (LH + GAP), LH + GAP, C_BG);
            if (page == PG_EDIT) draw_edit_area(); else draw_song_area();
            if (legend) draw_legend();
            cs_end();
            platform_poll();
        }
    } else {
        for (l = 0; l < 4; l++) {
            draw_lane(l);
            if (legend) draw_legend();
            cs_end();
            platform_poll();
        }
    }
    area_dirty = 0;
    draw_footer(force);
    frames++;
    if (fm1_ms - fps_t0 >= 1000u) { fps = frames; frames = 0; fps_t0 = fm1_ms; }
}

/* ---- saving: settings (256 B) and patterns (their own sector), only while stopped (an erase stalls the sound) */
typedef struct {
    uint8_t ver, key, scale, prog, bpc, arc, build, peak, autod, target, bpm, pad;
    struct { uint8_t prof, lock, dens; int8_t oct, filt; uint8_t res, wob, bright, drive, slide, retrig, rate, crush, echo, level; int8_t inst; } l[NLANE];
} save_t;
typedef struct { upat_t p[NLANE]; } pats_t;
static void do_save(void)
{
    static save_t S;
    static pats_t P;
    int32_t l, rc;
    S.ver = 1; S.key = (uint8_t)key; S.scale = (uint8_t)scale_sel; S.prog = (uint8_t)prog_sel; S.bpc = (uint8_t)bpc; S.arc = (uint8_t)arc_on;
    S.build = (uint8_t)build_ph; S.peak = (uint8_t)peak_ph; S.autod = (uint8_t)auto_drop; S.target = (uint8_t)target; S.bpm = (uint8_t)seq_bpm;
    for (l = 0; l < (int32_t)NLANE; l++) {
        S.l[l].prof = ln[l].prof; S.l[l].lock = ln[l].lock; S.l[l].dens = ln[l].dens; S.l[l].oct = ln[l].oct; S.l[l].filt = (int8_t)fx[l].filt;
        S.l[l].res = (uint8_t)fx[l].res; S.l[l].wob = (uint8_t)fx[l].wob; S.l[l].bright = (uint8_t)fx[l].bright; S.l[l].drive = (uint8_t)fx[l].drive;
        S.l[l].slide = (uint8_t)fx[l].slide; S.l[l].retrig = (uint8_t)fx[l].retrig; S.l[l].rate = (uint8_t)fx[l].rtg_rate; S.l[l].crush = (uint8_t)fx[l].crush;
        S.l[l].echo = (uint8_t)fx[l].echo; S.l[l].level = (uint8_t)fx[l].level; S.l[l].inst = (int8_t)fx[l].inst;
        P.p[l] = ln[l].up;
    }
    rc = settings_save(&S, sizeof S);
    if (!rc) rc = patterns_save(&P, sizeof P);
    toast(rc ? "SAVE FAILED" : "SAVED");
}
static void do_load(void)
{
    static save_t S;
    static pats_t P;
    int32_t l;
    if (settings_load(&S, sizeof S) == 0 && S.ver == 1) {
        set_key(S.key % 12u); scale_sel = S.scale < NSCALE ? S.scale : 0; prog_sel = S.prog <= NPROG ? S.prog : 0; bpc = clampi(S.bpc, 1, 4);
        arc_on = S.arc != 0; build_ph = clampi(S.build, 2, 16); peak_ph = clampi(S.peak, 1, 8); auto_drop = S.autod != 0; target = S.target % NPROF; set_bpm(S.bpm);
        for (l = 0; l < (int32_t)NLANE; l++) {
            ln[l].prof = (uint8_t)(S.l[l].prof % NPROF); ln[l].lock = S.l[l].lock != 0; ln[l].dens = (uint8_t)clampi(S.l[l].dens, 0, 100); ln[l].oct = (int8_t)clampi(S.l[l].oct, -1, 1);
            fx[l].filt = clampi(S.l[l].filt, -100, 100); fx[l].res = clampi(S.l[l].res, 0, 100); fx[l].wob = clampi(S.l[l].wob, 0, 5); fx[l].bright = clampi(S.l[l].bright, 0, 100);
            fx[l].drive = clampi(S.l[l].drive, 0, 100); fx[l].slide = clampi(S.l[l].slide, 0, 100); fx[l].retrig = clampi(S.l[l].retrig, 0, 100); fx[l].rtg_rate = clampi(S.l[l].rate, 1, 3);
            fx[l].crush = clampi(S.l[l].crush, 0, 100); fx[l].echo = clampi(S.l[l].echo, 0, 100); fx[l].level = clampi(S.l[l].level, 0, 100);
            fx[l].inst = clampi(S.l[l].inst, -1, l == L_DRUMS ? (int32_t)NKIT - 1 : (int32_t)NPATCH - 1);
        }
    }
    if (patterns_load(&P, sizeof P) == 0)
        for (l = 0; l < (int32_t)NLANE; l++) { ln[l].up = P.p[l]; if (ln[l].up.bars < 1 || ln[l].up.bars > 2) ln[l].up.bars = 1; }
}

/* ---- input */
static int is_black(int32_t k) { return (0x54Au >> ((53 + k) % 12)) & 1u; }
static int32_t white_index(int32_t k) { int32_t i, w = 0; for (i = 0; i < k; i++) if (!is_black(i)) w++; return is_black(k) ? -1 : w; }
static void start_stop(int from_top)
{
    if (seq_playing && !from_top) { frozen_step = (int32_t)vis_step; seq_stop_req = 1; return; }
    if (from_top) { if (seq_playing) { seq_stop_req = 1; while (seq_playing) platform_poll(); } frozen_step = 0; launch_bar = -1; arc_restart(0); invalidate_from(0); }
    while (bars_update()) ;                                         /* the first bars exist before the clock starts */
    seq_start_step = frozen_step;
    seq_start_req = 1;
}
static void set_page(int32_t p) { page = page == p && p != PG_PLAY ? PG_PLAY : p; area_dirty = 1; }
static void edit_key(int32_t k, int32_t sel_held_now)
{
    upat_t *U = &ln[sel].up;
    int32_t n = U->bars * 16, wi = white_index(k);
    if (sel == L_DRUMS) {
        if (wi >= 0 && wi < NDK) { U->kinds[cursor] ^= (uint8_t)(1u << wi); if (!seq_playing) audition(L_DRUMS, (uint32_t)wi, fx[L_DRUMS].inst >= 0 ? (uint32_t)fx[L_DRUMS].inst : PROF[ln[L_DRUMS].prof].inst[L_DRUMS]); }
        else cursor = fmodp(cursor + 1, n);
        U->on = 1; invalidate_now(); return;
    }
    if (wi < 0) {                                                   /* black keys: rest, SEL + black: tie */
        if (sel_held_now) { int32_t s; for (s = cursor - 1; s >= 0; s--) if (U->k[s] != EMPTY) { U->len[s] = (uint8_t)clampi(cursor - s + 1, 1, 16); break; } }
        else U->k[cursor] = EMPTY;
        cursor = fmodp(cursor + 1, n); invalidate_now(); return;
    }
    {
        int32_t s;
        U->k[cursor] = (int8_t)(wi - 4 + 7 * entry_oct); U->len[cursor] = (uint8_t)entry_len;
        for (s = 1; s < entry_len; s++) U->k[(cursor + s) % n] = EMPTY;
        if (!seq_playing) {                                         /* hear it */
            harm_t H; tools_t T; uint8_t nt;
            harm_at(cur_bar(), &H); tools_make(&T, &H, LBASE[sel] + ln[sel].oct * 12 + key_off);
            nt = (uint8_t)clampi(stp(&T, U->k[cursor]), 0, 127);
            audition((uint32_t)sel, nt, fx[sel].inst >= 0 ? (uint32_t)fx[sel].inst : PROF[ln[sel].prof].inst[sel]);
        }
        cursor = (cursor + entry_len) % n;
        U->on = 1; invalidate_now();
    }
}
static void input(void)
{
    uint32_t rel, b = fm1_input_edges(&rel), keys = fm1_input_note_edges();
    int32_t e, k, held = (fm1_in.buttons >> BTN_SEL) & 1u;
    static int32_t sel_used;
    if (b & (1u << BTN_SEL)) sel_used = 0;
    for (e = 0; e < 7; e++) {
        int32_t d = fm1_enc_take((uint32_t)e);
        const enc_t *E = &ENC[page][e];
        char t[48], *o;
        if (!d || !E->turn) continue;
        if (held) { d *= 10; sel_used = 1; }
        E->turn(d);
        o = s_cat(t, E->name); o = s_cat(o, " "); E->val(o);
        toast(t);
        area_dirty = 1;
    }
    if (rel & (1u << BTN_SEL) && !sel_used) { legend_on = !legend_on; area_dirty = 1; }
    if (b & (1u << BTN_PLAY)) { start_stop(held); sel_used |= held; }
    if (b & (1u << BTN_HOME)) set_page(PG_PLAY);
    if (b & (1u << BTN_FX)) set_page(PG_FX);
    if (b & (1u << BTN_ARP)) set_page(PG_TRK);
    if (b & (1u << BTN_EDIT)) { if (held && page == PG_EDIT) { int32_t i; for (i = 0; i < 32; i++) { ln[sel].up.k[i] = EMPTY; ln[sel].up.kinds[i] = 0; } cursor = 0; toast("PATTERN CLEARED"); invalidate_now(); sel_used = 1; } else set_page(PG_EDIT); }
    if (b & (1u << BTN_GLO)) set_page(PG_SONG);
    if (b & (1u << BTN_SEQ)) {
        if (launch_bar >= 0) { launch_bar = -1; toast("LAUNCH CANCELLED"); }
        else { launch_bar = (cur_bar() / 4 + 1) * 4; toast("LAUNCH ON THE NEXT PHRASE"); }
        invalidate();
    }
    if (b & (1u << BTN_REC)) {
        if (page == PG_EDIT) { if (held) { int32_t i; for (i = 0; i < 32; i++) { ln[sel].up.k[i] = EMPTY; ln[sel].up.kinds[i] = 0; } toast("PATTERN CLEARED"); invalidate_now(); sel_used = 1; } else { pat_grab(sel); toast("GRABBED THIS BAR"); } }
        else if (!arc_on) toast("THE ARC IS OFF (SONG PAGE)");
        else { drop_press(sel); { char t[40], *o = s_cat(t, "BREAK: "); o = s_cat(o, LNAME[sel]); s_cat(o, " ALONE"); toast(t); } }
    }
    if (rel & (1u << BTN_REC)) drop_release();
    if (b & (1u << BTN_ENV)) {
        if (page == PG_EDIT) edit_key(1, 1);                        /* tie (key 1 = F#3, a black key) */
        else { ln[sel].lock = (uint8_t)!ln[sel].lock; toast(ln[sel].lock ? "LOCKED: KEEPS ITS PROFILE" : "UNLOCKED"); invalidate(); }
    }
    if (b & (1u << BTN_LFO)) {
        int32_t n = pat_generate(sel, held);
        char t[40], *o;
        sel_used |= held;
        if (n) { o = s_cat(t, LNAME[sel]); o = s_cat(o, held ? ": NEW PATTERN #" : ": VARIATION #"); s_int(o, n - 1); toast(t); gen_count[sel] = n; }
        else toast("NO NEW ONE: SEL+LFO = FRESH");
    }
    if (b & (1u << BTN_OCT_DOWN)) { if (page == PG_EDIT) entry_oct = clampi(entry_oct - 1, -1, 1); else t_oct(-1); area_dirty = 1; }
    if (b & (1u << BTN_OCT_UP)) { if (page == PG_EDIT) entry_oct = clampi(entry_oct + 1, -1, 1); else t_oct(1); area_dirty = 1; }
    if (b & (1u << BTN_SAVE)) {
        if (held) { debug_on = !debug_on; isr_us_max = 0; sel_used = 1; }
        else if (seq_playing) { save_pending = 1; toast("SAVES WHEN YOU STOP"); }
        else do_save();
    }
    for (k = 0; k < 27; k++) {
        if (!(keys >> k & 1u)) continue;
        if (page == PG_EDIT) edit_key(k, held);
        else { set_key((53 + k) % 12); { char t[24], *o = s_cat(t, "KEY "); o = s_cat(o, NN[key]); s_cat(o, " FROM THE NEXT BAR"); toast(t); } }
        area_dirty = 1;
    }
}
static void leds(void)
{
    static uint32_t last = 0xFFFFFFFFu, lastk = 0xFFFFFFFFu;
    uint32_t m = 0, km = 0, k;
    arc_t A = arc_at(cur_bar());
    m |= (uint32_t)seq_playing << BTN_PLAY;
    m |= (uint32_t)(A.mode == A_BREAK || drop_seg >= 0) << BTN_REC;
    m |= (uint32_t)(page == PG_PLAY) << BTN_HOME | (uint32_t)(page == PG_FX) << BTN_FX | (uint32_t)(page == PG_TRK) << BTN_ARP;
    m |= (uint32_t)(page == PG_EDIT) << BTN_EDIT | (uint32_t)(page == PG_SONG) << BTN_GLO;
    m |= (uint32_t)(launch_bar >= 0) << BTN_SEQ | (uint32_t)ln[sel].lock << BTN_ENV | (uint32_t)save_pending << BTN_SAVE;
    if (page == PG_EDIT) {
        const upat_t *U = &ln[sel].up;
        for (k = 0; k < 27u; k++) {
            int32_t wi = white_index((int32_t)k);
            if (sel == L_DRUMS) { if (wi >= 0 && wi < NDK && (U->kinds[cursor] >> wi & 1u)) km |= 1u << k; }
            else if (wi >= 0 && U->k[cursor] != EMPTY && wi - 4 + 7 * entry_oct == U->k[cursor]) km |= 1u << k;
        }
    } else for (k = 0; k < 27u; k++) if ((53u + k) % 12u == (uint32_t)key) km |= 1u << k;
    if (m != last) { for (k = 0; k < 14u; k++) fm1_led_key(k, m >> k & 1u); last = m; }
    if (km != lastk) { for (k = 0; k < 27u; k++) fm1_led_key(KEY_LED(k), km >> k & 1u); lastk = km; }
}

/* ---- the splash: the logo (4 bits a pixel, a 16-colour palette), the version in a corner */
static void splash(void)
{
    int32_t y, x;
    for (y = 0; y < 240; y += (int32_t)STRIP_H) {
        int32_t h = 240 - y < (int32_t)STRIP_H ? 240 - y : (int32_t)STRIP_H, r;
        cs_begin(y, h, C_BG);
        for (r = 0; r < h; r++) {
            const uint8_t *src = LOGO_PX + (y + r) * 120;
            uint16_t *dst = cvp + r * 240;
            for (x = 0; x < 120; x++) { dst[2 * x] = SW(LOGO_PAL[src[x] >> 4]); dst[2 * x + 1] = SW(LOGO_PAL[src[x] & 15]); }
        }
        textr(236, 226, "v" APP_VER, C_DIMG, 1);
        cs_end();
    }
}

static void app_main(void)
{
    uint32_t t_frame = 0, t_splash;
    synth_init();
    music_init();
    set_key(2);
    set_bpm(125);
    do_load();
    splash();
    t_splash = fm1_ms;
    while (fm1_ms - t_splash < 1500u) platform_poll();
    lcd_fill(0, 0, 240, 240, C_BG);
    for (;;) {
        uint8_t st, d1, d2;
        platform_poll();
        input();
        while (midi_get(&st, &d1, &d2)) {                           /* MIDI in: start / stop / continue */
            if (st == 0xFA) { if (!seq_playing) start_stop(1); }
            else if (st == 0xFB) { if (!seq_playing) start_stop(0); }
            else if (st == 0xFC) { if (seq_playing) start_stop(0); }
        }
        while (esp_mq_r != esp_mq_w) { uint32_t m = esp_mq[esp_mq_r % 64u]; esp_mq_r++; midi_send(m & 0xFFu, m >> 8 & 0xFFu, m >> 16 & 0xFFu); }
        if (launch_bar >= 0 && cur_bar() >= launch_bar) { commit_launch(); toast("LAUNCHED"); }
        bars_update();
        bars_update();
        if (save_pending && !seq_playing) { save_pending = 0; do_save(); }
        leds();
        if (fm1_ms - t_frame >= 33u || area_dirty) { t_frame = fm1_ms; draw_frame(area_dirty); }
    }
}
