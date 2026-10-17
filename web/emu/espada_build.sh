#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Espada in the browser: firmware/src/espada.c on a simulated FM-1 (web/emu/espada_web.c), compiled to WebAssembly,
# with Felucca's page and worklet (web/emu/index.html, worklet.js: Leo Kuroshita, Hügelton Instruments) under
# Espada's name.
#   web/emu/espada_build.sh   ->  build/espada_emu/{index.html, worklet.js, espada.wasm, espada-logo.png, fonts/}
# Needs clang with the wasm32 target and wasm-ld (lld), or Emscripten (emcc). Espada needs no C library: the module
# imports nothing.
set -e
cd "$(dirname "$0")/../.."
OUT=build/espada_emu
VER=${1:-$(git describe --tags --always 2>/dev/null || echo dev)}
mkdir -p "$OUT/fonts" "$OUT/obj"
CF="-O2 -ffreestanding -fno-builtin -DDINGHY_WEB -DAPP_VER=\"WEB\" -Ifirmware/src -Ifirmware/gen -w"
if command -v clang >/dev/null 2>&1 && command -v wasm-ld >/dev/null 2>&1; then
    # shellcheck disable=SC2086
    clang --target=wasm32 -nostdlib $CF -c firmware/src/espada.c -o "$OUT/obj/espada.o"
    wasm-ld --no-entry --export-dynamic --gc-sections -z stack-size=1048576 --initial-memory=8388608 \
        "$OUT/obj/espada.o" -o "$OUT/espada.wasm"
elif command -v emcc >/dev/null 2>&1; then
    # shellcheck disable=SC2086
    emcc $CF --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=8388608 -sFILESYSTEM=0 \
        -o "$OUT/espada.wasm" firmware/src/espada.c
else
    echo "espada emu: no clang + wasm-ld and no emcc"; exit 1
fi
# Felucca's worklet as it is; Felucca's page with Espada's name, its own saves, SEL for the button Felucca calls SCL
cp web/emu/worklet.js "$OUT/worklet.js"
sed -e "s/@VERSION@/Espada $VER/g" -e "s/@PREVIEW@//g" \
    -e 's|<title>Felucca</title>|<title>Espada</title>|' \
    -e 's|content="Felucca, the multi-engine synth firmware for the FM-1, running in your browser: the same code the instrument runs."|content="Espada, generative tracker grooves for the FM-1, running in your browser: the same code the instrument runs."|' \
    -e 's|const STORE = "felucca-emu-flash"|const STORE = "espada-emu-flash"|' -e 's|"felucca-emu-knobs"|"espada-emu-knobs"|' -e 's|"felucca-emu-knobmode"|"espada-emu-knobmode"|' \
    -e 's|fetch("felucca.wasm")|fetch("espada.wasm")|' -e 's|felucca.wasm: |espada.wasm: |' \
    -e 's|FX: 0, SCL: 1,|FX: 0, SEL: 1,|' -e 's|\["FX", "SCL", "ENV"|["FX", "SEL", "ENV"|' \
    -e 's|  Felucca by Leo Kuroshita, Hügelton Instruments|  Espada by delirioux · this page and its worklet: Felucca by Leo Kuroshita, Hügelton Instruments|' \
    -e 's|  <nav><a href="../installer/">Install</a><a href="../editor/">Editor</a><a href="https://github.com/hugelton/Felucca">Source</a></nav>|  <nav><a href="../install/">Install on the FM-1</a><a href="https://github.com/delirioux/espada/blob/main/docs/MANUAL.md">Manual</a><a href="https://github.com/delirioux/espada">Source</a></nav>|' \
    -e 's|Felucca is not affiliated with or endorsed by them.|Espada and Felucca are not affiliated with or endorsed by them.|' \
    -e 's|<link rel="icon" href="[^"]*">|<link rel="icon" href="espada-logo.png">|' \
    -e 's|^<div class="device" id="device">|<img src="espada-logo.png" alt="Espada" width="128" height="128" style="display:block;margin:0 auto 12px;border-radius:10px">\n&|' \
    web/emu/index.html > "$OUT/index.html"
for s in 'src="espada-logo.png"' 'fetch("espada.wasm")' 'SEL: 1' '"espada-emu-flash"'; do
    grep -q "$s" "$OUT/index.html" || { echo "espada emu: Felucca's page changed ($s not set)"; exit 1; }
done
cp web/emu/fonts/DotGothic16-subset.woff web/emu/fonts/OFL.txt "$OUT/fonts/"
python3 -c "from PIL import Image; Image.open('assets/logo.png').convert('RGB').resize((256, 256), Image.LANCZOS).save('$OUT/espada-logo.png', optimize=True)"
rm -rf "$OUT/obj"
echo "espada emu: $OUT ($(wc -c < "$OUT/espada.wasm" | tr -d ' ') B wasm)"
