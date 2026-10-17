// SPDX-License-Identifier: GPL-3.0-only
// Espada's browser build without a browser: the worklet's calls, from Node. Boots, presses PLAY, turns to a
// channel, plays a key, and checks that sound comes out, the screen changes and the LEDs answer.
//   node web/emu/espada_emu_test.mjs build/espada_emu/espada.wasm [screen.ppm]
import fs from "node:fs";
const [wasmPath, ppm] = process.argv.slice(2);
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
const ex = instance.exports;
const fail = (m) => { console.error("FAIL: " + m); process.exit(1); };
ex._initialize();
ex.web_nor_erase();
ex.web_master(800);
ex.web_boot();
const L = { FX: 0, SEL: 1, PLAY: 10, HOME: 6 };
const run = (ms) => { let e = 0; for (let i = 0; i < Math.ceil(ms * 44.1 / 128); i++) { ex.web_render(128);
  const l = new Float32Array(ex.memory.buffer, ex.web_out_l(), 128); for (const v of l) e += v * v; } return Math.sqrt(e / (ms * 44.1)); };
const press = (label) => { ex.web_buttons(1 << L[label]); run(40); ex.web_buttons(0); run(40); };
const quiet = run(500);
const draws0 = ex.web_screen_draws();
press("PLAY");
const playing = run(4000);
if (!(playing > 0.01)) fail(`no sound while playing (rms ${playing})`);
if (!(ex.web_lit_buttons() >> 14 & 1)) fail("PLAY's light is off while playing");
if (!ex.web_dim_buttons()) fail("the buttons do not glow");
if (ex.web_screen_draws() <= draws0) fail("the screen did not change");
ex.web_enc(1, 1); run(200);                       // ALGORITHM: to LEAD
ex.web_keys(1 << 7); run(300);                    // C4 held
if (!(ex.web_lit_keys() >> 7 & 1)) fail("a held key is not lit");
ex.web_keys(0); run(300);
press("PLAY");
const t = ex.web_now_ms();
console.log(`ok: quiet ${quiet.toFixed(4)}, playing ${playing.toFixed(3)} rms, ${ex.web_screen_draws()} screen draws, device time ${t} ms`);
if (ppm) {
  const fb = new Uint16Array(ex.memory.buffer, ex.web_screen(), 240 * 240), px = Buffer.alloc(240 * 240 * 3);
  for (let i = 0; i < fb.length; i++) { const c = ((fb[i] & 255) << 8) | (fb[i] >> 8);
    px[3 * i] = (c >> 11) << 3; px[3 * i + 1] = ((c >> 5) & 63) << 2; px[3 * i + 2] = (c & 31) << 3; }
  fs.writeFileSync(ppm, Buffer.concat([Buffer.from("P6\n240 240\n255\n"), px]));
}
