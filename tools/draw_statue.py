#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The STATUE picture's pictures, drawn here (no photo: nothing to license): a marble bust of a young Greek god,
ray-marched from signed distance functions, and the scroller's text. Writes firmware/gen/makohai_statue.h:
  STATUE_PX   120 x 120, 4 bits a pixel (0: not the statue, 1..15: marble from dark to light)
  SCROLL_BITS the text, 1 bit a pixel, SCROLL_H rows of SCROLL_W
  STATUE_PATH the bust's outline, a little out: PATH_N points (x, y from the middle), a pixel apart
Needs numpy and Pillow; takes about a minute."""
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
N = 240                                     # rendered at 240, kept at 120
TEXT = "MAKE FIRMWARE NOT WAR   *   MAKOHAI ON THE FM-1   *   GREETINGS TO ALL WHO WRITE THEIR OWN   *   "
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
SCROLL_H = 20


def length(p):
    return np.sqrt((p * p).sum(-1))


def sphere(p, c, r):
    return length(p - np.array(c)) - r


def ellip(p, c, r):
    q = (p - np.array(c)) / np.array(r)
    k0 = length(q)
    k1 = length(q / np.array(r))
    return k0 * (k0 - 1.0) / np.maximum(k1, 1e-6)


def capsule(p, a, b, r):
    a, b = np.array(a), np.array(b)
    pa, ba = p - a, b - a
    h = np.clip((pa * ba).sum(-1) / (ba * ba).sum(), 0, 1)
    return length(pa - ba * h[..., None]) - r


def cyl(p, c, r, h):
    q = p - np.array(c)
    d = np.stack([np.sqrt(q[..., 0] ** 2 + q[..., 2] ** 2) - r, np.abs(q[..., 1]) - h], -1)
    return np.minimum(np.maximum(d[..., 0], d[..., 1]), 0) + length(np.maximum(d, 0))


def smin(a, b, k):
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0, 1)
    return b * (1 - h) + a * h - k * h * (1 - h)


def smax(a, b, k):
    return -smin(-a, -b, k)


def sdf(p):
    x, y, z = p[..., 0], p[..., 1], p[..., 2]
    # the head
    d = ellip(p, (0, 0.12, -0.02), (0.47, 0.6, 0.55))                 # skull
    d = smin(d, ellip(p, (0, -0.12, 0.14), (0.4, 0.56, 0.44)), 0.12)  # face
    d = smin(d, ellip(p, (0, -0.47, 0.24), (0.25, 0.17, 0.22)), 0.1)  # chin and jaw
    d = smin(d, capsule(p, (0, 0.1, 0.47), (0, -0.17, 0.585), 0.062), 0.07)  # the straight Greek nose
    d = smin(d, capsule(p, (-0.2, 0.16, 0.47), (0.2, 0.16, 0.47), 0.055), 0.06)   # brow
    for s in (-1, 1):
        d = smax(d, -sphere(p, (s * 0.17, 0.05, 0.54), 0.095), 0.035)  # eye sockets
        d = smin(d, sphere(p, (s * 0.17, 0.045, 0.44), 0.075), 0.02)  # blank marble eyes
        d = smin(d, ellip(p, (s * 0.47, -0.02, -0.02), (0.06, 0.15, 0.1)), 0.04)   # ears
    d = smin(d, ellip(p, (0, -0.29, 0.53), (0.12, 0.035, 0.06)), 0.02)  # upper lip
    d = smin(d, ellip(p, (0, -0.35, 0.51), (0.1, 0.035, 0.05)), 0.02)   # lower lip
    # the curls: a cap of hair, bumpy
    hair = ellip(p, (0, 0.2, -0.06), (0.55, 0.62, 0.62))
    hair = smax(hair, -(z - 0.38 + 0.6 * (y - 0.25)), 0.05)           # off the forehead and the face
    hair = smax(hair, -(y + 0.15 - 0.45 * np.maximum(z, 0)), 0.05)    # down to the ears at the sides, higher in front
    curls = 0.022 * (np.sin(x * 19) + np.sin(y * 21 + 1.3) + np.sin(z * 20 + 0.7)) / 3
    curls += 0.018 * np.abs(np.sin(x * 15 + y * 11) * np.cos(z * 14 - y * 9))
    d = smin(d, hair - curls, 0.04)
    # the neck, the shoulders, the chest: a herm-like bust on a pedestal
    d = smin(d, capsule(p, (0, -0.55, -0.04), (0, -1.0, 0.0), 0.26), 0.12)
    chest = ellip(p, (0, -1.38, -0.02), (0.92, 0.48, 0.4))
    chest = smin(chest, ellip(p, (0, -1.15, 0.08), (0.55, 0.22, 0.26)), 0.15)   # collarbones and pectorals
    chest = smax(chest, -(y + 1.62), 0.02)                            # cut flat at the bottom
    d = smin(d, chest, 0.14)
    ped = cyl(p, (0, -1.72, 0), 0.32, 0.1)
    ped = smin(ped, cyl(p, (0, -1.9, 0), 0.42, 0.08), 0.02)
    return np.minimum(d, ped)


def normal(p):
    e = 0.002
    dx = np.array([e, 0, 0]); dy = np.array([0, e, 0]); dz = np.array([0, 0, e])
    n = np.stack([sdf(p + dx) - sdf(p - dx), sdf(p + dy) - sdf(p - dy), sdf(p + dz) - sdf(p - dz)], -1)
    return n / np.maximum(length(n)[..., None], 1e-9)


def render():
    yaw, pitch = np.radians(28), np.radians(-6)
    cam = np.array([np.sin(yaw) * 5.2, -0.45 + 5.2 * np.sin(pitch) * -1, np.cos(yaw) * 5.2])
    target = np.array([0, -0.6, 0])
    fw = target - cam; fw /= np.linalg.norm(fw)
    rt = np.cross(fw, [0, 1, 0]); rt /= np.linalg.norm(rt)
    up = np.cross(rt, fw)
    u, v = np.meshgrid(np.linspace(-1, 1, N), np.linspace(1, -1, N))
    fov = 0.34
    dirs = fw + (u[..., None] * rt + v[..., None] * up) * fov
    dirs /= length(dirs)[..., None]
    t = np.zeros((N, N)); hit = np.zeros((N, N), bool); steps = np.zeros((N, N))
    for _ in range(160):
        p = cam + dirs * t[..., None]
        d = sdf(p)
        live = ~hit & (t < 12)
        hit |= live & (d < 0.0015)
        t = np.where(live & ~hit, t + d * 0.9, t)
        steps += live
    p = cam + dirs * t[..., None]
    n = normal(p)
    key = np.array([-0.55, 0.6, 0.6]); key /= np.linalg.norm(key)
    rim = np.array([0.8, 0.3, -0.5]); rim /= np.linalg.norm(rim)
    dif = np.clip(((n * key).sum(-1) + 0.25) / 1.25, 0, 1)            # wrapped: marble lets light in
    # soft shadow toward the key light
    sh = np.ones((N, N)); st = np.full((N, N), 0.02)
    for _ in range(40):
        q = p + n * 0.003 + key * st[..., None]
        h = sdf(q)
        sh = np.minimum(sh, np.clip(8 * h / st, 0, 1))
        st += np.clip(h, 0.01, 0.2)
    ao = np.zeros((N, N))
    for k in range(1, 6):
        h = 0.03 * k
        ao += (h - sdf(p + n * h)) / (2 ** k)
    ao = np.clip(1 - 4 * ao, 0, 1)
    rimv = np.clip((n * rim).sum(-1), 0, 1) ** 2
    spec = np.clip((n * (key - dirs) / np.maximum(length(key - dirs)[..., None], 1e-6)).sum(-1), 0, 1) ** 24
    vein = 0.04 * np.sin(p[..., 0] * 9 + 3 * np.sin(p[..., 1] * 5 + p[..., 2] * 7))   # a faint vein in the marble
    l = 0.12 + 0.72 * dif * (0.45 + 0.55 * sh) * ao + 0.25 * rimv + 0.2 * spec * sh + vein
    l = np.clip(l, 0, 1)
    rgb = np.stack([l * 245, l * 238, l * 222], -1)                     # a warm white marble
    a = np.where(hit, 255, 0)
    img = np.concatenate([rgb, a[..., None]], -1).astype(np.uint8)
    return Image.fromarray(img, "RGBA")



def main():
    img = render()                                              # RGBA, transparent around the statue
    small = img.resize((120, 120), Image.LANCZOS)
    a = np.asarray(small).astype(np.int32)
    lum = a[..., 0]
    idx = np.where(a[..., 3] > 127, 1 + np.clip(lum * 15 // 256, 0, 14), 0).astype(np.uint8)
    px = idx.reshape(-1)
    packed = [(int(px[i]) << 4) | int(px[i + 1]) for i in range(0, len(px), 2)]
    try:
        font = ImageFont.truetype(FONT, 18)
    except OSError:
        font = ImageFont.load_default()
    w = int(ImageDraw.Draw(Image.new("1", (1, 1))).textlength(TEXT, font=font)) + 8
    strip = Image.new("1", (w, SCROLL_H), 0)
    ImageDraw.Draw(strip).text((0, 0), TEXT, fill=1, font=font)
    bits = np.asarray(strip, dtype=np.uint8)
    stride = (w + 7) // 8
    sb = []
    for r in range(SCROLL_H):
        for c in range(stride):
            v = 0
            for k in range(8):
                x = c * 8 + k
                if x < w and bits[r, x]:
                    v |= 0x80 >> k
            sb.append(v)
    # the path the text runs along: the bust's outline, a little out, smoothed, a point every pixel
    m = a[..., 3] > 127
    ang = np.linspace(0, 2 * np.pi, 720, endpoint=False)
    rad = np.zeros(len(ang))
    for i, t in enumerate(ang):
        far = 0
        for d in np.arange(0, 60, 0.5):
            x, y = int(60 + d * np.cos(t)), int(60 + d * np.sin(t))
            if 0 <= x < 120 and 0 <= y < 120 and m[y, x]:
                far = d
        rad[i] = min(far + 9, 58)
    k = np.ones(31) / 31
    rad = np.convolve(np.concatenate([rad[-15:], rad, rad[:15]]), k, mode="valid")
    px_, py_ = 60 + rad * np.cos(ang), 60 + rad * np.sin(ang)
    seg = np.hypot(np.diff(np.append(px_, px_[0])), np.diff(np.append(py_, py_[0])))
    cum = np.concatenate([[0], np.cumsum(seg)])
    n = int(cum[-1])
    xs = np.interp(np.arange(n), cum, np.append(px_, px_[0]))
    ys = np.interp(np.arange(n), cum, np.append(py_, py_[0]))
    path = []
    for x, y in zip(xs, ys):
        path += [int(round(x)) - 60 & 255, int(round(y)) - 60 & 255]

    def rows(data, n):
        return ",\n    ".join(", ".join(f"0x{b:02X}" for b in data[i:i + n]) for i in range(0, len(data), n))
    out = ROOT / "firmware" / "gen" / "makohai_statue.h"
    out.write_text("/* SPDX-License-Identifier: GPL-3.0-only */\n"
                   "/* generated by tools/draw_statue.py: a marble bust (120 x 120, 4 bits a pixel, 0 = around it) and the\n"
                   " * scroller's text (1 bit a pixel) */\n"
                   f"#define SCROLL_W {w}\n#define SCROLL_H {SCROLL_H}\n"
                   "static const uint8_t STATUE_PX[120 * 120 / 2] = {\n    " + rows(packed, 24) + "\n};\n"
                   f"static const uint8_t SCROLL_BITS[{len(sb)}] = {{\n    " + rows(sb, 24) + "\n};\n"
                   f"#define PATH_N {n}\n"
                   f"static const int8_t STATUE_PATH[PATH_N * 2] = {{\n    " + rows(path, 24).replace("0x", "(int8_t)0x") + "\n};\n")
    small.save(ROOT / "build" / "statue-check.png") if (ROOT / "build").exists() else None
    print("->", out, f"(statue {len(packed)} B, text {w} x {SCROLL_H}, {len(sb)} B)")


if __name__ == "__main__":
    main()
