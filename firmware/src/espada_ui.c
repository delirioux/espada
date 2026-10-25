/* SPDX-License-Identifier: GPL-3.0-only */
/* Espada's screen and panel: one overview, a mixing desk of settings.
 *
 *   header 0..20     the group (PLAY, SOUND, TRACKER, SONG; EDIT and its lane), the bar in the phrase, the tempo
 *   title 22..58     the setting in large letters, its place in the group, the status (launch, break, next)
 *   lanes 58..148    all five lanes, always (SONG's KEY and ARC rows: the chords, the arc; EDIT: the step grid)
 *   knobs 148..240   KNOB 1..4: the setting for LEAD, PAD, BASS, DRUMS (SONG: four song or sfx settings)
 *
 * A group is a button: HOME play, FX sound, ARP tracker effects, GLO song and sfx; pressed again, the next
 * setting. PRESETS steps through the group's settings, ALGORITHM picks the lane the buttons act on (turning a
 * lane's knob picks it too), SELECT the tempo. SEL held: steps of 10.
 * Buttons: EDIT the lane's steps (again: back), PLAY start/stop (SEL+PLAY: from the top), SEQ launch, REC drop
 * (tap or hold; EDIT: grab, SEL+REC: clear), ENV fade the lane out and in (SEL+ENV: lock; EDIT: tie), LFO a new
 * idea (EDIT: generate, SEL+LFO: fresh), OCT-/OCT+ the keys' octave (both: reset), SAVE (SEL+SAVE: the audio
 * interrupt's time).
 * Keys: play the lane; on EDIT they enter steps. Never settings. */

#define RGBC(h) RGB(((h) >> 16) & 255, ((h) >> 8) & 255, (h) & 255)
#define SW(c) ((uint16_t)(((c) >> 8) | ((c) << 8)))
static const uint16_t LCOL[NLANE] = {RGBC(0x5fe3e8), RGBC(0xb49bff), RGBC(0xffb04a), RGBC(0xf0e07a), RGBC(0xff7a9a), RGBC(0x7ee88c)};
static const char *const LNAME[NLANE] = {"LEAD", "PAD", "BASS", "DRUMS", "SFX", "ARP"};
static const uint8_t LROW[NLANE] = {L_LEAD, L_ARP, L_PAD, L_BASS, L_DRUMS, L_SFX};   /* top to bottom on the screen */
static const char *const SXNAME[NSX] = {"WASH", "RISER", "IMPACT", "ZAP", "CRACKLE", "BITS", "SURF", "GALE"};
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
#define HEAD_H 20                   /* the header: the group, the tempo, the bar in the phrase */
#define TITLE_Y 22                  /* the setting, large */
#define TITLE_H 36
#define LANE_Y 58                   /* six lanes of 14 + 1 */
#define LH 14
#define GAP 1
#define LANES_H 90
#define KNOB_Y 148                  /* KNOB 1..4, as the knobs sit on the panel */
#define KNOB_H 92
#define COL_W 60
static int32_t pan_y = LANE_Y, pan_h = LANES_H;    /* where the step grid, the song and the arc draw */

/* ---- the look (SETUP): a palette (ESPADA in its colours, or one colour in shades, as Felucca's) and the screen's
 * level. Every colour drawn passes through paint(). */
static const char *const PALN[6] = {"ESPADA", "AMBER", "GREEN", "CYAN", "RED", "MONO"};
static const uint8_t PALC[6][3] = {{0, 0, 0}, {255, 166, 40}, {120, 255, 146}, {140, 222, 255}, {255, 112, 92}, {226, 226, 226}};
static int32_t palette, screen_lvl = 100, glow_on = 1;
static uint16_t paint(uint16_t c)
{
    int32_t r = (c >> 11) << 3, g = ((c >> 5) & 63) << 2, b = (c & 31) << 3;
    if (palette > 0 && palette < 6) {
        int32_t y = (r * 77 + g * 150 + b * 29) >> 8;
        r = PALC[palette][0] * y / 255; g = PALC[palette][1] * y / 255; b = PALC[palette][2] * y / 255;
    }
    if (screen_lvl < 100) { r = r * screen_lvl / 100; g = g * screen_lvl / 100; b = b * screen_lvl / 100; }
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/* ---- the strip canvas */
#define STRIP_H 46u
static uint16_t cvb[2][240u * STRIP_H] __attribute__((section(".pool")));
static uint16_t *cvp;
static int32_t cy0, chh;
static uint32_t cvn;
static void cs_begin(int32_t y0, int32_t h, uint16_t bg)
{
    uint32_t i, n = 240u * (uint32_t)h;
    uint16_t s = SW(paint(bg));
    cvp = cvb[cvn++ & 1u]; cy0 = y0; chh = h;
    for (i = 0; i < n; i++) cvp[i] = s;
}
static void cs_end(void) { lcd_blit(0, (uint32_t)cy0, 240, (uint32_t)chh, cvp); }
static void fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t i, j, x1 = x + w, y1 = y + h;
    uint16_t s = SW(paint(c));
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
    uint16_t sw = SW(paint(c));
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

/* ---- UI state: the group, the setting in each group, the lane the buttons act on, EDIT */
enum { G_PLAY, G_SOUND, G_TRK, G_SONG, NGRP };
static const char *const GNAME[NGRP] = {"PLAY", "SOUND", "TRACKER", "SONG"};
static const uint16_t GCOL[NGRP] = {RGBC(0xefe6c8), RGBC(0x9ad04a), RGBC(0x6aa0ff), RGBC(0x3fe0e0)};
static int32_t grp = G_PLAY, rowi[NGRP], edit_on, sel = L_LEAD, debug_on;
static int32_t last_enc = -1, area_dirty = 1, kb_oct;
static uint32_t last_enc_ms, keys_down;
static int32_t cursor, entry_len = 1, entry_oct;
static char toast_s[48];
static uint32_t toast_until;
static int32_t save_pending, gen_count[NLANE];
static void toast(const char *s) { uint32_t i; for (i = 0; i < 47u && s[i]; i++) toast_s[i] = s[i]; toast_s[i] = 0; toast_until = fm1_ms + 1600u; }
static const char *const NN[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const char *const WOBN[6] = {"OFF", "1/1", "1/2", "1/4", "1/8", "1/16"};
static const char *const DKN[NDK] = {"K", "S", "H", "T", "C", "X", "O", "M", "R"};   /* EDIT rows: kick, snare, hat, timpani, crash, clap, open hat, tom, rim */
static const char *const PSHORT[10] = {"EPIC", "HYPER", "FUSION", "OLDSKOOL", "SYMPHONY", "TROPIC", "STEEL", "DRIFT", "FUNK", "RAVE"};
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
/* EDIT reaches your part's bars and the one right after it (writing there makes the part a bar longer), up to MAXB */
static int32_t nav_bars(int32_t l) { return ln[l].up.bars < MAXB ? ln[l].up.bars + 1 : MAXB; }
static int32_t nav_steps(int32_t l) { return nav_bars(l) * 16; }
static void set_sel(int32_t l)
{
    if (l == sel) return;
    sel = fmodp(l, NLANE);
    cursor = clampi(cursor, 0, nav_steps(sel) - 1);
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
static void v_filt(char *o) { int32_t f = fxu[sel].filt; if (!f) s_cat(o, "OFF"); else s_pct(o, f < 0 ? -f : f); }
static void s_filt(char *o) { int32_t f = fxu[sel].filt; s_cat(o, f < 0 ? "LOWPASS" : f > 0 ? "HIGHPASS" : ""); }
static void t_filt(int32_t d) { fxu[sel].filt = clampi(fxu[sel].filt + d, -100, 100); }
static void t_res(int32_t d) { fxu[sel].res = clampi(fxu[sel].res + d, 0, 100); }
static void v_res(char *o) { s_pct(o, fxu[sel].res); }
static void t_wob(int32_t d) { fxu[sel].wob = clampi(fxu[sel].wob + (d > 0 ? 1 : -1), 0, 5); }
static void v_wob(char *o) { s_cat(o, WOBN[fxu[sel].wob]); }
static void t_bright(int32_t d) { if (!HITS(sel)) fxu[sel].bright = clampi(fxu[sel].bright + d, 0, 100); }
static void v_bright(char *o) { if (HITS(sel)) s_cat(o, "--"); else s_pct(o, fxu[sel].bright); }
static void t_drive(int32_t d) { fxu[sel].drive = clampi(fxu[sel].drive + d, 0, 100); }
static void v_drive(char *o) { s_pct(o, fxu[sel].drive); }
static void t_send(int32_t d) { fxu[sel].send = clampi(fxu[sel].send + d, 0, 100); }
static void v_send(char *o) { s_pct(o, fxu[sel].send); }
static void t_vary(int32_t d) { fxu[sel].vary = clampi(fxu[sel].vary + d, 0, 100); }
static void v_vary(char *o) { s_pct(o, fxu[sel].vary); }
static void s_vary(char *o) { if (fxu[sel].vary) s_cat(o, VARYN[vary_char[sel]]); }
static void t_level(int32_t d) { fxu[sel].level = clampi(fxu[sel].level + d, 0, 100); }
static void v_level(char *o) { s_pct(o, fxu[sel].level); }
/* the tracker's effects: PORTA (1xx/2xx), VIBRATO (4xy), TREMOLO (7xy), TREMOR (Ixy), GATE (ECx), SWING (EDx), PAN (8xx) */
static void t_porta(int32_t d) { if (!HITS(sel)) fxu[sel].porta = clampi(fxu[sel].porta + d, -100, 100); }
static void v_porta(char *o) { if (HITS(sel)) s_cat(o, "--"); else s_int(o, fxu[sel].porta); }
static void s_porta(char *o) { if (!HITS(sel)) s_cat(o, fxu[sel].porta > 0 ? "UP" : fxu[sel].porta < 0 ? "DOWN" : "OFF"); }
static void t_vibr(int32_t d) { if (!HITS(sel)) fxu[sel].vibr = clampi(fxu[sel].vibr + d, 0, 100); }
static void v_vibr(char *o) { if (HITS(sel)) s_cat(o, "--"); else s_pct(o, fxu[sel].vibr); }
static void t_trem(int32_t d) { fxu[sel].trem = clampi(fxu[sel].trem + d, 0, 100); }
static void v_trem(char *o) { s_pct(o, fxu[sel].trem); }
static void s_trem(char *o) { if (fxu[sel].trem) s_cat(o, "8THS"); }
static void t_tremor(int32_t d) { fxu[sel].tremor = clampi(fxu[sel].tremor + (d > 0 ? 1 : -1), 0, 3); }
static void v_tremor(char *o) { static const char *const N[4] = {"OFF", "8THS", "16THS", "32NDS"}; s_cat(o, N[fxu[sel].tremor & 3]); }
static void t_gate(int32_t d) { if (!HITS(sel)) fxu[sel].gate = clampi(fxu[sel].gate + d, 0, 100); }
static void v_gate(char *o) { if (HITS(sel)) s_cat(o, "--"); else s_pct(o, fxu[sel].gate); }
static void s_gate(char *o) { if (!HITS(sel) && fxu[sel].gate) s_cat(o, "SHORTER"); }
static void t_swing(int32_t d) { fxu[sel].swing = clampi(fxu[sel].swing + d, 0, 100); }
static void v_swing(char *o) { s_pct(o, fxu[sel].swing); }
static void t_pan(int32_t d) { fxu[sel].pan = clampi(fxu[sel].pan + d, -100, 100); }
static void v_pan(char *o) { s_int(o, fxu[sel].pan); }
static void s_pan(char *o) { s_cat(o, fxu[sel].pan > 0 ? "RIGHT" : fxu[sel].pan < 0 ? "LEFT" : "MIDDLE"); }
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
static void t_step(int32_t d) { cursor = clampi(cursor + d, 0, nav_steps(sel) - 1); area_dirty = 1; }
static void v_step(char *o) { s_int(o, (cursor & 15) + 1); }
static void s_step(char *o) { s_cat(o, "OF 16"); }
static void t_len(int32_t d) { entry_len = clampi(entry_len + (d > 0 ? 1 : -1), 1, 16); }
static void v_len(char *o) { s_int(o, entry_len); }
static void t_bar(int32_t d) { int32_t b = clampi((cursor >> 4) + (d > 0 ? 1 : -1), 0, nav_bars(sel) - 1); cursor = b * 16 + (cursor & 15); area_dirty = 1; }
static void v_bar(char *o) { s_int(o, (cursor >> 4) + 1); }
static void s_bar(char *o)
{
    int32_t b = cursor >> 4, n = ln[sel].up.bars;
    if (b >= n) s_cat(o, "NEW");
    else if (n >= MAXB && b == n - 1) s_cat(o, "FULL");
    else { o = s_cat(o, "OF "); s_int(o, n); }
}
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
static void t_arc(int32_t d) { int32_t was = arc_on; arc_on = d > 0; if (arc_on && !was) arc_restart(cur_bar() + 1); if (seq_playing) invalidate(); else form_arm(); }
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
/* the four musical knobs of a channel (as Tuna's): MELODY which idea, RHYTHM sparse..busy, PITCH narrow..wide and
 * high, CHANGE how soon it brings new material; at once, from the next step */
static void t_mel(int32_t d) { ln[sel].mel = (uint8_t)fmodp(ln[sel].mel + d, 100); invalidate(); }
static void v_mel(char *o) { s_int(o, ln[sel].mel); }
static void t_rhy(int32_t d) { ln[sel].rhy = (uint8_t)clampi(ln[sel].rhy + d, 0, 100); invalidate(); }
static void v_rhy(char *o) { s_pct(o, ln[sel].rhy); }
static void s_rhy(char *o) { s_cat(o, ln[sel].rhy < 34 ? "SLOW" : ln[sel].rhy < 67 ? "MIDDLE" : "BUSY"); }
static void t_pit(int32_t d) { ln[sel].pit = (uint8_t)clampi(ln[sel].pit + d, 0, 100); invalidate(); }
static void v_pit(char *o) { s_pct(o, ln[sel].pit); }
static void s_pit(char *o)
{
    if (HITS(sel)) { s_cat(o, ln[sel].pit > 60 ? "TIMPANI" : ""); return; }       /* the drums: timpani on the off-beats */
    s_cat(o, ln[sel].pit < 34 ? "STEPS" : ln[sel].pit < 67 ? "LEAPS" : ln[sel].pit > 80 ? "HIGH" : "WIDE");
}
static void t_chg(int32_t d) { ln[sel].chg = (uint8_t)clampi(ln[sel].chg + d, 0, 100); invalidate(); }
static void v_chg(char *o) { s_pct(o, ln[sel].chg); }
static void s_chg(char *o) { s_cat(o, ln[sel].chg < 10 ? "FROZEN" : ln[sel].chg < 40 ? "64 BARS" : ln[sel].chg < 70 ? "32 BARS" : "16 BARS"); }
static void t_auto(int32_t d) { auto_drop = d > 0; invalidate(); }
static void v_auto(char *o) { s_cat(o, auto_drop ? "ON" : "OFF"); }
/* EDIT's second row: FILL (the style LFO fills the lane from; OWN = the lane's) and COPY (the lane SEQ copies from) */
static int32_t copy_src = L_LEAD;
static void t_fill(int32_t d) { fill_style = fmodp(fill_style + 1 + (d > 0 ? 1 : -1), NPROF + 1) - 1; }
static void v_fill(char *o) { s_cat(o, fill_style < 0 ? "OWN" : PSHORT[fill_style]); }
static void s_fill(char *o) { s_cat(o, "BY LFO"); }
static void t_csrc(int32_t d) { copy_src = fmodp(copy_src + (d > 0 ? 1 : -1), NLANE); }
static void v_csrc(char *o) { s_cat(o, LNAME[copy_src]); }
static void s_csrc(char *o) { s_cat(o, ln[copy_src].up.on ? "BY SEQ" : "NO PART"); }
/* AFTER: what plays past your last bar; REPEAT: the bar before into this one, then on to the next (turn right) */
static void t_after(int32_t d) { ln[sel].up.after = (uint8_t)fmodp(ln[sel].up.after + (d > 0 ? 1 : -1), NAF); ln[sel].up.ver++; invalidate_now(); }
static void v_after(char *o) { static const char *const N[NAF] = {"LOOP", "PROFILE", "MY STYLE"}; s_cat(o, N[ln[sel].up.after % NAF]); }
static void s_after(char *o) { o = s_cat(o, "AFTER "); s_int(o, ln[sel].up.bars); }
static void t_rep(int32_t d)
{
    int32_t b = cursor >> 4;
    if (d <= 0) return;
    if (b < 1) { toast("REPEAT: FROM BAR 2 ON"); return; }
    if (!pat_repeat(sel, b)) { toast("FULL"); return; }
    if (b + 1 < nav_bars(sel)) cursor = (b + 1) * 16 + (cursor & 15);
    area_dirty = 1;
}
static void v_rep(char *o) { s_cat(o, "TURN"); }
static void s_rep(char *o) { int32_t b = cursor >> 4; if (b < 1) s_cat(o, "FROM BAR 2"); else { o = s_cat(o, "BAR "); s_int(o, b); } }


/* ---- the settings. A lane group's setting is one row of the desk: KNOB 1..4 set it for LEAD, PAD, BASS, DRUMS.
 * SONG's rows are four settings each, of the song or of the sfx lane. The functions act on `sel`: drawing sets it
 * for the column and puts it back, turning a lane's knob leaves it there (the lane you touch is the lane). */
static const card_t ROW_PLAY[] = {
    {"MELODY", t_mel, v_mel, 0}, {"RHYTHM", t_rhy, v_rhy, s_rhy}, {"PITCH", t_pit, v_pit, s_pit}, {"CHANGE", t_chg, v_chg, s_chg},
    {"DENSITY", t_dens, v_dens, 0}, {"OCTAVE", t_oct, v_oct, 0}, {"STYLE", t_lprof, v_lprof, 0}, {"SOUND", t_inst, v_inst, s_inst},
    {"LEVEL", t_level, v_level, 0}};
static const card_t ROW_SOUND[] = {
    {"FILTER", t_filt, v_filt, s_filt}, {"RESO", t_res, v_res, 0}, {"WOBBLE", t_wob, v_wob, 0}, {"DRIVE", t_drive, v_drive, 0},
    {"BRIGHT", t_bright, v_bright, 0}, {"CRUSH", t_crush, v_crush, 0}, {"VARY", t_vary, v_vary, s_vary}, {"REVERB", t_send, v_send, 0}};
static const card_t ROW_TRK[] = {
    {"SLIDE", t_slide, v_slide, 0}, {"RETRIG", t_rtg, v_rtg, 0}, {"RATE", t_rate, v_rate, 0}, {"ECHO", t_echo, v_echo, 0},
    {"PORTA", t_porta, v_porta, s_porta}, {"VIBRATO", t_vibr, v_vibr, 0}, {"TREMOLO", t_trem, v_trem, s_trem}, {"TREMOR", t_tremor, v_tremor, 0},
    {"GATE", t_gate, v_gate, s_gate}, {"SWING", t_swing, v_swing, 0}, {"PAN", t_pan, v_pan, s_pan}};
typedef struct { const char *name; int8_t lane; card_t c[4]; } srow_t;     /* lane: -1 the song, else that lane's */
/* the arp lane's settings, as heard: the pattern MELODY gives, the step RHYTHM gives, the octaves PITCH gives */
static void s_apat(char *o)
{
    static const char *const N[NAP] = {"UP", "DOWN", "UP-DOWN", "ALBERTI", "BROKEN", "RANDOM"};
    s_cat(o, N[(PROF[ln[L_ARP].prof].arp[0] + ln[L_ARP].mel) % NAP]);
}
static void s_astep(char *o)
{
    int32_t st = PROF[ln[L_ARP].prof].arp[1];
    if (ln[L_ARP].rhy >= 75 && st > 1) st /= 2; else if (ln[L_ARP].rhy < 25 && st < 4) st *= 2;
    s_cat(o, st == 1 ? "16THS" : st == 2 ? "8THS" : "QUARTERS");
}
static void s_aoct(char *o)
{
    int32_t oc = PROF[ln[L_ARP].prof].arp[2] + (ln[L_ARP].pit >= 75) - (ln[L_ARP].pit < 25);
    oc = oc < 1 ? 1 : oc > 3 ? 3 : oc;
    o = s_int(o, oc); s_cat(o, oc == 1 ? " OCTAVE" : " OCTAVES");
}
enum { SR_LIVE, SR_KEY, SR_ARC, SR_SFX };
static const srow_t ROW_SONG[] = {
    {"LIVE", -1, {{"STYLE", t_prof, v_prof, 0}, {"BLEND", t_blend, v_blend, s_blend}, {"ENERGY", t_energy, v_energy, 0}, {"SPACE", t_space, v_space, 0}}},
    {"KEY", -1, {{"KEY", t_key, v_key, 0}, {"SCALE", t_scale, v_scale, 0}, {"CHORDS", t_prog, v_prog, 0}, {"BARS/CH", t_bpc, v_bpc, 0}}},
    {"ARC", -1, {{"ARC", t_arc, v_arc, 0}, {"BUILD", t_build, v_build, s_phr}, {"PEAK", t_peak, v_peak, s_phr}, {"AUTODROP", t_auto, v_auto, 0}}},
    {"SFX", L_SFX, {{"SOUND", t_inst, v_inst, 0}, {"RHYTHM", t_rhy, v_rhy, s_rhy}, {"CHANGE", t_chg, v_chg, s_chg}, {"LEVEL", t_level, v_level, 0}}},
    {"SFX PART", L_SFX, {{"STYLE", t_lprof, v_lprof, 0}, {"DENSITY", t_dens, v_dens, 0}, {"MELODY", t_mel, v_mel, 0}, {"VARY", t_vary, v_vary, s_vary}}},
    {"SFX FX", L_SFX, {{"FILTER", t_filt, v_filt, s_filt}, {"RESO", t_res, v_res, 0}, {"WOBBLE", t_wob, v_wob, 0}, {"DRIVE", t_drive, v_drive, 0}}},
    {"SFX FX 2", L_SFX, {{"CRUSH", t_crush, v_crush, 0}, {"ECHO", t_echo, v_echo, 0}, {"RETRIG", t_rtg, v_rtg, 0}, {"RATE", t_rate, v_rate, 0}}},
    {"SFX ROOM", L_SFX, {{"REVERB", t_send, v_send, 0}, {"PAN", t_pan, v_pan, s_pan}, {"TREMOLO", t_trem, v_trem, s_trem}, {"TREMOR", t_tremor, v_tremor, 0}}},
    {"ARP", L_ARP, {{"SOUND", t_inst, v_inst, s_inst}, {"MELODY", t_mel, v_mel, s_apat}, {"RHYTHM", t_rhy, v_rhy, s_astep}, {"LEVEL", t_level, v_level, 0}}},
    {"ARP PART", L_ARP, {{"STYLE", t_lprof, v_lprof, 0}, {"PITCH", t_pit, v_pit, s_aoct}, {"DENSITY", t_dens, v_dens, 0}, {"CHANGE", t_chg, v_chg, s_chg}}},
    {"ARP FX", L_ARP, {{"FILTER", t_filt, v_filt, s_filt}, {"RESO", t_res, v_res, 0}, {"WOBBLE", t_wob, v_wob, 0}, {"DRIVE", t_drive, v_drive, 0}}},
    {"ARP FX 2", L_ARP, {{"ECHO", t_echo, v_echo, 0}, {"SLIDE", t_slide, v_slide, 0}, {"CRUSH", t_crush, v_crush, 0}, {"REVERB", t_send, v_send, 0}}},
    {"ARP MIX", L_ARP, {{"PAN", t_pan, v_pan, s_pan}, {"GATE", t_gate, v_gate, s_gate}, {"SWING", t_swing, v_swing, 0}, {"TREMOR", t_tremor, v_tremor, 0}}},
};
static const card_t ROW_EDIT[2][4] = {
    {{"STEP", t_step, v_step, s_step}, {"LENGTH", t_len, v_len, 0}, {"BAR", t_bar, v_bar, s_bar}, {"PATTERN", t_use, v_use, 0}},
    {{"FILL", t_fill, v_fill, s_fill}, {"COPY", t_csrc, v_csrc, s_csrc}, {"AFTER", t_after, v_after, s_after}, {"REPEAT", t_rep, v_rep, s_rep}}};
static int32_t edit_row;
/* SETUP (hold HOME): the look, the MIDI channels, the defaults */
static int32_t setup_on, setup_row, reset_turns;
static void defaults(void);
static void t_pal(int32_t d) { palette = fmodp(palette + (d > 0 ? 1 : -1), 6); area_dirty = 1; }
static void v_pal(char *o) { s_cat(o, PALN[palette]); }
static void t_scr(int32_t d) { screen_lvl = clampi(screen_lvl + (d > 0 ? 25 : -25), 25, 100); area_dirty = 1; }
static void v_scr(char *o) { s_pct(o, screen_lvl); }
static void t_glow(int32_t d) { glow_on = d > 0; if (glow_on) fm1_glow_all(); else fm1_glow_none(); }
static void v_glow(char *o) { s_cat(o, glow_on ? "GLOW" : "OFF"); }
static void s_glow(char *o) { s_cat(o, "THE KEYS"); }
static void t_ch(int32_t l, int32_t d) { midi_chan[l] = (uint8_t)clampi(midi_chan[l] + (d > 0 ? 1 : -1), 0, 16); }
static void v_ch(char *o, int32_t l) { if (midi_chan[l] > 15u) s_cat(o, "OFF"); else s_int(o, midi_chan[l] + 1); }
static void t_ch0(int32_t d) { t_ch(L_LEAD, d); }
static void t_ch1(int32_t d) { t_ch(L_PAD, d); }
static void t_ch2(int32_t d) { t_ch(L_BASS, d); }
static void t_ch3(int32_t d) { t_ch(L_DRUMS, d); }
static void v_ch0(char *o) { v_ch(o, L_LEAD); }
static void v_ch1(char *o) { v_ch(o, L_PAD); }
static void v_ch2(char *o) { v_ch(o, L_BASS); }
static void v_ch3(char *o) { v_ch(o, L_DRUMS); }
static void t_ch5(int32_t d) { t_ch(L_ARP, d); }
static void v_ch5(char *o) { v_ch(o, L_ARP); }
static void s_ch(char *o) { s_cat(o, "CHANNEL"); }
static void t_reset(int32_t d)                                  /* five clicks to the right: the defaults */
{
    reset_turns = clampi(reset_turns + (d > 0 ? 1 : -1), 0, 5);
    if (reset_turns == 5) { reset_turns = 0; defaults(); toast("DEFAULTS: SAVE TO KEEP"); }
}
static void v_reset(char *o) { if (!reset_turns) s_cat(o, "TURN"); else { o = s_int(o, 5 - reset_turns); s_cat(o, " MORE"); } }
static void s_reset(char *o) { s_cat(o, "5 RIGHT"); }
/* SYSTEM: what the memory has left (the stacks: the most they have ever used, read back from the paint); AUDIO: the
 * audio interrupt's time in microseconds, of the 5805 it has. Read twice a second; any knob on AUDIO starts the worst again */
static sysmem_t sysm;
static uint32_t sys_at, isr_avg_us, isr_sum0, isr_n0;
static void sys_poll(void)
{
    if (sys_at && fm1_ms - sys_at < 500u) return;
    sys_at = fm1_ms ? fm1_ms : 1u;
    sys_mem(&sysm);
    if (isr_n != isr_n0) { isr_avg_us = (isr_us_sum - isr_sum0) / (isr_n - isr_n0); isr_sum0 = isr_us_sum; isr_n0 = isr_n; }
}
static void s_kib(char *o, uint32_t b) { o = s_int(o, (int32_t)((b + 512u) / 1024u)); s_cat(o, "K"); }
static void v_free(char *o, uint32_t b) { sys_poll(); if (!sysm.known) s_cat(o, "N/A"); else s_kib(o, b); }
static void t_none(int32_t d) { (void)d; }
static void v_stk(char *o) { sys_poll(); v_free(o, sysm.stack_free); }
static void s_stk(char *o) { if (sysm.known) { o = s_cat(o, "OF "); s_kib(o, sysm.stack_size); } }
static void v_irq(char *o) { sys_poll(); v_free(o, sysm.irq_free); }
static void s_irq(char *o) { if (sysm.known) { o = s_cat(o, "OF "); s_kib(o, sysm.irq_size); } }
static void v_ram(char *o) { sys_poll(); v_free(o, sysm.ram_free); }
static void v_pool(char *o) { sys_poll(); v_free(o, sysm.pool_free); }
static void s_free(char *o) { if (sysm.known) s_cat(o, "FREE"); }
static void t_isr(int32_t d) { (void)d; isr_us_max = 0; }
static void v_worst(char *o) { s_int(o, (int32_t)isr_us_max); }
static void v_avg(char *o) { sys_poll(); s_int(o, (int32_t)isr_avg_us); }
static void v_load(char *o) { s_pct(o, (int32_t)(isr_us_max * 100u / 5805u)); }
static void s_msw(char *o) { s_cat(o, "US"); }
static void s_load(char *o) { s_cat(o, "OF 5.8MS"); }
static void v_tr(char *o) { s_cat(o, "TURN"); }
static void s_tr(char *o) { s_cat(o, "WORST"); }
static const srow_t ROW_SETUP[] = {
    {"LOOK", -1, {{"PALETTE", t_pal, v_pal, 0}, {"SCREEN", t_scr, v_scr, 0}, {"LIGHTS", t_glow, v_glow, s_glow}, {0, 0, 0, 0}}},
    {"MIDI", -1, {{"LEAD", t_ch0, v_ch0, s_ch}, {"PAD", t_ch1, v_ch1, s_ch}, {"BASS", t_ch2, v_ch2, s_ch}, {"DRUMS", t_ch3, v_ch3, s_ch}}},
    {"MIDI 2", -1, {{"ARP", t_ch5, v_ch5, s_ch}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}},
    {"RESET", -1, {{"DEFAULTS", t_reset, v_reset, s_reset}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}},
    {"SYSTEM", -1, {{"STACK", t_none, v_stk, s_stk}, {"IRQ", t_none, v_irq, s_irq}, {"RAM", t_none, v_ram, s_free}, {"POOL", t_none, v_pool, s_free}}},
    {"AUDIO", -1, {{"WORST", t_isr, v_worst, s_msw}, {"AVERAGE", t_isr, v_avg, s_msw}, {"LOAD", t_isr, v_load, s_load}, {"RESET", t_isr, v_tr, s_tr}}},
};
#define NROWS_OF(a) ((int32_t)(sizeof(a) / sizeof(a[0])))
static int32_t nrows(int32_t g) { return g == G_PLAY ? NROWS_OF(ROW_PLAY) : g == G_SOUND ? NROWS_OF(ROW_SOUND) : g == G_TRK ? NROWS_OF(ROW_TRK) : NROWS_OF(ROW_SONG); }
static const char *row_name(void)
{
    int32_t r = rowi[grp];
    if (setup_on) return ROW_SETUP[setup_row].name;
    return grp == G_PLAY ? ROW_PLAY[r].name : grp == G_SOUND ? ROW_SOUND[r].name : grp == G_TRK ? ROW_TRK[r].name : ROW_SONG[r].name;
}
/* what KNOB c does now, and for which lane (-1: the song) */
static const card_t *knob_card(int32_t c, int32_t *lane)
{
    int32_t r = rowi[grp];
    if (setup_on) { *lane = -1; return &ROW_SETUP[setup_row].c[c]; }
    if (edit_on) { *lane = sel; return &ROW_EDIT[edit_row][c]; }
    if (grp == G_SONG) { *lane = ROW_SONG[r].lane; return &ROW_SONG[r].c[c]; }
    *lane = c;
    return grp == G_PLAY ? &ROW_PLAY[r] : grp == G_SOUND ? &ROW_SOUND[r] : &ROW_TRK[r];
}
static void card_text(const card_t *C, int32_t lane, char *v, char *sb)
{
    int32_t keep = sel;
    v[0] = sb[0] = 0;
    if (!C->name) return;
    if (lane >= 0) sel = lane;
    C->val(v);
    if (C->sub) C->sub(sb);
    sel = keep;
}
static uint16_t knob_col(int32_t c, int32_t lane) { return setup_on ? C_LIGHT : edit_on ? LCOL[sel] : grp == G_SONG ? (lane >= 0 ? LCOL[lane] : GCOL[G_SONG]) : LCOL[c]; }
static uint16_t head_col(void) { return setup_on ? C_LIGHT : edit_on ? LCOL[sel] : GCOL[grp]; }

/* ---- drawing: the lanes */
static uint32_t pos16;                                      /* the play head, in steps Q16: it wraps after 65536 steps (65 minutes at
                                                              * 125), so it is only ever used as a difference or in its low bits */
static int32_t sx(uint32_t step_q16) { return NOWX + (int32_t)(((int64_t)(int32_t)(step_q16 - pos16) * PPS) >> 16); }
static int rt_on(int32_t l, const ev_t *e) { return fxu[l].retrig > 0 && (int32_t)(e->rv * 100u >> 8) < fxu[l].retrig; }
static void shape(int32_t x, int32_t y, int32_t w, int32_t h, int past, int ghost, int on, uint16_t col)
{
    if (on && seq_playing) fill(x, y - 1, w, h + 2, C_WHITE);
    else if (ghost && !past) outline(x, y, w, h, col);
    else fill(x, y, w, h, past ? cscale(col, 77) : col);
}
static int32_t note_y(int32_t l, int32_t y, int32_t h, int32_t n)
{
    int32_t big = h > 40, yy = y + h - (big ? 6 : 5) - (n - LLO[l]) * (h - (big ? 16 : 9)) / (LHI[l] - LLO[l]);
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
        int32_t a = (big ? 44 : l == sel ? 48 : 22) + flash * 56, tri = 256 - (r > h / 2 ? r - h / 2 : h / 2 - r) * 256 / (h / 2);
        fill(0, y + r, 240, 1, cscale(col, a * tri >> 8));
    }
    b0 = cur_step() / 16 - 1; if (b0 < 0) b0 = 0;                  /* the bars from the step count, which does not wrap */
    b1 = (cur_step() + (240 - NOWX) / PPS) / 16 + 1;
    for (b = b0; b <= b1; b++) {                                    /* bar and phrase lines */
        int32_t x = sx((uint32_t)b * 16u << 16);
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
            int32_t x = sx(((uint32_t)b * 16u + e->st) << 16), rt = rt_on(l, e), k;
            if (x > 240 || x < -64) continue;
            if (HITS(l)) {
                int32_t kind = e->note[0], row, rows, w, yy, len = 0;
                if (l == L_DRUMS) { rows = 3; row = kind == DK_SNARE || kind == DK_CLAP || kind == DK_RIM ? 1 : kind == DK_HAT || kind == DK_CRASH || kind == DK_OHAT ? 2 : 0; w = kind == DK_KETTLE || kind == DK_CRASH || kind == DK_TOM ? 6 : kind == DK_OHAT || kind == DK_CLAP ? 4 : kind == DK_RIM ? 2 : 3; }
                else { rows = NSX; row = kind; w = 3; if (SX_LONG(kind)) len = e->len * PPS - 1; }
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
        if (big) { int32_t ty = clampi(lp ? yc + 3 : yc - 13, y + 2, y + h - 12); fill(213, ty - 2, 20, 14, C_BG); text(215, ty, lp ? "LP" : "HP", col, 1); }
    }
    if (Bc && Bc->muted[l]) darken(0, y, NOWX, h, 140);
    if (ln[l].mute || form_gone(l, cur_bar())) darken(0, y, 240, h, 90);   /* ENV, END: faded out */
    if (!big) {                                                     /* the overview: the name; locked: inverted */
        int lock = ln[l].lock && l != L_SFX;
        fill(2, y + (h - 14) / 2, tw(LNAME[l], 1) + 5, 14, lock ? C_CREAM : C_BG);
        text(4, y + (h - 14) / 2 + 2, LNAME[l], lock ? C_BG : ln[l].mute ? cscale(col, 90) : col, 1);
        if (l == sel) fill(0, y, 2, h, C_WHITE);                   /* the lane the buttons and keys act on */
    } else {
    fill(1, y + 1, tw(LNAME[l], 1) + 5, 14, C_BG);                 /* the label: name */
    text(3, y + 3, LNAME[l], col, 1);
    {                                                               /* profile, USR or the sfx sound; locked: inverted */
        char t[4] = {PROF[ln[l].prof].name[0], PROF[ln[l].prof].name[1], PROF[ln[l].prof].name[2], 0};
        const char *tag = ln[l].mute ? "OFF" : ln[l].up.on ? "USR" : l == L_SFX ? (fx[l].inst < 0 ? "AUT" : SXNAME[fx[l].inst]) : t;
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

/* ---- drawing: EDIT (a step grid), the chords (SONG KEY) and the arc (SONG ARC), in strips over the lane area */
static void draw_edit_area(void)
{
    const upat_t *U = &ln[sel].up;
    const ubar_t *P = &ubar[sel][cursor >> 4];
    int32_t n = 16, x0 = 16, cwq = (220 << 8) / n, top = pan_y + 1, h = pan_h - 2, hits = HITS(sel), cs = cursor & 15, pb;
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
    fill(CX(cs), top, CX(cs + 1) - CX(cs), h, RGBC(0x2a3a3a));
    outline(CX(cs), top, CX(cs + (hits ? 1 : entry_len) > n ? n : cs + (hits ? 1 : entry_len)) - CX(cs) + 1, h, C_WHITE);
    for (s = 0; s < n; s++) {
        if (hits) { for (i = 0; i < rows; i++) if (P->kinds[s] >> i & 1u) fill(CX(s) + 1, top + i * rh + 2, CX(s + 1) - CX(s) - 2, rh - 5, col); }
        else if (P->n.k[s] != EMPTY) {
            i = 14 - P->n.k[s] - 3;
            if (i >= 0 && i < rows) fill(CX(s) + 1, top + i * rh + 2, CX(s + (P->n.len[s] > n - s ? n - s : P->n.len[s])) - CX(s) - 2, rh - 4, col);
        }
    }
    if ((cursor >> 4) >= U->bars) { fill(46, top + h / 2 - 9, 160, 18, C_BG); textc(126, top + h / 2 - 5, "NEW BAR: WRITE TO ADD", C_MUTE, 1); }
    pb = cur_bar();                                                 /* the bar playing, if it is this one of yours */
    pb = pb < U->bars ? pb : U->after == AF_LOOP ? pb % (U->bars ? U->bars : 1) : -1;
    if (seq_playing && U->on && pb == (cursor >> 4)) fill(CX((int32_t)((pos16 >> 16) % (uint32_t)n)) + (int32_t)(((pos16 & 0xFFFFu) * (uint32_t)(cwq >> 8)) >> 16), top, 1, h, C_CYAN);
#undef CX
}
static void draw_song_area(void)
{
    char t[48];
    int32_t auto_ = prog_sel == 0, i, n, cur, per, bw, y0 = pan_y;
    int32_t roots[8]; int8_t tones[8][3];
    text(6, y0 + 8, NN[key], C_CYAN, 2);
    t[0] = 0;
    if (auto_) { char *o = s_cat(t, "PROFILE "); s_cat(o, PROF[ln[L_BASS].prof].name); } else s_cat(t, SCALES[scale_sel].name);
    text(48, y0 + 6, t, C_CREAM, 1);
    t[0] = 0;
    if (auto_) s_cat(t, "ITS OWN CHORDS"); else { char *o = s_int(t, bpc); s_cat(o, bpc == 1 ? " BAR A CHORD" : " BARS A CHORD"); }
    text(48, y0 + 20, t, C_MUTE, 1);
    if (auto_) { const prof_t *P = &PROF[ln[L_BASS].prof]; n = 8; for (i = 0; i < 8; i++) { int32_t q = P->prog[i][1], k; roots[i] = P->prog[i][0]; for (k = 0; k < 3; k++) tones[i][k] = k < QUAL_N[q] ? QUAL[q][k] : 0; if (QUAL_N[q] == 2) tones[i][2] = 7, tones[i][1] = 7; } }
    else { const int8_t *P = prog_of(prog_sel - 1); n = 4; for (i = 0; i < 4; i++) chord_of(scale_sel, P[i], &roots[i], tones[i]); }
    cur = auto_ ? prof_chord(&PROF[ln[L_BASS].prof], cur_bar()) : (cur_bar() / bpc) % n;
    per = n > 4 ? 4 : n; bw = (232 - (per - 1) * 4) / per;
    for (i = 0; i < n; i++) {                               /* the chords; the one playing lit */
        int32_t big = n <= 4, hh = big ? 40 : 24, x = 4 + (i % per) * (bw + 4), yy = y0 + 38 + (i / per) * (hh + 2), on = seq_playing && i == cur;
        char *o = t;
        o = s_cat(o, NN[(key + roots[i]) % 12]);
        s_cat(o, tones[i][1] == 7 ? "5" : tones[i][2] == 6 ? "dim" : tones[i][2] == 8 ? "+" : tones[i][1] == 3 ? "m" : "");
        fill(x, yy, bw, hh, on ? C_CYAN : RGBC(0x1b2928));
        textc(x + bw / 2, yy + (big ? 7 : 7), t, on ? C_BG : C_CREAM, 1);
        if (big) { numeral(t, roots[i], tones[i]); textc(x + bw / 2, yy + 23, t, on ? C_BG : C_MUTE, 1); }
    }
}
/* the arc: the energy over the coming bars, the phases in colour */
static const uint16_t MODEC[5] = {RGBC(0x5d7370), RGBC(0x3fe0e0), C_WHITE, RGBC(0xffb04a), RGBC(0xb49bff)};
static void draw_arc_area(void)
{
    int32_t b, cb = cur_bar(), base = pan_y + pan_h - 20;
    static const char *const MN[5] = {"OFF", "BUILD", "BREAK", "PEAK", "DOWN"};
    int32_t m;
    if (!arc_on) { textc(120, pan_y + 30, "THE ARC IS OFF", C_MUTE, 1); textc(120, pan_y + 48, "KNOB 1 TURNS IT ON", C_DIMG, 1); return; }
    for (b = 0; b < 58; b++) {
        arc_t A = arc_at(cb + b);
        int32_t hgt = 2 + A.e * (pan_h - 30) / 256;
        fill(4 + b * 4, base - hgt, 3, hgt, b ? MODEC[A.mode] : C_WHITE);
    }
    for (m = 1; m < 5; m++) { fill(8 + (m - 1) * 58, base + 7, 8, 8, MODEC[m]); text(20 + (m - 1) * 58, base + 6, MN[m], C_MUTE, 1); }
}

/* ---- header, title, knobs */
static void draw_header(void)
{
    char t[40], wh[24];
    int32_t cb = cur_bar(), cw, i, x;
    uint16_t hc = head_col();
    wh[0] = 0;
    if (setup_on) s_cat(wh, "SETUP");
    else if (edit_on) { char *o = s_cat(wh, "EDIT "); s_cat(o, LNAME[sel]); } else s_cat(wh, GNAME[grp]);
    cw = tw(wh, 1) + 6;
    cs_begin(0, HEAD_H + 2, C_BG);
    fill(0, 0, cw, 15, hc);
    text(3, 3, wh, C_BG, 1);

    t[0] = 0;
    if (debug_on) { char *o = s_cat(t, "ISR "); o = s_int(o, (int32_t)isr_us_max); s_cat(o, "US"); }
    else { char *o = s_int(t, seq_bpm); s_cat(o, " BPM"); }
    x = 238 - tw(t, 1);
    text(x, 3, t, debug_on ? C_WHITE : C_CREAM, 1);
    if (cw + 6 + tw(PSHORT[ln[L_BASS].prof], 1) < x - 54) text(cw + 6, 3, PSHORT[ln[L_BASS].prof], C_MUTE, 1);   /* the style playing */
    for (i = 0; i < 4; i++) {                                       /* the bar in the phrase */
        int32_t on = seq_playing && cb % 4 == i;
        fill(x - 50 + i * 11, 4, 9, 9, on ? hc : seq_playing && cb % 4 > i ? cscale(hc, 90) : RGBC(0x1d2b2b));
    }
    fill(0, HEAD_H, 240, 1, cscale(hc, 90));
    cs_end();
}
/* the status at the title's right: what is about to happen, else what holds the lane */
static uint16_t status(char *o)
{
    int32_t cb = cur_bar();
    arc_t A = arc_at(cb);
    *o = 0;
    if (launch_bar >= 0) { int32_t left = launch_bar - cb; o = s_cat(o, "LAUNCH "); s_int(o, left < 0 ? 0 : left); return (pos16 >> 17) % 2 ? C_WHITE : C_CYAN; }
    if (A.mode == A_BREAK || arc_at(cb + 1).mode == A_BREAK) {
        arc_t N = A.mode == A_BREAK ? A : arc_at(cb + 1);
        if (N.until >= 0) { o = s_cat(o, "DROP IN "); s_int(o, N.until - cb); } else s_cat(o, "LET GO: DROP");
        return (pos16 >> 17) % 2 ? C_WHITE : RGBC(0xffb04a);
    }
    { const char *f = seq_playing ? form_name(cb) : 0; if (f) { s_cat(o, f); return RGBC(0xffb04a); } }
    if (ln[sel].mute) { o = s_cat(o, LNAME[sel]); s_cat(o, " OFF"); return C_CREAM; }
    if (ln[sel].lock && sel != L_SFX) { o = s_cat(o, LNAME[sel]); s_cat(o, " LOCK"); return C_CREAM; }
    if (target != ln[L_BASS].prof || blend) { o = s_cat(o, "NEXT "); s_cat(o, PSHORT[target]); return blend ? C_CYAN : C_DIMG; }
    if (kb_oct) { o = s_cat(o, "KEYS "); s_sign(o, kb_oct); return C_MUTE; }
    return C_MUTE;
}
static uint32_t title_hash;
static void draw_title(int force)
{
    char st[24];
    const char *name = row_name();
    uint16_t sc = status(st);
    int32_t n = setup_on ? NROWS_OF(ROW_SETUP) : nrows(grp), i, toast_on = fm1_ms < toast_until;
    uint32_t h = 2166136261u;
    for (i = 0; st[i]; i++) h = (h ^ (uint8_t)st[i]) * 16777619u;
    for (i = 0; toast_on && toast_s[i]; i++) h = (h ^ (uint8_t)toast_s[i]) * 16777619u;
    h ^= (uint32_t)(grp * 977 + rowi[grp] * 31 + toast_on * 7 + setup_on * 4099 + setup_row * 13) + sc;
    if (!force && h == title_hash) return;
    title_hash = h;
    cs_begin(TITLE_Y, TITLE_H, C_BG);
    if (toast_on) { fill(0, TITLE_Y + 4, 240, 26, C_FOOT); textc(120, TITLE_Y + 12, toast_s, C_WHITE, 1); }
    else {
        text(4, TITLE_Y + 8, name, C_CREAM, 2);
        for (i = 0; i < n; i++) fill(236 - (n - i) * 7 + 2, TITLE_Y + 6, 5, 5, i == (setup_on ? setup_row : rowi[grp]) ? head_col() : RGBC(0x2d4040));
        textr(238, TITLE_Y + 18, st, sc, 1);
    }
    cs_end();
}
/* KNOB 1..4: per column the lane (or the setting's name), the value large, a bar for a share, a word under it */
static uint32_t knobs_hash;
static void draw_knobs(int force)
{
    int32_t c, k;
    uint32_t h = 2166136261u;
    char v[4][24], sb[4][24];
    for (c = 0; c < 4; c++) {
        int32_t lane; uint32_t i;
        const card_t *C = knob_card(c, &lane);
        card_text(C, lane, v[c], sb[c]);
        for (i = 0; v[c][i]; i++) h = (h ^ (uint8_t)v[c][i]) * 16777619u;
        for (i = 0; sb[c][i]; i++) h = (h ^ (uint8_t)sb[c][i]) * 16777619u;
        h = (h ^ (uint32_t)(uintptr_t)C ^ (uint32_t)(lane >= 0 ? ln[lane].mute : 0) << 3) * 16777619u;
    }
    h ^= (uint32_t)(last_enc * 131 + (fm1_ms - last_enc_ms < 1500u) + sel * 7919 + grp * 31 + edit_on * 101 + setup_on * 3001 + setup_row * 17);
    if (!force && h == knobs_hash) return;
    knobs_hash = h;
    for (k = 0; k < 2; k++) {
        cs_begin(KNOB_Y + k * (KNOB_H / 2), KNOB_H / 2, C_BG);
        fill(0, KNOB_Y, 240, 1, C_GRID);
        for (c = 0; c < 4; c++) {
            int32_t lane, x = c * COL_W, cx = x + COL_W / 2, hot = last_enc == c + 2 && fm1_ms - last_enc_ms < 1500u, n, pct = -1, big, i, off;
            const card_t *C = knob_card(c, &lane);
            const char *label;
            uint16_t col = knob_col(c, lane);
            char *val = v[c];
            if (c) fill(x, KNOB_Y + 1, 1, KNOB_H - 1, C_GRID);
            if (!C->name) continue;
            off = !edit_on && lane >= 0 && ln[lane].mute;
            if (grp != G_SONG && !edit_on && !setup_on ? c == sel : hot) fill(x + 1, KNOB_Y + 1, COL_W - 1, KNOB_H - 1, cscale(col, 40));
            fill(x + 4, KNOB_Y + 1, COL_W - 7, 2, hot ? C_WHITE : cscale(col, off ? 70 : 150));
            label = grp != G_SONG && !edit_on && !setup_on ? LNAME[c] : C->name;
            if (tw(label, 1) <= COL_W - 2) textc(cx, KNOB_Y + 8, label, off ? cscale(col, 100) : col, 1);
            else text7(cx - tw7(label) / 2, KNOB_Y + 8, label, col);
            for (n = 0; val[n]; n++) ;
            if (n && val[n - 1] == '%') {                           /* a share: the number large, the bar */
                val[--n] = 0;
                for (pct = 0, i = 0; i < n; i++) if (val[i] >= '0' && val[i] <= '9') pct = pct * 10 + val[i] - '0';
            }
            if (val[0] == '-' && val[1] == '-') { textc(cx, KNOB_Y + 34, "-", C_DIMG, 2); continue; }    /* not for this lane */
            /* large: short numbers and note names; every word in the small font, so words never jump in size */
            big = n <= 3 && ((val[0] >= '0' && val[0] <= '9') || ((val[0] == '-' || val[0] == '+') && val[1] >= '0' && val[1] <= '9')
                             || (val[0] >= 'A' && val[0] <= 'G' && (n == 1 || (n == 2 && val[1] == '#'))));
            if (big) textc(cx, KNOB_Y + 30, val, hot ? C_WHITE : C_CREAM, 2);
            else { if (n > 8) val[8] = 0; text7(cx - tw7(val) / 2, KNOB_Y + 35, val, hot ? C_WHITE : C_CREAM); }
            if (pct >= 0) { fill(x + 6, KNOB_Y + 60, COL_W - 12, 4, C_GRID); fill(x + 6, KNOB_Y + 60, (COL_W - 12) * clampi(pct, 0, 100) / 100, 4, col); }
            if (off) { sb[c][0] = 0; s_cat(sb[c], "OFF"); }
            if (sb[c][0]) { if (tw7(sb[c]) > COL_W - 2) sb[c][8] = 0; text7(cx - tw7(sb[c]) / 2, KNOB_Y + 72, sb[c], off ? C_CREAM : C_MUTE); }
        }
        cs_end();
    }
}

/* ---- the frame: the header, the title, the lanes (or the grid, the chords, the arc) in strips, the knobs */
static uint32_t frames, fps_t0, fps;
static uint32_t last_drawn_pos = 0xFFFFFFFFu;
static void draw_frame(int force)
{
    int32_t l, k, ns, y0, h0, view = edit_on ? 1 : setup_on ? 0 : grp == G_SONG && rowi[G_SONG] == SR_KEY ? 2 : grp == G_SONG && rowi[G_SONG] == SR_ARC ? 3 : 0;
    pos16 = (uint32_t)cur_step() * 65536u + (seq_playing ? (uint32_t)vis_frac : 0u);
    draw_knobs(force);
    if (!edit_on) draw_title(force);
    if (!force && pos16 == last_drawn_pos && !area_dirty && !seq_playing) return;
    last_drawn_pos = pos16;
    draw_header();
    platform_poll();
    if (edit_on) { pan_y = TITLE_Y; pan_h = KNOB_Y - TITLE_Y; ns = 3; }        /* the grid takes the title's place too */
    else { pan_y = LANE_Y; pan_h = LANES_H; ns = 2; }
    for (k = 0; k < ns; k++) {
        y0 = pan_y + k * pan_h / ns; h0 = pan_y + (k + 1) * pan_h / ns - y0;
        cs_begin(y0, h0, C_BG);
        if (view == 1) draw_edit_area();
        else if (view == 2) draw_song_area();
        else if (view == 3) draw_arc_area();
        else for (l = 0; l < (int32_t)NLANE; l++) draw_lane(LROW[l], LANE_Y + l * (LH + GAP), LH, 0);
        cs_end();
        platform_poll();
    }
    area_dirty = 0;
    frames++;
    if (fm1_ms - fps_t0 >= 1000u) { fps = frames; frames = 0; fps_t0 = fm1_ms; }
}

/* ---- saving: settings (256 B) and patterns (their own sector), only while stopped (an erase stalls the sound) */
typedef struct {
    uint8_t ver, key, scale, prog, bpc, arc, build, peak, autod, target, bpm; int8_t energy; uint8_t space;
    struct { uint8_t prof, lock, dens; int8_t oct, filt; uint8_t res, wob, bright, drive, slide, retrig, rate, crush, echo, level; int8_t inst; uint8_t vary, mel, rhy, pit, chg, mute, send, midi; int8_t porta, pan; uint8_t vibr, trem, gate, tremor, swing; } l[NLANE];
    uint8_t palette, screen, glow;
} save_t;
typedef char save_fits[sizeof(save_t) <= 256u ? 1 : -1];          /* settings_save takes at most 256 bytes */
/* the own parts, in their store: [head 16 B][lanes 20 B][to 64][each lane's bars, 32 B each]; the CRC covers all
 * after the head, which is written last, so a cut save reads as none */
#define PARTS_MAGIC 0x53545250u                 /* "PRTS" */
typedef struct { uint32_t magic, total, crc, rsv; } phead_t;
typedef struct { uint16_t bars; uint8_t on, after; } plane_t;
static uint32_t pcrc(uint32_t c, const uint8_t *p, uint32_t n)  /* CRC-32, a piece at a time (start with 0) */
{
    uint32_t k;
    c = ~c;
    while (n--) { c ^= *p++; for (k = 0; k < 8u; k++) c = c & 1u ? (c >> 1) ^ 0xEDB88320u : c >> 1; }
    return ~c;
}
static int parts_save(void)
{
    static phead_t H;
    static plane_t L[NLANE];
    uint32_t total = 64, off, c, l;
    int rc = 0;
    for (l = 0; l < NLANE; l++) { L[l].bars = ln[l].up.bars; L[l].on = ln[l].up.on; L[l].after = ln[l].up.after; total += (uint32_t)L[l].bars * sizeof(ubar_t); }
    if (total > LONGS_LEN) return -9;                               /* cannot happen: 5 x MAXB bars fit (checked below) */
    for (off = 0; off < total && !rc; off += 0x1000u) rc = longs_erase(off);
    if (rc) return rc;
    c = pcrc(0, (const uint8_t *)L, sizeof L);
    if ((rc = longs_write(16, L, sizeof L)) != 0) return rc;
    for (off = 64, l = 0; l < NLANE; l++) {
        uint32_t n = (uint32_t)L[l].bars * sizeof(ubar_t);
        c = pcrc(c, (const uint8_t *)ubar[l], n);
        if ((rc = longs_write(off, ubar[l], n)) != 0) return rc;
        off += n;
    }
    H.magic = PARTS_MAGIC; H.total = total; H.crc = c; H.rsv = 0;
    return longs_write(0, &H, sizeof H);
}
static void parts_load(void)
{
    static phead_t H;
    static plane_t L[NLANE];
    uint32_t total = 64, off, c, l;
    if (longs_read(0, &H, sizeof H) || H.magic != PARTS_MAGIC || H.total > LONGS_LEN || longs_read(16, L, sizeof L)) return;
    for (l = 0; l < NLANE; l++) { if (L[l].bars < 1 || L[l].bars > MAXB) return; total += (uint32_t)L[l].bars * sizeof(ubar_t); }
    if (total != H.total) return;
    c = pcrc(0, (const uint8_t *)L, sizeof L);
    for (off = 64, l = 0; l < NLANE; l++) {
        uint32_t n = (uint32_t)L[l].bars * sizeof(ubar_t);
        if (longs_read(off, ubar[l], n)) { music_init(); return; }
        c = pcrc(c, (const uint8_t *)ubar[l], n);
        off += n;
    }
    if (c != H.crc) { for (l = 0; l < NLANE; l++) { int32_t b; for (b = 0; b < MAXB; b++) ubar_clear((int32_t)l, b); } return; }
    for (l = 0; l < NLANE; l++) {
        int32_t b;
        ln[l].up.bars = L[l].bars; ln[l].up.on = L[l].on != 0; ln[l].up.after = (uint8_t)(L[l].after % NAF); ln[l].up.ver++;
        for (b = L[l].bars; b < MAXB; b++) ubar_clear((int32_t)l, b);
    }
}
typedef char parts_fit[64 + NLANE * MAXB * sizeof(ubar_t) <= LONGS_LEN ? 1 : -1];   /* the whole store always fits */
static void do_save(void)
{
    static save_t S;
    int32_t l, rc;
    S.ver = 10; S.palette = (uint8_t)palette; S.screen = (uint8_t)screen_lvl; S.glow = (uint8_t)glow_on; S.energy = (int8_t)energy; S.space = (uint8_t)space; S.key = (uint8_t)key; S.scale = (uint8_t)scale_sel; S.prog = (uint8_t)prog_sel; S.bpc = (uint8_t)bpc; S.arc = (uint8_t)arc_on;
    S.build = (uint8_t)build_ph; S.peak = (uint8_t)peak_ph; S.autod = (uint8_t)auto_drop; S.target = (uint8_t)target; S.bpm = (uint8_t)seq_bpm;
    for (l = 0; l < (int32_t)NLANE; l++) {
        S.l[l].prof = ln[l].prof; S.l[l].lock = ln[l].lock; S.l[l].dens = ln[l].dens; S.l[l].oct = ln[l].oct; S.l[l].filt = (int8_t)fxu[l].filt;
        S.l[l].res = (uint8_t)fxu[l].res; S.l[l].wob = (uint8_t)fxu[l].wob; S.l[l].bright = (uint8_t)fxu[l].bright; S.l[l].drive = (uint8_t)fxu[l].drive;
        S.l[l].slide = (uint8_t)fxu[l].slide; S.l[l].retrig = (uint8_t)fxu[l].retrig; S.l[l].rate = (uint8_t)fxu[l].rtg_rate; S.l[l].crush = (uint8_t)fxu[l].crush;
        S.l[l].echo = (uint8_t)fxu[l].echo; S.l[l].level = (uint8_t)fxu[l].level; S.l[l].inst = (int8_t)fxu[l].inst; S.l[l].vary = (uint8_t)fxu[l].vary;
        S.l[l].mel = ln[l].mel; S.l[l].rhy = ln[l].rhy; S.l[l].pit = ln[l].pit; S.l[l].chg = ln[l].chg; S.l[l].mute = ln[l].mute; S.l[l].send = (uint8_t)fxu[l].send; S.l[l].midi = midi_chan[l];
        S.l[l].porta = (int8_t)fxu[l].porta; S.l[l].pan = (int8_t)fxu[l].pan; S.l[l].vibr = (uint8_t)fxu[l].vibr; S.l[l].trem = (uint8_t)fxu[l].trem;
        S.l[l].gate = (uint8_t)fxu[l].gate; S.l[l].tremor = (uint8_t)fxu[l].tremor; S.l[l].swing = (uint8_t)fxu[l].swing;
    }
    rc = settings_save(&S, sizeof S);
    if (!rc) rc = parts_save();
    toast(rc ? "SAVE FAILED" : "SAVED");
}
static void do_load(void)
{
    static save_t S;
    int32_t l;
    if (settings_load(&S, sizeof S) == 0 && S.ver == 10) {
        palette = S.palette % 6u; screen_lvl = clampi(S.screen, 25, 100) / 25 * 25; glow_on = S.glow != 0;
        energy = clampi(S.energy, -50, 50); space = clampi(S.space, 0, 100);
        set_key(S.key % 12u); scale_sel = S.scale < NSCALE ? S.scale : 0; prog_sel = S.prog <= NPROG ? S.prog : 0; bpc = clampi(S.bpc, 1, 4);
        arc_on = S.arc != 0; build_ph = clampi(S.build, 2, 16); if (arc_on) arc_restart(0); peak_ph = clampi(S.peak, 1, 8); auto_drop = S.autod != 0; target = S.target % NPROF; set_bpm(S.bpm);
        for (l = 0; l < (int32_t)NLANE; l++) {
            ln[l].prof = (uint8_t)(S.l[l].prof % NPROF); ln[l].lock = S.l[l].lock != 0; ln[l].dens = (uint8_t)clampi(S.l[l].dens, 0, 100); ln[l].oct = (int8_t)clampi(S.l[l].oct, -1, 1);
            fxu[l].filt = clampi(S.l[l].filt, -100, 100); fxu[l].res = clampi(S.l[l].res, 0, 100); fxu[l].wob = clampi(S.l[l].wob, 0, 5); fxu[l].bright = clampi(S.l[l].bright, 0, 100);
            fxu[l].drive = clampi(S.l[l].drive, 0, 100); fxu[l].slide = clampi(S.l[l].slide, 0, 100); fxu[l].retrig = clampi(S.l[l].retrig, 0, 100); fxu[l].rtg_rate = clampi(S.l[l].rate, 1, 3);
            fxu[l].crush = clampi(S.l[l].crush, 0, 100); fxu[l].echo = clampi(S.l[l].echo, 0, 100); fxu[l].level = clampi(S.l[l].level, 0, 100);
            fxu[l].vary = clampi(S.l[l].vary, 0, 100);
            ln[l].mel = (uint8_t)(S.l[l].mel % 100u); ln[l].rhy = (uint8_t)clampi(S.l[l].rhy, 0, 100); ln[l].pit = (uint8_t)clampi(S.l[l].pit, 0, 100);
            ln[l].chg = (uint8_t)clampi(S.l[l].chg, 0, 100); ln[l].mute = S.l[l].mute != 0; fxu[l].send = clampi(S.l[l].send, 0, 100);
            fxu[l].porta = clampi(S.l[l].porta, -100, 100); fxu[l].pan = clampi(S.l[l].pan, -100, 100); fxu[l].vibr = clampi(S.l[l].vibr, 0, 100);
            fxu[l].trem = clampi(S.l[l].trem, 0, 100); fxu[l].gate = clampi(S.l[l].gate, 0, 100); fxu[l].tremor = clampi(S.l[l].tremor, 0, 3); fxu[l].swing = clampi(S.l[l].swing, 0, 100); midi_chan[l] = (uint8_t)(S.l[l].midi > 16u ? 16u : S.l[l].midi);
            fxu[l].inst = clampi(S.l[l].inst, -1, l == L_SFX ? (int32_t)NSX - 1 : l == L_DRUMS ? (int32_t)NKIT - 1 : (int32_t)NPATCH - 1);
        }
    }
    parts_load();
}

/* the drop, the moment DROP is let go: its one has passed, so the crash and the impact come now */
static void drop_hit(void)
{
    live_push(L_DRUMS, DK_CRASH, 26, 1, (uint32_t)inst_eff(L_DRUMS));
    live_push(L_DRUMS, DK_KICK, 26, 1, (uint32_t)inst_eff(L_DRUMS));
    live_push(L_SFX, SX_IMPACT, 26, 1, 0);
}
static int is_black(int32_t k) { return (0x54Au >> ((53 + k) % 12)) & 1u; }
static int32_t white_index(int32_t k) { int32_t i, w = 0; for (i = 0; i < k; i++) if (!is_black(i)) w++; return is_black(k) ? -1 : w; }
static void start_stop(int from_top)
{
    if (seq_playing && !from_top) { frozen_step = 0; seq_stop_req = 1; form_arm(); return; }   /* the next PLAY starts from the top, with a new INTRO */
    if (from_top) { if (seq_playing) { seq_stop_req = 1; while (seq_playing) platform_poll(); } frozen_step = 0; launch_bar = -1; arc_restart(0); invalidate_from(0); }
    while (bars_update()) ;                                         /* the first bars exist before the clock starts */
    seq_start_step = frozen_step;
    seq_start_req = 1;
}
/* PLAY: stopped, it starts from the top with an INTRO (SEL + PLAY: everything at once); playing, it begins the END
 * (SEL + PLAY, or PLAY again during the END: stop now) */
static void play_button(int held)
{
    if (!seq_playing) {
        if (held || arc_on) form_reset();
        else if (intro_from < 0) form_intro(0);                     /* normally chosen already while stopped (form_arm) */
        start_stop(1);
        toast(held || arc_on ? "PLAY" : "INTRO");
        return;
    }
    if (held || end_from >= 0) { start_stop(0); toast("STOPPED"); return; }
    form_end(cur_bar()); toast("END: THE LANES FADE OUT");
}
/* a group button: the group; pressed again, its next setting. From EDIT: back to the group. */
static void group_btn(int32_t g)
{
    if (setup_on) setup_on = 0;
    else if (edit_on) edit_on = 0;
    else if (grp == g) rowi[g] = fmodp(rowi[g] + 1, nrows(g));
    grp = g;
    area_dirty = 1;
}
static void clear_bar_ui(void)                                  /* EDIT held: this bar (the last one goes) */
{
    char t[24], *o;
    int32_t b = cursor >> 4, last = b == ln[sel].up.bars - 1 && b > 0;
    if (b >= ln[sel].up.bars) return;
    pat_clear_bar(sel, b);
    o = s_cat(t, "BAR "); o = s_int(o, b + 1); s_cat(o, last ? " REMOVED" : " CLEARED"); toast(t);
    cursor = clampi(cursor, 0, nav_steps(sel) - 1);
}
static void clear_part_ui(void)                                 /* REC held: the whole part */
{
    pat_clear_all(sel);
    cursor = 0; toast("PART CLEARED");
}
static void edit_key(int32_t k, int32_t sel_held_now)
{
    int32_t n = nav_steps(sel), wi = white_index(k), l = sel;
    if (HITS(sel)) {
        int32_t nk = sel == L_DRUMS ? NDK : NSX;
        if (wi >= 0 && wi < nk) {
            UKIND(l, cursor) ^= (uint16_t)(1u << wi); pat_touched(l, cursor >> 4);
            if (!seq_playing) audition((uint32_t)sel, (uint32_t)wi, sel == L_SFX ? 0u : fxu[L_DRUMS].inst >= 0 ? (uint32_t)fxu[L_DRUMS].inst : PROF[ln[L_DRUMS].prof].inst[L_DRUMS]);
        }
        else cursor = clampi(cursor + 1, 0, nav_steps(sel) - 1);
        area_dirty = 1; return;
    }
    if (wi < 0) {                                                   /* black keys: rest, SEL + black: tie */
        if ((cursor >> 4) < ln[l].up.bars) {
            if (sel_held_now) { int32_t s; for (s = cursor - 1; s >= 0; s--) if (UK(l, s) != EMPTY) { ULEN(l, s) = (uint8_t)clampi(cursor - s + 1, 1, 16); break; } }
            else UK(l, cursor) = EMPTY;
            ln[l].up.ver++; invalidate_now();
        }
        cursor = clampi(cursor + 1, 0, n - 1); area_dirty = 1; return;
    }
    {
        int32_t s;
        UK(l, cursor) = (int8_t)(wi - 4 + 7 * entry_oct); ULEN(l, cursor) = (uint8_t)entry_len;
        pat_touched(l, cursor >> 4);
        n = nav_steps(sel);
        for (s = 1; s < entry_len && cursor + s < ln[l].up.bars * 16; s++) UK(l, cursor + s) = EMPTY;
        if (!seq_playing) {                                         /* hear it */
            harm_t H; tools_t T; uint8_t nt;
            harm_at(cur_bar(), &H); tools_make(&T, &H, LBASE[sel] + ln[sel].oct * 12 + key_off);
            nt = (uint8_t)clampi(stp(&T, UK(l, cursor)), 0, 127);
            audition((uint32_t)sel, nt, fxu[sel].inst >= 0 ? (uint32_t)fxu[sel].inst : PROF[ln[sel].prof].inst[sel]);
        }
        cursor = clampi(cursor + entry_len, 0, n - 1);
        area_dirty = 1;
    }
}

/* ---- the keys play the lane (the white bar beside it), F3..G5 plus OCT- / OCT+; drums and sfx: their sounds on the
 * white keys. On EDIT they enter steps. */
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
        if (e == ENC_ALGORITHM) { int32_t r = 0; while (LROW[r] != sel) r++; set_sel(LROW[fmodp(r + (d > 0 ? 1 : -1), NLANE)]); continue; }   /* the lane, in screen order */
        if (e == ENC_PRESETS) {                                         /* the setting in the group */
            if (setup_on) { setup_row = fmodp(setup_row + (d > 0 ? 1 : -1), NROWS_OF(ROW_SETUP)); reset_turns = 0; }
            else if (!edit_on) rowi[grp] = fmodp(rowi[grp] + (d > 0 ? 1 : -1), nrows(grp));
            else edit_row = !edit_row;                                  /* EDIT: the steps, or FILL and COPY */
            area_dirty = 1; continue;
        }
        {
            int32_t lane;
            const card_t *C = knob_card(e - ENC_KNOB1, &lane);
            if (!C->name) continue;
            if (lane >= 0) set_sel(lane);                               /* the lane you touch is the lane */
            C->turn(d);
            last_enc = e; last_enc_ms = fm1_ms;
            area_dirty = 1;
        }
    }
    if (b & (1u << BTN_PLAY)) play_button(held);
    {                                                               /* HOME: tap = the PLAY group; hold = SETUP */
        static uint32_t home_ms;
        static int home_held, home_used;
        if (b & (1u << BTN_HOME)) { home_ms = fm1_ms; home_held = 1; home_used = 0; }
        if (home_held && !home_used && fm1_ms - home_ms >= 600u) { home_used = 1; setup_on = 1; edit_on = 0; reset_turns = 0; area_dirty = 1; }
        if (rel & (1u << BTN_HOME)) { if (home_held && !home_used) group_btn(G_PLAY); home_held = 0; }
    }
    if (b & (1u << BTN_FX)) group_btn(G_SOUND);
    if (b & (1u << BTN_ARP)) group_btn(G_TRK);
    if (b & (1u << BTN_GLO)) group_btn(G_SONG);
    if (b & (1u << BTN_EDIT) && setup_on) { setup_on = 0; area_dirty = 1; }
    else if (b & (1u << BTN_EDIT)) {
        if (held && edit_on) clear_bar_ui();
        else { edit_on = !edit_on; cursor = clampi(cursor, 0, nav_steps(sel) - 1); area_dirty = 1; }
    }
    if (b & (1u << BTN_SEQ)) {
        if (edit_on) {                                              /* EDIT: copy the COPY lane's pattern into this one */
            char t[40], *o;
            if (copy_src == sel) toast("COPY: PICK ANOTHER LANE (EDIT 2)");
            else if (pat_empty(copy_src)) { o = s_cat(t, LNAME[copy_src]); s_cat(o, " HAS NO PART: FILL IT FIRST"); toast(t); }
            else { pat_copy(copy_src, sel); o = s_cat(t, LNAME[copy_src]); o = s_cat(o, " COPIED TO "); s_cat(o, LNAME[sel]); toast(t); }
        } else {                                                    /* LAUNCH: the queued style, at once */
            char t[40], *o;
            launch_tempo = !held;
            commit_launch();
            o = s_cat(t, PSHORT[target]); s_cat(o, held ? ", YOUR TEMPO" : ""); toast(t);
        }
        invalidate();
    }
    if (b & (1u << BTN_REC)) {
        if (edit_on) {
            if (held) clear_part_ui();
            else { char t[24], *o; pat_grab(sel, cursor >> 4); o = s_cat(t, "GRABBED INTO BAR "); s_int(o, (cursor >> 4) + 1); toast(t); }
        }
        else if (!seq_playing) toast("DROP: PRESS PLAY FIRST");
        else { drop_press(sel); { char t[40], *o = s_cat(t, LNAME[sel]); s_cat(o, " ALONE"); toast(t); } }
    }
    if (rel & (1u << BTN_REC) && drop_seg >= 0) { drop_release(); toast(drop_tapped ? "INTER" : "DROP"); }
    if (b & (1u << BTN_ENV)) {
        if (edit_on) edit_key(1, 1);                                /* tie (key 1 = F#3, a black key) */
        else if (held) { if (sel != L_SFX) { ln[sel].lock = (uint8_t)!ln[sel].lock; toast(ln[sel].lock ? "LOCKED: KEEPS ITS PROFILE" : "UNLOCKED"); invalidate(); } }
        else { char t[32], *o = s_cat(t, LNAME[sel]); ln[sel].mute = (uint8_t)!ln[sel].mute; s_cat(o, ln[sel].mute ? " FADES OUT" : " FADES IN"); toast(t); }
        area_dirty = 1;
    }
    if (b & (1u << BTN_LFO)) {
        if (!edit_on) {                                             /* a new idea, at once */
            char t[32], *o;
            ln[sel].mel = (uint8_t)fmodp(ln[sel].mel + 1 + (int32_t)(fm1_ms % 13u), 100); invalidate();
            o = s_cat(t, LNAME[sel]); o = s_cat(o, ": MELODY "); s_int(o, ln[sel].mel); toast(t);
        } else {
            int32_t n = pat_generate(sel, held, cursor >> 4);
            char t[40], *o;
            if (n) {
                o = s_cat(t, LNAME[sel]);
                if (fill_style >= 0) { o = s_cat(o, ": "); o = s_cat(o, PSHORT[fill_style]); o = s_cat(o, " #"); }
                else o = s_cat(o, held ? ": NEW PATTERN #" : ": VARIATION #");
                s_int(o, n - 1); toast(t); gen_count[sel] = n;
            }
            else toast("NO NEW ONE: SEL+LFO = FRESH");
        }
        area_dirty = 1;
    }
    {                                                               /* OCT- / OCT+: the keys' octave (EDIT: the entry octave); both: 0 */
        uint32_t both = (1u << BTN_OCT_DOWN) | (1u << BTN_OCT_UP);
        if ((fm1_in.buttons & both) == both && b & both) { kb_oct = 0; entry_oct = 0; }
        else if (b & (1u << BTN_OCT_DOWN)) { if (edit_on) entry_oct = clampi(entry_oct - 1, -1, 1); else kb_oct = clampi(kb_oct - 1, -2, 2); }
        else if (b & (1u << BTN_OCT_UP)) { if (edit_on) entry_oct = clampi(entry_oct + 1, -1, 1); else kb_oct = clampi(kb_oct + 1, -2, 2); }
        if (b & both) area_dirty = 1;
    }
    if (b & (1u << BTN_SAVE)) {
        if (held) { debug_on = !debug_on; isr_us_max = 0; }
        else if (seq_playing) { save_pending = 1; toast("SAVES WHEN YOU STOP"); }
        else do_save();
    }
    if (edit_on) {
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
    m |= (uint32_t)(setup_on || (!edit_on && grp == G_PLAY)) << BTN_HOME | (uint32_t)(!edit_on && grp == G_SOUND) << BTN_FX | (uint32_t)(!edit_on && grp == G_TRK) << BTN_ARP;
    m |= (uint32_t)edit_on << BTN_EDIT | (uint32_t)(!edit_on && grp == G_SONG) << BTN_GLO;
    m |= (uint32_t)(launch_bar >= 0) << BTN_SEQ | (uint32_t)ln[sel].mute << BTN_ENV | (uint32_t)save_pending << BTN_SAVE;
    m |= (uint32_t)(edit_on ? entry_oct < 0 : kb_oct < 0) << BTN_OCT_DOWN | (uint32_t)(edit_on ? entry_oct > 0 : kb_oct > 0) << BTN_OCT_UP;
    m |= (uint32_t)debug_on << BTN_SEL;
    if (edit_on) {
        for (k = 0; k < 27u; k++) {
            int32_t wi = white_index((int32_t)k);
            if (HITS(sel)) { if (wi >= 0 && wi < (sel == L_DRUMS ? NDK : NSX) && (UKIND(sel, cursor) >> wi & 1u)) km |= 1u << k; }
            else if (wi >= 0 && UK(sel, cursor) != EMPTY && wi - 4 + 7 * entry_oct == UK(sel, cursor)) km |= 1u << k;
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
/* everything as it comes: the lanes, the effects, the song, the look, the MIDI channels (patterns cleared) */
static void defaults(void)
{
    static const uint8_t CH0[NLANE] = {0, 1, 2, 9, 16, 3};
    uint32_t l;
    synth_init();
    music_init();
    for (l = 0; l < NLANE; l++) { fxu[l] = fx[l]; midi_chan[l] = CH0[l]; }
    fxu[L_LEAD].vary = 40;
    set_key(2); set_bpm(125); scale_sel = 0; prog_sel = 0; bpc = 1; energy = 0; space = 35;
    arc_on = 0; build_ph = 6; peak_ph = 4; auto_drop = 1; target = 2; blend = 0;
    palette = 0; screen_lvl = 100; glow_on = 1; fm1_glow_all();
    if (seq_playing) invalidate_from(cur_bar()); else form_arm();
    area_dirty = 1;
}
static void app_boot(void)
{
    uint32_t t_splash;
    defaults();
    do_load();
    {                                                               /* every power-on a new song: MELODY at random */
        rng_t r;
        uint32_t l;
        r.a = sys_seed() | 1u;
        for (l = 0; l < NLANE; l++) ln[l].mel = (uint8_t)((rnd(&r) >> 16) * 100u >> 16);
        form_rng.a ^= r.a;                                          /* and the INTRO's order */
    }
    form_arm();                                                     /* the first INTRO, shown before PLAY */
    splash();
    t_splash = fm1_ms;
    while (fm1_ms - t_splash < 1500u) platform_poll();
    lcd_fill(0, 0, 240, 240, C_BG);
    if (glow_on) fm1_glow_all(); else fm1_glow_none();
}
static void app_pass(void)
{
    uint8_t st, d1, d2;
    platform_poll();
    input();
    while (midi_get(&st, &d1, &d2)) {                           /* MIDI in: start / stop / continue */
        if (st == 0xFA) { if (!seq_playing) { form_reset(); start_stop(1); } }      /* MIDI start: everything at once */
        else if (st == 0xFB) { if (!seq_playing) start_stop(0); }
        else if (st == 0xFC) { if (seq_playing) start_stop(0); }
    }
    while (esp_mq_r != esp_mq_w) { uint32_t m = esp_mq[esp_mq_r % 64u]; esp_mq_r++; midi_send(m & 0xFFu, m >> 8 & 0xFFu, m >> 16 & 0xFFu); }
    if (launch_bar >= 0 && cur_bar() >= launch_bar) { commit_launch(); toast("LAUNCHED"); }
    if (seq_playing && end_from >= 0 && cur_bar() >= end_from + END_LEN) { start_stop(0); toast("THE END"); }
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
