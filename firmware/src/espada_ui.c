/* SPDX-License-Identifier: GPL-3.0-only */
/* Espada's screen and panel, laid out as Tuna and Felucca do it:
 *
 *   header 0..20     where you are (MAIN, BASS, BASS FX ...), the bar in the phrase, the tempo
 *   cards 22..68     four cards: what KNOB 1..4 do here and their values, in the order the knobs sit
 *   panel 70..200    the lanes (MAIN: all five; a channel: its own, large), the step grid, the song, the arc
 *   footer 202..240  what PRESETS sets (MAIN: the queued profile; a channel: its sound), and messages
 *
 * Knobs: SELECT tempo, ALGORITHM the channel (MAIN, LEAD, PAD, BASS, DRUMS, SFX), PRESETS the sound, KNOB 1..4
 * the cards. SEL held: steps of 10.
 * Buttons: HOME the next channel (SEL+HOME: MAIN). FX the channel's effects (again: FX 2, again: back), ARP its
 * tracker effects, EDIT its steps, GLO the song (again: the arc). PLAY start/stop (SEL+PLAY: from the top), SEQ
 * launch, REC drop (tap or hold; EDIT: grab, SEL+REC: clear), ENV lock (EDIT: tie), LFO generate (SEL+LFO:
 * fresh), OCT-/OCT+ the keys' octave (both: reset), SAVE (SEL+SAVE: the audio interrupt's time).
 * Keys: play the channel in view (MAIN: the last one); on the EDIT page they enter steps. */

#define RGBC(h) RGB(((h) >> 16) & 255, ((h) >> 8) & 255, (h) & 255)
#define SW(c) ((uint16_t)(((c) >> 8) | ((c) << 8)))
static const uint16_t LCOL[NLANE] = {RGBC(0x5fe3e8), RGBC(0xb49bff), RGBC(0xffb04a), RGBC(0xf0e07a), RGBC(0xff7a9a)};
static const char *const LNAME[NLANE] = {"LEAD", "PAD", "BASS", "DRUMS", "SFX"};
static const char *const SXNAME[NSX] = {"WASH", "RISER", "IMPACT", "ZAP", "CRACKLE", "BITS"};
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
#define HEAD_H 20                   /* the header: where you are, the tempo, the bar in the phrase */
#define CARD_Y 22                   /* four cards: what KNOB 1..4 do here, as the knobs sit on the panel */
#define CARD_W 57
#define CARD_H 46
#define CARD_X(c) (3 + 59 * (int32_t)(c))
#define PANEL_Y 70                  /* the panel: the lanes, the step grid, the song */
#define PANEL_H 130
#define LH 24                       /* MAIN: five lanes of 24 + 2 */
#define GAP 2
#define FOOT_Y 202                  /* the footer: what PRESETS sets, and messages */

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
static int32_t text7(int32_t x, int32_t y, const char *s, uint16_t c)    /* FONT_S at 7 px a character (the cards) */
{
    char one[2] = {0, 0};
    for (; *s; s++) { one[0] = *s; text(x, y, one, c, 1); x += 7; }
    return x;
}
static int32_t tw7(const char *s) { int32_t n = 0; while (s[n]) n++; return n ? n * 7 - 1 : 0; }
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

/* ---- UI state: the view (0 = MAIN, 1.. = a channel: lane + 1) and its page */
enum { PG_HOME, PG_FX, PG_FX2, PG_TRK, PG_EDIT, PG_SONG, PG_SONG2, NPG };
static const char *const PGN[NPG] = {"", "FX", "FX 2", "TRK", "EDIT", "SONG", "ARC"};
static int32_t page = PG_HOME, back_page = PG_HOME, sel = L_LEAD, view = 0, debug_on;
static int32_t last_enc = -1, area_dirty = 1, kb_oct;
static uint32_t last_enc_ms, keys_down;
static int32_t cursor, entry_len = 1, entry_oct;
static char toast_s[48];
static uint32_t toast_until;
static int32_t save_pending, gen_count[NLANE];
static void toast(const char *s) { uint32_t i; for (i = 0; i < 47u && s[i]; i++) toast_s[i] = s[i]; toast_s[i] = 0; toast_until = fm1_ms + 1600u; }
static const char *const NN[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const char *const WOBN[6] = {"OFF", "1/1", "1/2", "1/4", "1/8", "1/16"};
static const char *const DKN[NDK] = {"K", "S", "H", "T", "C"};
static const char *const PSHORT[4] = {"EPIC", "HYPER", "FUSION", "OLDSKOOL"};
static const char *const SSHORT[13] = {"AUTO", "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYD", "LOCRIAN",
                                       "HARM MIN", "MEL MIN", "MAJ PENT", "MIN PENT", "BLUES"};
static void set_key(int32_t k) { key = fmodp(k, 12); key_off = key - 2 > 5 ? key - 14 : key - 2; invalidate(); }
static void set_bpm(int32_t b)
{
    if (b < 60) b = 60;
    if (b > 200) b = 200;
    seq_bpm = b;
    seq_step_len_q8 = 169344000u / (uint32_t)b;             /* 44100 * 60 / 4 * 256 / bpm */
}
static const char *inst_name(int32_t l, int32_t inst) { return l == L_SFX ? SXNAME[inst] : l == L_DRUMS ? KITS[inst].name : PATCH[inst].name; }
static int32_t inst_eff(int32_t l) { return fxu[l].inst >= 0 ? fxu[l].inst : l == L_SFX ? 0 : PROF[ln[l].prof].inst[l]; }

/* ---- the knobs' functions: turn, and the value (and a second line) as the cards show them */
typedef void (*turn_fn)(int32_t d);
typedef void (*val_fn)(char *o);
typedef struct { const char *name; turn_fn turn; val_fn val, sub; } card_t;
static int32_t clampi(int32_t v, int32_t a, int32_t b) { return v < a ? a : v > b ? b : v; }
static void go_view(int32_t v)
{
    view = fmodp(v, (int32_t)NLANE + 1);
    if (view) { sel = view - 1; cursor = clampi(cursor, 0, ln[sel].up.bars * 16 - 1); }
    else if (page < PG_SONG) page = PG_HOME;
    area_dirty = 1;
}
static void t_lprof(int32_t d) { ln[sel].prof = (uint8_t)fmodp(ln[sel].prof + (d > 0 ? 1 : -1), NPROF); invalidate(); }
static void v_lprof(char *o) { s_cat(o, PSHORT[ln[sel].prof]); }
static void t_prof(int32_t d) { target = fmodp(target + (d > 0 ? 1 : -1), NPROF); invalidate(); }
static void v_prof(char *o) { s_cat(o, PSHORT[target]); }
static void t_blend(int32_t d) { blend = clampi(blend + (d > 0 ? 1 : -1), 0, 4); invalidate(); }
static void v_blend(char *o) { s_pct(o, blend * 25); }
static void t_dens(int32_t d) { ln[sel].dens = (uint8_t)clampi(ln[sel].dens + d, 0, 100); invalidate(); }
static void v_dens(char *o) { s_pct(o, ln[sel].dens); }
static void t_oct(int32_t d) { if (HITS(sel)) return; ln[sel].oct = (int8_t)clampi(ln[sel].oct + (d > 0 ? 1 : -1), -1, 1); invalidate(); }
static void v_oct(char *o) { if (HITS(sel)) s_cat(o, "--"); else s_sign(o, ln[sel].oct); }
static void t_inst(int32_t d)
{
    int32_t n = sel == L_SFX ? (int32_t)NSX : sel == L_DRUMS ? (int32_t)NKIT : (int32_t)NPATCH, v = fxu[sel].inst + 1;   /* 0 = AUTO */
    v = fmodp(v + (d > 0 ? 1 : -1), n + 1);
    fxu[sel].inst = v - 1;
    if (sel == L_SFX) invalidate();                     /* the sfx sound changes what is generated */
}
static void v_inst(char *o) { if (sel == L_SFX && fxu[sel].inst < 0) s_cat(o, "AUTO"); else s_cat(o, inst_name(sel, inst_eff(sel))); }
static void s_inst(char *o) { if (fxu[sel].inst < 0 && sel != L_SFX) s_cat(o, "AUTO"); }
static void t_tempo(int32_t d) { set_bpm(seq_bpm + d); }
static void v_tempo(char *o) { o = s_int(o, seq_bpm); s_cat(o, " BPM"); }
static void v_filt(char *o) { int32_t f = fxu[sel].filt; if (!f) s_cat(o, "OFF"); else { o = s_cat(o, f < 0 ? "LP " : "HP "); s_pct(o, f < 0 ? -f : f); } }
static void t_filt(int32_t d) { fxu[sel].filt = clampi(fxu[sel].filt + d, -100, 100); }
static void t_res(int32_t d) { fxu[sel].res = clampi(fxu[sel].res + d, 0, 100); }
static void v_res(char *o) { s_pct(o, fxu[sel].res); }
static void t_wob(int32_t d) { fxu[sel].wob = clampi(fxu[sel].wob + (d > 0 ? 1 : -1), 0, 5); }
static void v_wob(char *o) { s_cat(o, WOBN[fxu[sel].wob]); }
static void t_bright(int32_t d) { if (!HITS(sel)) fxu[sel].bright = clampi(fxu[sel].bright + d, 0, 100); }
static void v_bright(char *o) { if (HITS(sel)) s_cat(o, "--"); else s_pct(o, fxu[sel].bright); }
static void t_drive(int32_t d) { fxu[sel].drive = clampi(fxu[sel].drive + d, 0, 100); }
static void v_drive(char *o) { s_pct(o, fxu[sel].drive); }
static void t_vary(int32_t d) { fxu[sel].vary = clampi(fxu[sel].vary + d, 0, 100); }
static void v_vary(char *o) { s_pct(o, fxu[sel].vary); }
static void s_vary(char *o) { if (fxu[sel].vary) s_cat(o, VARYN[vary_char[sel]]); }
static void t_level(int32_t d) { fxu[sel].level = clampi(fxu[sel].level + d, 0, 100); }
static void v_level(char *o) { s_pct(o, fxu[sel].level); }
static void t_slide(int32_t d) { if (!HITS(sel)) fxu[sel].slide = clampi(fxu[sel].slide + d, 0, 100); }
static void v_slide(char *o) { if (HITS(sel)) s_cat(o, "--"); else s_pct(o, fxu[sel].slide); }
static void t_rtg(int32_t d) { fxu[sel].retrig = clampi(fxu[sel].retrig + d, 0, 100); }
static void v_rtg(char *o) { s_pct(o, fxu[sel].retrig); }
static void t_rate(int32_t d) { fxu[sel].rtg_rate = clampi(fxu[sel].rtg_rate + (d > 0 ? 1 : -1), 1, 3); }
static void v_rate(char *o) { o = s_cat(o, "X"); s_int(o, fxu[sel].rtg_rate); }
static void t_crush(int32_t d) { fxu[sel].crush = clampi(fxu[sel].crush + d, 0, 100); }
static void v_crush(char *o) { s_pct(o, fxu[sel].crush); }
static void t_echo(int32_t d) { fxu[sel].echo = clampi(fxu[sel].echo + d, 0, 100); }
static void v_echo(char *o) { s_pct(o, fxu[sel].echo); }
static void t_step(int32_t d) { int32_t n = ln[sel].up.bars * 16; cursor = fmodp(cursor + d, n); }
static void v_step(char *o) { o = s_int(o, cursor + 1); o = s_cat(o, "/"); s_int(o, ln[sel].up.bars * 16); }
static void t_len(int32_t d) { entry_len = clampi(entry_len + (d > 0 ? 1 : -1), 1, 16); }
static void v_len(char *o) { s_int(o, entry_len); }
static void t_bars(int32_t d) { ln[sel].up.bars = (uint8_t)(d > 0 ? 2 : 1); cursor = clampi(cursor, 0, ln[sel].up.bars * 16 - 1); invalidate_now(); }
static void v_bars(char *o) { s_int(o, ln[sel].up.bars); }
static void t_use(int32_t d) { ln[sel].up.on = (uint8_t)(d > 0); invalidate_now(); }
static void v_use(char *o) { s_cat(o, ln[sel].up.on ? "YOURS" : "PROFILE"); }
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
static void v_scale(char *o) { s_cat(o, SSHORT[scale_sel]); }
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
static void v_prog(char *o) { if (!prog_sel) s_cat(o, "AUTO"); else { o = s_int(o, prog_sel); o = s_cat(o, " OF "); s_int(o, NPROG); } }
static void t_bpc(int32_t d) { bpc = clampi(bpc + (d > 0 ? 1 : -1), 1, 4); invalidate(); }
static void v_bpc(char *o) { s_int(o, bpc); }
static void t_arc(int32_t d) { int32_t was = arc_on; arc_on = d > 0; if (arc_on && !was) arc_restart(cur_bar() + 1); invalidate(); }
static void v_arc(char *o) { s_cat(o, arc_on ? "ON" : "OFF"); }
static void t_build(int32_t d) { build_ph = clampi(build_ph + (d > 0 ? 1 : -1), 2, 16); invalidate(); }
static void v_build(char *o) { s_int(o, build_ph); }
static void s_phr(char *o) { s_cat(o, "PHRASES"); }
static void t_peak(int32_t d) { peak_ph = clampi(peak_ph + (d > 0 ? 1 : -1), 1, 8); invalidate(); }
static void v_peak(char *o) { s_int(o, peak_ph); }
static void t_energy(int32_t d) { energy = clampi(energy + d, -50, 50); invalidate(); }
static void v_energy(char *o) { if (energy) s_sign(o, energy); else s_cat(o, "0"); }
static void t_space(int32_t d) { space = clampi(space + d, 0, 100); }
static void v_space(char *o) { s_pct(o, space); }
static void s_blend(char *o) { static const char *const W[5] = {"", "DRUMS", "+BASS", "+PAD", "ALL"}; s_cat(o, W[blend]); }
static void t_auto(int32_t d) { auto_drop = d > 0; invalidate(); }
static void v_auto(char *o) { s_cat(o, auto_drop ? "ON" : "OFF"); }


/* the four cards of each page: KNOB 1..4 */
static const card_t CARDS[NPG][4] = {
    {{"PROFILE", t_lprof, v_lprof, 0}, {"DENSITY", t_dens, v_dens, 0}, {"OCTAVE", t_oct, v_oct, 0}, {"LEVEL", t_level, v_level, 0}},
    {{"FILTER", t_filt, v_filt, 0}, {"RESO", t_res, v_res, 0}, {"WOBBLE", t_wob, v_wob, 0}, {"DRIVE", t_drive, v_drive, 0}},
    {{"BRIGHT", t_bright, v_bright, 0}, {"CRUSH", t_crush, v_crush, 0}, {"VARY", t_vary, v_vary, s_vary}, {0, 0, 0, 0}},
    {{"SLIDE", t_slide, v_slide, 0}, {"RETRIG", t_rtg, v_rtg, 0}, {"RATE", t_rate, v_rate, 0}, {"ECHO", t_echo, v_echo, 0}},
    {{"STEP", t_step, v_step, 0}, {"LENGTH", t_len, v_len, 0}, {"BARS", t_bars, v_bars, 0}, {"PATTERN", t_use, v_use, 0}},
    {{"KEY", t_key, v_key, 0}, {"SCALE", t_scale, v_scale, 0}, {"CHORDS", t_prog, v_prog, 0}, {"BARS/CH", t_bpc, v_bpc, 0}},
    {{"ARC", t_arc, v_arc, 0}, {"BUILD", t_build, v_build, s_phr}, {"PEAK", t_peak, v_peak, s_phr}, {"AUTODROP", t_auto, v_auto, 0}},
};
static const card_t CARDS_MAIN[4] = {
    {"QUEUE", t_prof, v_prof, 0}, {"BLEND", t_blend, v_blend, s_blend}, {"ENERGY", t_energy, v_energy, 0}, {"SPACE", t_space, v_space, 0}};
static const card_t CARDS_SFX[4] = {
    {"SOUND", t_inst, v_inst, 0}, {"DENSITY", t_dens, v_dens, 0}, {0, 0, 0, 0}, {"LEVEL", t_level, v_level, 0}};
static const card_t *card_at(int32_t c)
{
    if (page >= PG_SONG) return &CARDS[page][c];
    if (!view) return &CARDS_MAIN[c];
    if (page == PG_HOME && sel == L_SFX) return &CARDS_SFX[c];
    return &CARDS[page][c];
}
/* where you are: MAIN, BASS, BASS FX, ..., SONG; and its colour */
static void where(char *o)
{
    if (page >= PG_SONG) { s_cat(o, PGN[page]); return; }
    if (!view) { s_cat(o, "MAIN"); return; }
    o = s_cat(o, LNAME[sel]);
    if (page != PG_HOME) { o = s_cat(o, " "); s_cat(o, PGN[page]); }
}
static uint16_t where_col(void) { return page >= PG_SONG ? RGBC(0xb49bff) : view ? LCOL[sel] : C_CYAN; }

/* ---- drawing: the lanes */
static int32_t pos16;                                       /* the play head, in steps Q16 */
static int32_t sx(int32_t step_q16) { return NOWX + (int32_t)(((int64_t)(int32_t)((uint32_t)step_q16 - (uint32_t)pos16) * PPS) >> 16); }
static int rt_on(int32_t l, const ev_t *e) { return fxu[l].retrig > 0 && (int32_t)(e->rv * 100u >> 8) < fxu[l].retrig; }
static void shape(int32_t x, int32_t y, int32_t w, int32_t h, int past, int ghost, int on, uint16_t col)
{
    if (on && seq_playing) fill(x, y - 1, w, h + 2, C_WHITE);
    else if (ghost && !past) outline(x, y, w, h, col);
    else fill(x, y, w, h, past ? cscale(col, 77) : col);
}
static int32_t note_y(int32_t l, int32_t y, int32_t h, int32_t n)
{
    int32_t yy = y + h - 6 - (n - LLO[l]) * (h - 16) / (LHI[l] - LLO[l]);
    if (fxu[l].crush > 0) { int32_t q = 1 + fxu[l].crush * 5 / 100; yy = (yy + q / 2) / q * q; }
    return yy;
}
static int fx_on(int32_t l) { return fx[l].filt || fx[l].wob || fx[l].drive || fx[l].bright != 25 || fx[l].slide || fx[l].retrig || fx[l].crush || fx[l].echo; }
/* one lane at y, h high, into the current strip (it clips); big: a channel's own view */
static void draw_lane(int32_t l, int32_t y, int32_t h, int big)
{
    int32_t r, b, b0, b1, prev_y = -1000, flash = (int32_t)(fm1_ms - lane_hit_ms[l]) < 110 && seq_playing;
    int32_t nh = big ? 5 : 3, hh = big ? 9 : 5;                     /* note and hit heights */
    uint16_t col = LCOL[l];
    const bar_t *Bc = bar_get(cur_bar());
    if (y >= cy0 + chh || y + h + GAP <= cy0) return;
    for (r = 0; r < h; r++) {                                       /* the copper bar */
        int32_t a = (big ? 44 : 26) + flash * 56, tri = 256 - (r > h / 2 ? r - h / 2 : h / 2 - r) * 256 / (h / 2);
        fill(0, y + r, 240, 1, cscale(col, a * tri >> 8));
    }
    b0 = (pos16 >> 16) / 16 - 1; if (b0 < 0) b0 = 0;
    b1 = ((pos16 >> 16) + (240 - NOWX) / PPS) / 16 + 1;
    for (b = b0; b <= b1; b++) {                                    /* bar and phrase lines */
        int32_t x = sx(b * 16 << 16);
        if (b == launch_bar) { int32_t k; for (k = 0; k < h; k += 4) fill(x, y + k, 1, 2, C_WHITE); }
        else fill(x, y, 1, h, cscale(RGBC(0x9fb3b0), b % 4 == 0 ? 82 : 31));
    }
    for (b = b0; b <= b1; b++) {
        const bar_t *B = bar_get(b);
        uint32_t i;
        int ghost;
        if (!B) continue;
        ghost = l != L_SFX && B->prof[l] != ln[l].prof && !B->user[l];
        for (i = 0; i < B->cnt[l]; i++) {
            const ev_t *e = &B->ev[l][i];
            int32_t x = sx((b * 16 + e->st) << 16), rt = rt_on(l, e), k;
            if (x > 240 || x < -64) continue;
            if (HITS(l)) {
                int32_t kind = e->note[0], row, rows, w, yy, len = 0;
                if (l == L_DRUMS) { rows = 3; row = kind == DK_SNARE ? 1 : kind == DK_HAT || kind == DK_CRASH ? 2 : 0; w = kind == DK_KETTLE || kind == DK_CRASH ? 6 : 3; }
                else { rows = NSX; row = kind; w = 3; if (kind == SX_WASH || kind == SX_RISER || kind == SX_CRACKLE) len = e->len * PPS - 1; }
                yy = y + h - 4 - hh - row * (h - 6 - hh) / (rows - 1);
                if (len) {                                          /* the long sfx: a band; the riser grows */
                    int32_t past = x + len < NOWX;
                    if (kind == SX_RISER) { for (k = 0; k < len; k += 2) { int32_t g = 1 + k * hh / len; fill(x + k, yy + hh - g, 2, g, past ? cscale(col, 77) : col); } }
                    else if (kind == SX_CRACKLE) { for (k = 0; k < len; k += 3) fill(x + k, yy + (int32_t)((k * 7) % (hh > 1 ? hh : 1)), 1, 2, past ? cscale(col, 77) : col); }
                    else fill(x, yy + hh / 3, len, hh - hh / 3, past ? cscale(col, 60) : cscale(col, 170));
                    continue;
                }
                if (fx[l].echo > 0) for (k = 1; k <= 2; k++) outline(x + k * 3 * PPS, yy, w, hh, cscale(col, k == 1 ? fx[l].echo * 140 / 100 : fx[l].echo * fx[l].echo * 90 / 10000));
                if (rt) for (k = 0; k < fx[l].rtg_rate; k++) { int32_t sw = PPS / fx[l].rtg_rate; shape(x + k * sw, yy, sw > 1 ? sw - 1 : 1, hh, x + k * sw < NOWX, ghost, 0, col); }
                else shape(x, yy, w, hh, x + w < NOWX, ghost, x <= NOWX && x + w + 3 > NOWX, col);
            } else {
                int32_t w = e->len * PPS - 1, nn;
                if (w < 2) w = 2;
                for (nn = 0; nn < e->n; nn++) {
                    int32_t yy = note_y(l, y, h, e->note[nn]), on = x <= NOWX && x + w > NOWX;
                    if (fx[l].echo > 0) for (k = 1; k <= 2; k++) outline(x + k * 3 * PPS, yy, w < 3 * PPS ? w : 3 * PPS, nh, cscale(col, k == 1 ? fx[l].echo * 140 / 100 : fx[l].echo * fx[l].echo * 90 / 10000));
                    if (e->flags & EV_ARP) { for (k = 0; k < w; k += 3) shape(x + k, yy, 2, nh, x + k + 2 < NOWX, ghost, on, col); }
                    else if (rt) { int32_t sw = PPS / fx[l].rtg_rate; if (sw < 1) sw = 1; for (k = 0; k < w; k += sw) shape(x + k, yy, sw > 1 ? sw - 1 : 1, nh, x + k < NOWX, ghost, on, col); }
                    else shape(x, yy, w, nh, x + w < NOWX, ghost, on, col);
                    if (nn == 0 && !(e->flags & EV_ARP)) {
                        if (fx[l].slide > 0 && prev_y > -1000 && (prev_y - yy > 1 || yy - prev_y > 1)) {
                            int32_t gl = fx[l].slide * 132 * PPS / (int32_t)(seq_step_len_q8 >> 8);   /* up to 0.3 s */
                            if (gl > w) gl = w;
                            line(x, prev_y + nh / 2, x + gl, yy + nh / 2, x < NOWX ? cscale(col, 77) : col);
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
        lo = HITS(l) ? 30 : LLO[l]; hi = HITS(l) ? 120 : LHI[l];
        yc = y + h - 6 - ((note16 >> 4) - lo) * (h - 16) / (hi - lo);
        if (yc < y) yc = y;
        if (yc > y + h) yc = y + h;
        if (lp) darken(0, y, 240, yc - y, 97); else darken(0, yc, 240, y + h - yc, 97);
        fill(0, yc - fx[l].res / 50, 240, 1 + fx[l].res / 50, cscale(col, 115 + fx[l].res * 140 / 100));
        { int32_t ty = clampi(lp ? yc + 3 : yc - 13, y + 2, y + h - 12); fill(213, ty - 2, 20, 14, C_BG); text(215, ty, lp ? "LP" : "HP", col, 1); }
    }
    if (Bc && Bc->muted[l]) darken(0, y, NOWX, h, 140);
    if (!big) {                                                     /* MAIN: the name; locked: inverted */
        int lock = ln[l].lock && l != L_SFX;
        fill(1, y + (h - 14) / 2, tw(LNAME[l], 1) + 5, 14, lock ? C_CREAM : C_BG);
        text(3, y + (h - 14) / 2 + 2, LNAME[l], lock ? C_BG : col, 1);
    } else {
    fill(1, y + 1, tw(LNAME[l], 1) + 5, 14, C_BG);                 /* the label: name */
    text(3, y + 3, LNAME[l], col, 1);
    {                                                               /* profile, USR or the sfx sound; locked: inverted */
        char t[4] = {PROF[ln[l].prof].name[0], PROF[ln[l].prof].name[1], PROF[ln[l].prof].name[2], 0};
        const char *tag = ln[l].up.on ? "USR" : l == L_SFX ? (fx[l].inst < 0 ? "AUT" : SXNAME[fx[l].inst]) : t;
        char t3[4] = {tag[0], tag[1], tag[2], 0};
        int lock = ln[l].lock && l != L_SFX;
        fill(1, y + 15, 27, 14, lock ? C_CREAM : C_BG);
        text(3, y + 17, t3, lock ? C_BG : ln[l].up.on ? C_CREAM : C_MUTE, 1);
        if (fx_on(l)) { fill(30, y + 15, 19, 14, col); text(32, y + 17, "FX", C_BG, 1); }
    }
    }
    if (!ln[l].lock && LORDER[l] < blend && target != ln[l].prof) fill(236, y + 2, 2, h - 4, C_WHITE);
    fill(NOWX, y, 1, h + GAP, cscale(C_WHITE, 218));                /* now */
    if (big || l == 0) { fill(NOWX - 3, y, 7, 1, C_WHITE); fill(NOWX - 2, y + 1, 5, 1, C_WHITE); fill(NOWX - 1, y + 2, 3, 1, C_WHITE); }
}

/* ---- drawing: the EDIT page (a step grid) and the SONG page, in strips over the lane area */
static void draw_edit_area(void)
{
    const upat_t *U = &ln[sel].up;
    int32_t n = U->bars * 16, x0 = 16, cwq = (220 << 8) / n, top = PANEL_Y + 1, h = PANEL_H - 2, hits = HITS(sel);
    int32_t rows = sel == L_DRUMS ? NDK : sel == L_SFX ? NSX : 18, rh = h / rows, i, s;
    uint16_t col = LCOL[sel];
#define CX(s) (x0 + (((s) * cwq) >> 8))
    for (i = 0; i < rows; i++) {
        int32_t yy = top + i * rh, k = 14 - i - 3, strong = hits || fmodp(k, 7) == 0;
        char t[3] = {0, 0, 0};
        fill(x0, yy, 236 - x0, rh - 1, strong ? C_GRID : RGBC(0x0a1414));
        if (sel == L_DRUMS) t[0] = DKN[i][0]; else if (sel == L_SFX) t[0] = SXNAME[i][0]; else t[0] = (char)('1' + fmodp(k, 7));
        if (strong) textc(8, yy + (rh - 10) / 2, t, col, 1);
    }
    for (s = 0; s <= n; s++) fill(CX(s), top, 1, h, s % 16 == 0 ? RGBC(0x9fb3b0) : s % 4 == 0 ? RGBC(0x3b5150) : RGBC(0x1b2928));
    fill(CX(cursor), top, CX(cursor + 1) - CX(cursor), h, RGBC(0x2a3a3a));
    outline(CX(cursor), top, CX(cursor + (hits ? 1 : entry_len)) - CX(cursor) + 1, h, C_WHITE);
    for (s = 0; s < n; s++) {
        if (hits) { for (i = 0; i < rows; i++) if (U->kinds[s] >> i & 1u) fill(CX(s) + 1, top + i * rh + 2, CX(s + 1) - CX(s) - 2, rh - 5, col); }
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
    int32_t auto_ = prog_sel == 0, i, n, cur, per, bw, y0 = PANEL_Y;
    int32_t roots[8]; int8_t tones[8][3];
    text(6, y0 + 6, NN[key], C_CYAN, 2);
    t[0] = 0;
    if (auto_) { char *o = s_cat(t, "PROFILE "); s_cat(o, PROF[ln[L_BASS].prof].name); } else s_cat(t, SCALES[scale_sel].name);
    text(48, y0 + 6, t, C_CREAM, 1);
    t[0] = 0;
    if (auto_) s_cat(t, "ITS OWN CHORDS"); else { char *o = s_int(t, bpc); s_cat(o, bpc == 1 ? " BAR A CHORD" : " BARS A CHORD"); }
    text(48, y0 + 20, t, C_MUTE, 1);
    {                                                       /* the suggestions, the chosen one in the middle */
        int32_t j;
        for (j = -1; j <= 1; j++) {
            int32_t k = prog_sel + j, yy = y0 + 38 + (j + 1) * 15;
            if (k < 0 || k > (auto_ ? 0 : NPROG)) continue;
            t[0] = 0;
            if (k == 0) s_cat(t, "FROM THE PROFILE"); else prog_text(t, k - 1);
            if (!j) { fill(4, yy, 232, 14, C_GRID); fill(4, yy, 3, 14, C_CYAN); }
            text(12, yy + 2, t, j ? C_DIMG : C_WHITE, 1);
        }
    }
    if (auto_) { const prof_t *P = &PROF[ln[L_BASS].prof]; n = 8; for (i = 0; i < 8; i++) { int32_t q = P->prog[i][1], k; roots[i] = P->prog[i][0]; for (k = 0; k < 3; k++) tones[i][k] = k < QUAL_N[q] ? QUAL[q][k] : 0; if (QUAL_N[q] == 2) tones[i][2] = 7, tones[i][1] = 7; } }
    else { const int8_t *P = prog_of(prog_sel - 1); n = 4; for (i = 0; i < 4; i++) chord_of(scale_sel, P[i], &roots[i], tones[i]); }
    cur = auto_ ? fmodp(cur_bar(), n) : (cur_bar() / bpc) % n;
    per = n > 4 ? 4 : n; bw = (232 - (per - 1) * 4) / per;
    for (i = 0; i < n; i++) {                               /* the chords; the one playing lit */
        int32_t big = n <= 4, hh = big ? 31 : 20, x = 4 + (i % per) * (bw + 4), yy = y0 + 86 + (i / per) * (hh + 2), on = seq_playing && i == cur;
        char *o = t;
        o = s_cat(o, NN[(key + roots[i]) % 12]);
        s_cat(o, tones[i][1] == 7 ? "5" : tones[i][2] == 6 ? "dim" : tones[i][2] == 8 ? "+" : tones[i][1] == 3 ? "m" : "");
        fill(x, yy, bw, hh, on ? C_CYAN : RGBC(0x1b2928));
        textc(x + bw / 2, yy + 5, t, on ? C_BG : C_CREAM, 1);
        if (big) { numeral(t, roots[i], tones[i]); textc(x + bw / 2, yy + 18, t, on ? C_BG : C_MUTE, 1); }
    }
}
/* the ARC page: the energy over the coming bars, the phases in colour */
static const uint16_t MODEC[5] = {RGBC(0x5d7370), RGBC(0x3fe0e0), C_WHITE, RGBC(0xffb04a), RGBC(0xb49bff)};
static void draw_arc_area(void)
{
    int32_t b, cb = cur_bar(), base = PANEL_Y + PANEL_H - 22;
    static const char *const MN[5] = {"OFF", "BUILD", "BREAK", "PEAK", "DOWN"};
    int32_t m;
    if (!arc_on) { textc(120, PANEL_Y + 50, "THE ARC IS OFF", C_MUTE, 1); textc(120, PANEL_Y + 68, "KNOB 1 TURNS IT ON", C_DIMG, 1); return; }
    for (b = 0; b < 58; b++) {
        arc_t A = arc_at(cb + b);
        int32_t hgt = 2 + A.e * (PANEL_H - 40) / 256;
        fill(4 + b * 4, base - hgt, 3, hgt, b ? MODEC[A.mode] : C_WHITE);
    }
    for (m = 1; m < 5; m++) { fill(8 + (m - 1) * 58, base + 9, 8, 8, MODEC[m]); text(20 + (m - 1) * 58, base + 8, MN[m], C_MUTE, 1); }
}

/* ---- header, cards, footer */
static void draw_header(void)
{
    char t[40], wh[24];
    int32_t cb = cur_bar(), cw, i, x;
    wh[0] = 0; where(wh);
    cw = tw(wh, 1) + 6;
    cs_begin(0, HEAD_H + 2, C_BG);
    fill(0, 0, cw, 15, where_col());
    text(3, 3, wh, C_BG, 1);
    t[0] = 0;
    if (debug_on) { char *o = s_cat(t, "ISR "); o = s_int(o, (int32_t)isr_us_max); s_cat(o, "US"); }
    else { char *o = s_int(t, seq_bpm); s_cat(o, " BPM"); }
    x = 238 - tw(t, 1);
    text(x, 3, t, debug_on ? C_WHITE : C_CREAM, 1);
    for (i = 0; i < 4; i++) {                                       /* the bar in the phrase */
        int32_t on = seq_playing && cb % 4 == i;
        fill(x - 50 + i * 11, 4, 9, 9, on ? where_col() : seq_playing && cb % 4 > i ? cscale(where_col(), 90) : RGBC(0x1d2b2b));
    }
    fill(0, HEAD_H, 240, 1, cscale(where_col(), 90));
    cs_end();
}
static uint32_t cards_hash;
static void draw_cards(int force)
{
    int32_t c;
    uint32_t h = 2166136261u;
    char v[4][24], sb[4][24];
    for (c = 0; c < 4; c++) {
        const card_t *C = card_at(c);
        uint32_t i;
        v[c][0] = sb[c][0] = 0;
        if (C->name) { C->val(v[c]); if (C->sub) C->sub(sb[c]); }
        for (i = 0; v[c][i]; i++) h = (h ^ (uint8_t)v[c][i]) * 16777619u;
        for (i = 0; sb[c][i]; i++) h = (h ^ (uint8_t)sb[c][i]) * 16777619u;
        h = (h ^ (uint32_t)(uintptr_t)C) * 16777619u;
    }
    h ^= (uint32_t)(last_enc * 131 + (fm1_ms - last_enc_ms < 1500u)) + where_col();
    if (!force && h == cards_hash) return;
    cards_hash = h;
    cs_begin(CARD_Y, CARD_H, C_BG);
    for (c = 0; c < 4; c++) {
        const card_t *C = card_at(c);
        int32_t x = CARD_X(c), hot = last_enc == c + 2 && fm1_ms - last_enc_ms < 1500u;
        fill(x, CARD_Y, CARD_W, CARD_H, hot ? cscale(where_col(), 60) : RGBC(0x0e1818));
        if (!C->name) continue;
        fill(x, CARD_Y, CARD_W, 2, hot ? C_WHITE : cscale(where_col(), 150));
        text7(x + 3, CARD_Y + 6, C->name, C_MUTE);
        text7(x + 3, CARD_Y + 20, v[c], hot ? C_WHITE : C_CREAM);
        if (sb[c][0]) text7(x + 3, CARD_Y + 33, sb[c], C_DIMG);
    }
    cs_end();
}
static uint32_t foot_hash;
static void draw_footer(int force)
{
    char a[56], b[56], *o;
    uint16_t ca = C_LIGHT, cb_ = C_LIGHT;
    int32_t cb = cur_bar(), boxes = !view && page < PG_SONG;
    arc_t A = arc_at(cb);
    uint32_t h = 2166136261u, i;
    a[0] = b[0] = 0;
    if (!view || page >= PG_SONG) { o = s_cat(a, "NEXT "); s_cat(o, PROF[target].name); ca = target == ln[L_BASS].prof && !blend ? C_MUTE : C_CYAN; }   /* PRESETS */
    else { o = s_cat(a, sel == L_SFX ? "SOUND " : sel == L_DRUMS ? "KIT " : "SOUND "); v_inst(o); while (*o) o++; s_inst(b); if (b[0]) { o = s_cat(o, " ("); o = s_cat(o, b); s_cat(o, ")"); } b[0] = 0; ca = LCOL[sel]; }
    if (launch_bar >= 0) {
        int32_t left = launch_bar - cb; o = s_cat(b, "LAUNCH "); o = s_cat(o, PROF[target].name); o = s_cat(o, " IN "); o = s_int(o, left < 0 ? 0 : left); s_cat(o, left == 1 ? " BAR" : " BARS");
        cb_ = (pos16 >> 17) % 2 ? C_WHITE : C_CYAN;
    } else if (A.mode == A_BREAK || arc_at(cb + 1).mode == A_BREAK) {
        arc_t N = A.mode == A_BREAK ? A : arc_at(cb + 1);
        if (N.until >= 0) { int32_t left = N.until - cb; o = s_cat(b, A.mode == A_BREAK ? "BREAK - DROP IN " : "BREAK NEXT - DROP IN "); o = s_int(o, left); s_cat(o, left == 1 ? " BAR" : " BARS"); }
        else s_cat(b, "BREAK - LET GO OF REC TO DROP");
        cb_ = (pos16 >> 17) % 2 ? C_WHITE : RGBC(0xffb04a);
    } else if (view && page < PG_SONG && ln[sel].lock && sel != L_SFX) { s_cat(b, "LOCKED (ENV)"); cb_ = C_CREAM; }
    else if (view && page < PG_SONG && kb_oct) { o = s_cat(b, "KEYS OCTAVE "); s_sign(o, kb_oct); cb_ = C_MUTE; }
    if (fm1_ms < toast_until) { char *d = b; const char *s = toast_s; while ((*d++ = *s++)) ; cb_ = C_WHITE; }
    for (i = 0; a[i]; i++) h = (h ^ (uint8_t)a[i]) * 16777619u;
    for (i = 0; b[i]; i++) h = (h ^ (uint8_t)b[i]) * 16777619u;
    h ^= (uint32_t)blend * 31u + (uint32_t)boxes * 977u + ca + cb_ * 7u;
    if (!force && h == foot_hash) return;
    foot_hash = h;
    cs_begin(FOOT_Y, 240 - FOOT_Y, C_FOOT);
    text(4, FOOT_Y + 5, a, ca, 1);
    if (boxes) {                                                    /* the blend: drums, bass, pad, lead */
        static const char *const BX[4] = {"D", "B", "P", "L"};
        for (i = 0; i < 4u; i++) { int32_t x = 150 + (int32_t)i * 22, on = (int32_t)i < blend; fill(x, FOOT_Y + 2, 20, 15, on ? C_CYAN : RGBC(0x1d2b2b)); textc(x + 10, FOOT_Y + 5, BX[i], on ? C_BG : C_MUTE, 1); }
    }
    text(4, FOOT_Y + 22, b, cb_, 1);
    cs_end();
}

/* ---- the frame: the header, the cards, the panel in three strips, the footer */
static uint32_t frames, fps_t0, fps;
static int32_t last_drawn_pos = -1;
static void draw_frame(int force)
{
    int32_t l, k;
    static const int32_t SY[4] = {0, 44, 88, 130};
    pos16 = cur_step() * 65536 + (seq_playing ? (int32_t)vis_frac : 0);
    draw_cards(force);
    if (!force && pos16 == last_drawn_pos && !area_dirty && !seq_playing) { draw_footer(0); return; }
    last_drawn_pos = pos16;
    draw_header();
    platform_poll();
    for (k = 0; k < 3; k++) {
        cs_begin(PANEL_Y + SY[k], SY[k + 1] - SY[k], C_BG);
        if (page == PG_SONG) draw_song_area();
        else if (page == PG_SONG2) draw_arc_area();
        else if (page == PG_EDIT && view) draw_edit_area();
        else if (!view) for (l = 0; l < (int32_t)NLANE; l++) draw_lane(l, PANEL_Y + l * (LH + GAP), LH, 0);
        else draw_lane(sel, PANEL_Y, PANEL_H, 1);
        cs_end();
        platform_poll();
    }
    area_dirty = 0;
    draw_footer(force);
    frames++;
    if (fm1_ms - fps_t0 >= 1000u) { fps = frames; frames = 0; fps_t0 = fm1_ms; }
}

/* ---- saving: settings (256 B) and patterns (their own sector), only while stopped (an erase stalls the sound) */
typedef struct {
    uint8_t ver, key, scale, prog, bpc, arc, build, peak, autod, target, bpm; int8_t energy; uint8_t space;
    struct { uint8_t prof, lock, dens; int8_t oct, filt; uint8_t res, wob, bright, drive, slide, retrig, rate, crush, echo, level; int8_t inst; uint8_t vary; } l[NLANE];
} save_t;
typedef struct { upat_t p[NLANE]; } pats_t;
static void do_save(void)
{
    static save_t S;
    static pats_t P;
    int32_t l, rc;
    S.ver = 5; S.energy = (int8_t)energy; S.space = (uint8_t)space; S.key = (uint8_t)key; S.scale = (uint8_t)scale_sel; S.prog = (uint8_t)prog_sel; S.bpc = (uint8_t)bpc; S.arc = (uint8_t)arc_on;
    S.build = (uint8_t)build_ph; S.peak = (uint8_t)peak_ph; S.autod = (uint8_t)auto_drop; S.target = (uint8_t)target; S.bpm = (uint8_t)seq_bpm;
    for (l = 0; l < (int32_t)NLANE; l++) {
        S.l[l].prof = ln[l].prof; S.l[l].lock = ln[l].lock; S.l[l].dens = ln[l].dens; S.l[l].oct = ln[l].oct; S.l[l].filt = (int8_t)fxu[l].filt;
        S.l[l].res = (uint8_t)fxu[l].res; S.l[l].wob = (uint8_t)fxu[l].wob; S.l[l].bright = (uint8_t)fxu[l].bright; S.l[l].drive = (uint8_t)fxu[l].drive;
        S.l[l].slide = (uint8_t)fxu[l].slide; S.l[l].retrig = (uint8_t)fxu[l].retrig; S.l[l].rate = (uint8_t)fxu[l].rtg_rate; S.l[l].crush = (uint8_t)fxu[l].crush;
        S.l[l].echo = (uint8_t)fxu[l].echo; S.l[l].level = (uint8_t)fxu[l].level; S.l[l].inst = (int8_t)fxu[l].inst; S.l[l].vary = (uint8_t)fxu[l].vary;
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
    if (settings_load(&S, sizeof S) == 0 && S.ver == 5) {
        energy = clampi(S.energy, -50, 50); space = clampi(S.space, 0, 100);
        set_key(S.key % 12u); scale_sel = S.scale < NSCALE ? S.scale : 0; prog_sel = S.prog <= NPROG ? S.prog : 0; bpc = clampi(S.bpc, 1, 4);
        arc_on = S.arc != 0; build_ph = clampi(S.build, 2, 16); if (arc_on) arc_restart(0); peak_ph = clampi(S.peak, 1, 8); auto_drop = S.autod != 0; target = S.target % NPROF; set_bpm(S.bpm);
        for (l = 0; l < (int32_t)NLANE; l++) {
            ln[l].prof = (uint8_t)(S.l[l].prof % NPROF); ln[l].lock = S.l[l].lock != 0; ln[l].dens = (uint8_t)clampi(S.l[l].dens, 0, 100); ln[l].oct = (int8_t)clampi(S.l[l].oct, -1, 1);
            fxu[l].filt = clampi(S.l[l].filt, -100, 100); fxu[l].res = clampi(S.l[l].res, 0, 100); fxu[l].wob = clampi(S.l[l].wob, 0, 5); fxu[l].bright = clampi(S.l[l].bright, 0, 100);
            fxu[l].drive = clampi(S.l[l].drive, 0, 100); fxu[l].slide = clampi(S.l[l].slide, 0, 100); fxu[l].retrig = clampi(S.l[l].retrig, 0, 100); fxu[l].rtg_rate = clampi(S.l[l].rate, 1, 3);
            fxu[l].crush = clampi(S.l[l].crush, 0, 100); fxu[l].echo = clampi(S.l[l].echo, 0, 100); fxu[l].level = clampi(S.l[l].level, 0, 100);
            fxu[l].vary = clampi(S.l[l].vary, 0, 100);
            fxu[l].inst = clampi(S.l[l].inst, -1, l == L_SFX ? (int32_t)NSX - 1 : l == L_DRUMS ? (int32_t)NKIT - 1 : (int32_t)NPATCH - 1);
        }
    }
    if (patterns_load(&P, sizeof P) == 0)
        for (l = 0; l < (int32_t)NLANE; l++) { ln[l].up = P.p[l]; if (ln[l].up.bars < 1 || ln[l].up.bars > 2) ln[l].up.bars = 1; }
}

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
/* a page button: from MAIN it opens the last channel; again: the next page (FX: FX 2) or back */
static void set_page(int32_t p)
{
    if (p == PG_SONG) {
        if (page == PG_SONG) page = PG_SONG2;
        else if (page == PG_SONG2) page = back_page;
        else { back_page = page; page = PG_SONG; }
    } else {
        if (!view) view = sel + 1;
        if (p == PG_FX) page = page == PG_FX ? PG_FX2 : page == PG_FX2 ? PG_HOME : PG_FX;
        else page = page == p ? PG_HOME : p;
    }
    area_dirty = 1;
}
static void edit_key(int32_t k, int32_t sel_held_now)
{
    upat_t *U = &ln[sel].up;
    int32_t n = U->bars * 16, wi = white_index(k);
    if (HITS(sel)) {
        int32_t nk = sel == L_DRUMS ? NDK : NSX;
        if (wi >= 0 && wi < nk) { U->kinds[cursor] ^= (uint8_t)(1u << wi); if (!seq_playing) audition((uint32_t)sel, (uint32_t)wi, sel == L_SFX ? 0u : fxu[L_DRUMS].inst >= 0 ? (uint32_t)fxu[L_DRUMS].inst : PROF[ln[L_DRUMS].prof].inst[L_DRUMS]); }
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
            audition((uint32_t)sel, nt, fxu[sel].inst >= 0 ? (uint32_t)fxu[sel].inst : PROF[ln[sel].prof].inst[sel]);
        }
        cursor = (cursor + entry_len) % n;
        U->on = 1; invalidate_now();
    }
}

/* ---- the keys play: the channel in view (MAIN: the last one), F3..G5 plus OCT- / OCT+; drums and sfx: their sounds
 * on the white keys. On the EDIT page they enter steps. */
static void keys_play(uint32_t pressed, uint32_t released)
{
    int32_t k, l = sel;
    for (k = 0; k < 27; k++) {
        if (released >> k & 1u) { live_push((uint32_t)l, 0, (uint32_t)k, 0, 0); keys_down &= ~(1u << k); }
        if (!(pressed >> k & 1u)) continue;
        keys_down |= 1u << k;
        if (HITS(l)) {
            int32_t wi = white_index(k);
            if (wi < 0 || wi >= (l == L_DRUMS ? NDK : NSX)) continue;
            live_push((uint32_t)l, (uint32_t)wi, (uint32_t)k, 1, (uint32_t)inst_eff(l));
        } else live_push((uint32_t)l, (uint32_t)clampi(53 + k + 12 * kb_oct, 0, 119), (uint32_t)k, 1, (uint32_t)inst_eff(l));
    }
}
static void input(void)
{
    uint32_t rel, b = fm1_input_edges(&rel), keys = fm1_input_note_edges(), now = fm1_in.notes;
    int32_t e, k, held = (fm1_in.buttons >> BTN_SEL) & 1u;
    static uint32_t keys_was;
    for (e = 0; e < 7; e++) {
        int32_t d = fm1_enc_take((uint32_t)e);
        if (!d) continue;
        if (held) d *= 10;
        if (e == ENC_SELECT) { set_bpm(seq_bpm + d); area_dirty = 1; continue; }              /* SELECT: tempo */
        if (e == ENC_ALGORITHM) { if (page >= PG_SONG) page = back_page; go_view(view + (d > 0 ? 1 : -1)); continue; }   /* the channel */
        if (e == ENC_PRESETS) {                                         /* the sound: MAIN the queued profile */
            if (!view || page >= PG_SONG) t_prof(d); else t_inst(d);
            area_dirty = 1; continue;
        }
        {
            const card_t *C = card_at(e - ENC_KNOB1);
            if (!C->name) continue;
            C->turn(d);
            last_enc = e; last_enc_ms = fm1_ms;
            area_dirty = 1;
        }
    }
    if (b & (1u << BTN_PLAY)) start_stop(held);
    if (b & (1u << BTN_HOME)) {                                     /* MAIN, LEAD, PAD, BASS, DRUMS, SFX, MAIN.. */
        if (held) { go_view(0); page = PG_HOME; }
        else if (page >= PG_SONG) { page = back_page; area_dirty = 1; }
        else { int32_t pg = page; go_view(view + 1); page = view ? pg : PG_HOME; }
    }
    if (b & (1u << BTN_FX)) set_page(PG_FX);
    if (b & (1u << BTN_ARP)) set_page(PG_TRK);
    if (b & (1u << BTN_EDIT)) { if (held && page == PG_EDIT) { int32_t i; for (i = 0; i < 32; i++) { ln[sel].up.k[i] = EMPTY; ln[sel].up.kinds[i] = 0; } cursor = 0; toast("PATTERN CLEARED"); invalidate_now(); } else set_page(PG_EDIT); }
    if (b & (1u << BTN_GLO)) set_page(PG_SONG);
    if (b & (1u << BTN_SEQ)) {
        if (launch_bar >= 0) { launch_bar = -1; toast("LAUNCH CANCELLED"); }
        else { launch_bar = (cur_bar() / 4 + 1) * 4; toast("LAUNCH ON THE NEXT PHRASE"); }
        invalidate();
    }
    if (b & (1u << BTN_REC)) {
        if (page == PG_EDIT && view) { if (held) { int32_t i; for (i = 0; i < 32; i++) { ln[sel].up.k[i] = EMPTY; ln[sel].up.kinds[i] = 0; } toast("PATTERN CLEARED"); invalidate_now(); } else { pat_grab(sel); toast("GRABBED THIS BAR"); } }
        else if (!seq_playing) toast("DROP: PRESS PLAY FIRST");
        else { drop_press(sel); { char t[40], *o = s_cat(t, "BREAK: "); o = s_cat(o, LNAME[sel]); s_cat(o, " ALONE"); toast(t); } }
    }
    if (rel & (1u << BTN_REC)) drop_release();
    if (b & (1u << BTN_ENV) && view) {
        if (page == PG_EDIT) edit_key(1, 1);                        /* tie (key 1 = F#3, a black key) */
        else if (sel != L_SFX) { ln[sel].lock = (uint8_t)!ln[sel].lock; toast(ln[sel].lock ? "LOCKED: KEEPS ITS PROFILE" : "UNLOCKED"); invalidate(); }
    }
    if (b & (1u << BTN_LFO)) {
        if (!view) toast("OPEN A CHANNEL: HOME");
        else {
            int32_t n = pat_generate(sel, held);
            char t[40], *o;
            if (n) { o = s_cat(t, LNAME[sel]); o = s_cat(o, held ? ": NEW PATTERN #" : ": VARIATION #"); s_int(o, n - 1); toast(t); gen_count[sel] = n; }
            else toast("NO NEW ONE: SEL+LFO = FRESH");
        }
    }
    {                                                               /* OCT- / OCT+: the keys' octave (EDIT: the entry octave); both: 0 */
        uint32_t both = (1u << BTN_OCT_DOWN) | (1u << BTN_OCT_UP);
        if ((fm1_in.buttons & both) == both && b & both) { kb_oct = 0; entry_oct = 0; }
        else if (b & (1u << BTN_OCT_DOWN)) { if (page == PG_EDIT) entry_oct = clampi(entry_oct - 1, -1, 1); else kb_oct = clampi(kb_oct - 1, -2, 2); }
        else if (b & (1u << BTN_OCT_UP)) { if (page == PG_EDIT) entry_oct = clampi(entry_oct + 1, -1, 1); else kb_oct = clampi(kb_oct + 1, -2, 2); }
        if (b & both) area_dirty = 1;
    }
    if (b & (1u << BTN_SAVE)) {
        if (held) { debug_on = !debug_on; isr_us_max = 0; }
        else if (seq_playing) { save_pending = 1; toast("SAVES WHEN YOU STOP"); }
        else do_save();
    }
    if (page == PG_EDIT && view) {
        for (k = 0; k < 27; k++) if (keys >> k & 1u) { edit_key(k, held); area_dirty = 1; }
    } else keys_play(keys, keys_was & ~now);
    keys_was = now;
}

/* ---- the lights: every button and key glows; lit: where you are and what is on */
static void leds(void)
{
    static uint32_t last = 0xFFFFFFFFu, lastk = 0xFFFFFFFFu;
    uint32_t m = 0, km = 0, k;
    arc_t A = arc_at(cur_bar());
    m |= (uint32_t)seq_playing << BTN_PLAY;
    m |= (uint32_t)(A.mode == A_BREAK || drop_seg >= 0) << BTN_REC;
    m |= (uint32_t)(page < PG_SONG) << BTN_HOME | (uint32_t)(page == PG_FX || page == PG_FX2) << BTN_FX | (uint32_t)(page == PG_TRK) << BTN_ARP;
    m |= (uint32_t)(page == PG_EDIT) << BTN_EDIT | (uint32_t)(page >= PG_SONG) << BTN_GLO;
    m |= (uint32_t)(launch_bar >= 0) << BTN_SEQ | (uint32_t)(view && ln[sel].lock && sel != L_SFX) << BTN_ENV | (uint32_t)save_pending << BTN_SAVE;
    m |= (uint32_t)(page == PG_EDIT ? entry_oct < 0 : kb_oct < 0) << BTN_OCT_DOWN | (uint32_t)(page == PG_EDIT ? entry_oct > 0 : kb_oct > 0) << BTN_OCT_UP;
    m |= (uint32_t)debug_on << BTN_SEL;
    if (page == PG_EDIT && view) {
        const upat_t *U = &ln[sel].up;
        for (k = 0; k < 27u; k++) {
            int32_t wi = white_index((int32_t)k);
            if (HITS(sel)) { if (wi >= 0 && wi < (sel == L_DRUMS ? NDK : NSX) && (U->kinds[cursor] >> wi & 1u)) km |= 1u << k; }
            else if (wi >= 0 && U->k[cursor] != EMPTY && wi - 4 + 7 * entry_oct == U->k[cursor]) km |= 1u << k;
        }
    } else {
        for (k = 0; k < 27u; k++) if (!HITS(sel) && (53u + k + 120u + 12u * (uint32_t)(kb_oct + 2)) % 12u == (uint32_t)key) km |= 1u << k;   /* the tonic */
        km |= keys_down;                                            /* and what you hold */
    }
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

/* the boot, then the main loop a pass at a time (the browser build runs the passes as its audio asks for time) */
static uint32_t t_frame;
static void app_boot(void)
{
    uint32_t t_splash;
    synth_init();
    music_init();
    { uint32_t l; for (l = 0; l < NLANE; l++) fxu[l] = fx[l]; fxu[L_LEAD].vary = 40; }
    set_key(2);
    set_bpm(125);
    do_load();
    splash();
    t_splash = fm1_ms;
    while (fm1_ms - t_splash < 1500u) platform_poll();
    lcd_fill(0, 0, 240, 240, C_BG);
    fm1_glow_all();
}
static void app_pass(void)
{
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
    fx_apply();
    bars_update();
    bars_update();
    if (save_pending && !seq_playing) { save_pending = 0; do_save(); }
    leds();
    if (fm1_ms - t_frame >= 33u || area_dirty) { t_frame = fm1_ms; draw_frame(area_dirty); }
}
static void app_main(void)
{
    app_boot();
    for (;;) app_pass();
}
