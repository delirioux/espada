#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Run firmware/src/makohai.c on this computer (tests/host.c): build/host/makohai.wav and the screens as PNG.
# Your own script: build/host/makohai OUT script.txt [FPS] (see tests/host.c and tests/demo.txt).
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host
cc -O2 -Wall -Wno-unused-function -DDINGHY_HOST -DAPP_VER="\"${APP_VER:-dev}\"" -Ifirmware/src -Ifirmware/gen -o build/host/makohai firmware/src/makohai.c
build/host/makohai build/host/makohai
python3 - <<'PY'
from pathlib import Path
from PIL import Image
for p in sorted(Path("build/host").glob("makohai-*.ppm")):
    Image.open(p).resize((480, 480), Image.NEAREST).save(p.with_suffix(".png"))
    p.unlink()
    print("screen ->", p.with_suffix(".png"))
PY
