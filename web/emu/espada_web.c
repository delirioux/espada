/* SPDX-License-Identifier: GPL-3.0-only */
/* Espada in the browser: a simulated FM-1 for firmware/src/espada.c, compiled to WebAssembly and run in an
 * AudioWorklet by Felucca's browser page and worklet (web/emu/index.html, worklet.js: Leo Kuroshita, Hügelton
 * Instruments, after charlesvestal's X0X, GPL-3.0). platform.c includes this file in place of the hardware when
 * DINGHY_WEB is set; nothing of Espada changes for it.
 *
 *   - the clock: platform_poll() is one millisecond of the device (as in host mode); the audio interrupt
 *     (app_audio) renders each 256-frame half as it comes due, into a ring the worklet drains. web_render(n)
 *     runs main-loop passes (app_pass) until n frames are there.
 *   - the panel: buttons by their printed labels and knobs by role, as Felucca's page sends them; the LEDs back.
 *   - the screen: lcd_fill / lcd_blit into a 240 x 240 RGB565 frame (the panel's byte order).
 *   - the flash: a 1 MiB image; settings and patterns in the sectors the page keeps (0xFC000, 0xFD000).
 * Not here: USB (MIDI out, audio, updates). MIDI in from the page's Web MIDI: start, continue, stop.
 *   web/emu/espada_build.sh  ->  build/espada_emu/{index.html, worklet.js, espada.wasm, ...} */
#include <stdint.h>

#ifndef APP_NAME
#define APP_NAME "ESPADA"
#endif
#ifndef APP_VER
#define APP_VER "WEB"
#endif
#define FS 44100u
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define HALF_FRAMES 256u
#define EXPORT __attribute__((used, visibility("default"))) /* (the page calls these) */

#include "../../firmware/src/libc.c"

static volatile uint32_t fm1_ms;
static struct { uint32_t notes, buttons; } fm1_in;

/* ---- the clock (TIMER4: 24 MHz; the audio interrupt's own timing reads 0 here: it runs on your computer) */
#define FM1_TICKS_PER_US 24u
static uint32_t web_ticks;
static uint32_t fm1_ticks(void) { return web_ticks; }

/* ---- the panel: Espada's button ids and encoders, set by the page's labels and roles */
static uint32_t web_edges, web_rel, web_notes_prev;
static int32_t web_enc_steps[7];
static uint32_t fm1_input_edges(uint32_t *released) { uint32_t b = web_edges; web_edges = 0; *released = web_rel; web_rel = 0; return b; }
static uint32_t fm1_input_note_edges(void) { uint32_t d = fm1_in.notes & ~web_notes_prev; web_notes_prev = fm1_in.notes; return d; }
static int32_t fm1_enc_take(uint32_t e) { int32_t d = e < 7u ? web_enc_steps[e] : 0; if (e < 7u) web_enc_steps[e] = 0; return d; }
static uint32_t web_lit;                         /* LEDs by id: buttons 0..13, keys 14..40 (bit 41 never) */
static uint64_t web_lit64, web_glow64;
static void fm1_led_key(uint32_t id, int on)
{
    if (id > 40u) return;
    if (on) web_lit64 |= 1ull << id; else web_lit64 &= ~(1ull << id);
    web_lit = (uint32_t)web_lit64;
}
static void fm1_glow_all(void) { web_glow64 = (1ull << 41) - 1u; }

/* ---- the screen: as the panel gets it (high byte first); gfx.c's canvases are already in that order */
static uint16_t web_fb[240 * 240];
static uint32_t web_draws;
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    uint16_t sw = (uint16_t)((c >> 8) | (c << 8));
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            web_fb[j * 240u + i] = sw;
    web_draws++;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            web_fb[(y + j) * 240u + x + i] = px[j * w + i];
    web_draws++;
}
#define CV_MAX (240u * 64u)
#include "../../firmware/src/gfx.c"

/* ---- the flash: settings at 0xFC000, patterns at 0xFD000 ([magic][length] then the bytes), in the sectors the
 * page keeps between visits */
static uint8_t nor[0x100000];
static uint32_t web_flash_writes;
#define WEB_SETTINGS 0xFC000u
#define WEB_PATTERNS 0xFD000u
static int web_load(uint32_t at, uint32_t magic, void *p, uint32_t n)
{
    uint32_t h[2];
    memcpy(h, nor + at, sizeof h);
    if (h[0] != magic || h[1] != n || n > 4096u - sizeof h) return -1;
    memcpy(p, nor + at + sizeof h, n);
    return 0;
}
static int web_save(uint32_t at, uint32_t magic, const void *p, uint32_t n)
{
    uint32_t h[2] = {magic, n};
    if (n > 4096u - sizeof h) return -1;
    memset(nor + at, 0xFF, 4096);
    memcpy(nor + at, h, sizeof h);
    memcpy(nor + at + sizeof h, p, n);
    web_flash_writes++;
    return 0;
}
static int settings_load(void *p, uint32_t n) { return web_load(WEB_SETTINGS, 0x59474E44u, p, n); }
static int settings_save(const void *p, uint32_t n) { return web_save(WEB_SETTINGS, 0x59474E44u, p, n); }
static int patterns_load(void *p, uint32_t n) { return web_load(WEB_PATTERNS, 0x41445345u, p, n); }
static int patterns_save(const void *p, uint32_t n) { return web_save(WEB_PATTERNS, 0x41445345u, p, n); }

/* ---- MIDI: in from the page (Web MIDI), out nowhere */
static uint8_t mq[64][3];
static uint32_t mq_w, mq_r;
static int midi_get(uint8_t *st, uint8_t *d1, uint8_t *d2)
{
    if (mq_r == mq_w) return 0;
    *st = mq[mq_r % 64u][0]; *d1 = mq[mq_r % 64u][1]; *d2 = mq[mq_r % 64u][2];
    mq_r++;
    return 1;
}
static void midi_send(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

static void app_audio(int32_t *out, uint32_t n);
static void app_boot(void);
static void app_pass(void);

/* ---- the device's millisecond: the clock, then the audio halves due by now (into the ring, stereo float) */
#define RING 16384u
static float ring[RING * 2];
static uint32_t ring_w, ring_r, web_owed, web_master_q8 = 180;
static void platform_poll(void)
{
    static int32_t blk[2u * HALF_FRAMES];
    fm1_ms++;
    web_ticks += 1000u * FM1_TICKS_PER_US;
    web_owed += FS;
    while (web_owed >= 1000u * HALF_FRAMES) {
        uint32_t k;
        web_owed -= 1000u * HALF_FRAMES;
        memset(blk, 0, sizeof blk);
        app_audio(blk, HALF_FRAMES);
        for (k = 0; k < HALF_FRAMES; k++) {
            uint32_t i = (ring_w++ % RING) * 2u;
            int32_t l = (blk[2u * k] * (int32_t)web_master_q8) >> 8, r = (blk[2u * k + 1u] * (int32_t)web_master_q8) >> 8;
            ring[i] = (float)(l > 32767 ? 32767 : l < -32767 ? -32767 : l) / 32768.0f;
            ring[i + 1] = (float)(r > 32767 ? 32767 : r < -32767 ? -32767 : r) / 32768.0f;
        }
        if (ring_w - ring_r > RING) ring_r = ring_w - RING;     /* (the boot's splash: no one listens yet) */
    }
}

/* ---- the exports */
static uint32_t web_booted;
static float out_l[1024], out_r[1024];
EXPORT void _initialize(void) {}
EXPORT float *web_out_l(void) { return out_l; }
EXPORT float *web_out_r(void) { return out_r; }
EXPORT void web_render(uint32_t n)
{
    uint32_t k;
    if (n > 1024u) n = 1024u;
    if (!web_booted) { for (k = 0; k < n; k++) out_l[k] = out_r[k] = 0.0f; return; }
    while (ring_w - ring_r < n) app_pass();
    for (k = 0; k < n; k++) {
        uint32_t i = (ring_r++ % RING) * 2u;
        out_l[k] = ring[i];
        out_r[k] = ring[i + 1];
    }
}
EXPORT uint8_t *web_nor(void) { return nor; }
EXPORT uint32_t web_nor_size(void) { return sizeof nor; }
EXPORT void web_nor_erase(void) { memset(nor, 0xFF, sizeof nor); }
EXPORT uint32_t web_flash_writes_count(void) { return web_flash_writes; }
EXPORT void web_boot(void)
{
    if (web_booted) return;
    app_boot();
    ring_r = ring_w;                                            /* from now on, in step with the page */
    web_booted = 1;
}
/* MASTER: 0..1023 as the pot reads (the FM-1 applies it after the app; here the same) */
EXPORT void web_master(uint32_t v) { web_master_q8 = (v > 1023u ? 1023u : v) * 360u / 1023u; }
EXPORT uint32_t web_sample_rate(void) { return FS; }
EXPORT uint32_t web_now_ms(void) { return fm1_ms; }
EXPORT uint16_t *web_screen(void) { return web_fb; }
EXPORT uint32_t web_screen_draws(void) { return web_draws; }

/* the page's buttons by label (Felucca's panel.c order) and knobs by role, as Espada's ids */
static const uint8_t LABEL_BTN[14] = {BTN_FX, BTN_SEL, BTN_ENV, BTN_LFO, BTN_EDIT, BTN_GLO, BTN_HOME, BTN_SAVE,
                                      BTN_ARP, BTN_SEQ, BTN_PLAY, BTN_REC, BTN_OCT_DOWN, BTN_OCT_UP};
static const uint8_t ROLE_ENC[7] = {ENC_SELECT, ENC_ALGORITHM, ENC_PRESETS, ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
EXPORT void web_buttons(uint32_t labels)
{
    uint32_t m = 0, b, old = fm1_in.buttons;
    for (b = 0; b < 14u; b++)
        if (labels >> b & 1u) m |= 1u << LABEL_BTN[b];
    web_edges |= m & ~old;
    web_rel |= old & ~m;
    fm1_in.buttons = m;
}
EXPORT void web_keys(uint32_t mask) { fm1_in.notes = mask & ((1u << 27) - 1u); }
EXPORT void web_enc(uint32_t role, int32_t n) { if (role < 7u) web_enc_steps[ROLE_ENC[role]] += n; }
EXPORT uint32_t web_midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (mq_w - mq_r >= 64u) return 0;
    mq[mq_w % 64u][0] = (uint8_t)st; mq[mq_w % 64u][1] = (uint8_t)d1; mq[mq_w % 64u][2] = (uint8_t)d2;
    mq_w++;
    return 1;
}
/* the LEDs back, by label (bit 14: PLAY's green) and key */
static uint32_t by_label(uint64_t ids)
{
    uint32_t b, m = 0;
    for (b = 0; b < 14u; b++) m |= (uint32_t)(ids >> LABEL_BTN[b] & 1u) << b;
    return m;
}
EXPORT uint32_t web_lit_buttons(void) { return by_label(web_lit64) | (uint32_t)(web_lit64 >> BTN_PLAY & 1u) << 14; }
EXPORT uint32_t web_lit_keys(void) { return (uint32_t)(web_lit64 >> 14) & ((1u << 27) - 1u); }
EXPORT uint32_t web_dim_buttons(void) { return by_label(web_glow64); }
EXPORT uint32_t web_dim_keys(void) { return (uint32_t)(web_glow64 >> 14) & ((1u << 27) - 1u); }
EXPORT uint32_t web_dim_level(void) { return 0; }
EXPORT uint32_t web_breath_buttons(void) { return 0; }
EXPORT uint32_t web_breath_keys(void) { return 0; }
