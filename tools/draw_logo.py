#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Makohai's logo as SVG (assets/logo.svg): render it at 1024 x 1024 to assets/logo.png, then run tools/make_logo.py.
Usage: draw_logo.py [ACCENT_COLOUR] [OUT.svg]"""
# The look:
# black ground, a ring, audio bars, a line-art fish in one accent colour + cream,
# music notes, a little three-knob box, a wide italic name in cream.
import math, sys

ACC = sys.argv[1] if len(sys.argv) > 1 else "#ff6a3d"   # the accent colour
OUT = sys.argv[2] if len(sys.argv) > 2 else str(__import__("pathlib").Path(__file__).resolve().parent.parent / "assets" / "logo.svg")
CREAM, INK, BG = "#f2ead0", "#050608", "#000000"
W = H = 1024
CX, CY, R = 512, 440, 362

# ---- letters (6 x 7 grid, I is 1.8 wide), as before but rounder chamfers
L = {
    "M": [[(0,7),(0,0.9),(0.9,0),(1.9,0),(3,1.9),(4.1,0),(5.1,0),(6,0.9),(6,7),(4.3,7),(4.3,3.4),(3,5.3),(1.7,3.4),(1.7,7)]],
    "A": [[(0,7),(0,1.6),(1.6,0),(4.4,0),(6,1.6),(6,7),(4.3,7),(4.3,4.7),(1.7,4.7),(1.7,7)],
          [(1.7,1.7),(4.3,1.7),(4.3,3.1),(1.7,3.1)]],
    "K": [[(0,0),(1.7,0),(1.7,2.7),(4,0),(6.1,0),(3.3,3.5),(6.1,7),(4,7),(1.7,4.3),(1.7,7),(0,7)]],
    "O": [[(0,1.6),(1.6,0),(4.4,0),(6,1.6),(6,5.4),(4.4,7),(1.6,7),(0,5.4)],
          [(1.7,1.7),(4.3,1.7),(4.3,5.3),(1.7,5.3)]],
    "H": [[(0,0),(1.7,0),(1.7,2.65),(4.3,2.65),(4.3,0),(6,0),(6,7),(4.3,7),(4.3,4.35),(1.7,4.35),(1.7,7),(0,7)]],
    "I": [[(0,0),(1.7,0),(1.7,7),(0,7)]],
}
WID = {c: (1.7 if c == "I" else 6) for c in L}
GAP, WORD, SK = 0.7, "MAKOHAI", 0.24
total = sum(WID[c] for c in WORD) + GAP * (len(WORD) - 1)
U = 15.2
TY0 = 836


def word_path():
    x0 = (W - total * U - 7 * U * SK) / 2
    d, x = [], 0.0
    for c in WORD:
        for poly in L[c]:
            pts = [f"{x0 + (x + px) * U + (7 - py) * U * SK:.1f},{TY0 + py * U:.1f}" for px, py in poly]
            d.append("M" + " L".join(pts) + " Z")
        x += WID[c] + GAP
    return " ".join(d)


# ---- audio bars, a waveform round the centre line, clipped to the ring
bars = []
n = 24
for i in range(n):
    x = 176 + i * 29
    t = i / (n - 1)
    h = 60 + 230 * abs(math.sin(t * 7.3 + 0.6)) * (0.55 + 0.45 * math.sin(t * math.pi))
    bars.append(f'<rect x="{x}" y="{CY - 10 - h * 0.62:.0f}" width="13" height="{h:.0f}" rx="2"/>')

# ---- the mako (facing right, a little nose-up), open jaw
body = ("M 896 352 "
        "C 860 300, 770 270, 670 262 "                # head to the dorsal
        "Q 630 210, 606 150 Q 592 220, 556 266 "        # first dorsal
        "C 460 276, 350 300, 278 322 "                  # back to the keel
        "Q 214 248, 134 186 Q 176 272, 210 344 "        # upper tail lobe
        "Q 178 404, 146 474 Q 220 410, 276 360 "        # lower tail lobe
        "C 350 390, 440 420, 520 426 "                  # belly
        "C 600 430, 680 420, 740 410 "
        "C 800 400, 840 388, 870 378 "                  # throat
        "L 900 372 Z")
jaw_top = "M 896 352 L 838 366 L 900 372"
belly = ("M 870 378 C 840 388, 800 400, 740 410 C 680 420, 600 430, 520 426 C 440 420, 350 390, 276 360 "
         "C 360 368, 460 384, 560 386 C 680 388, 790 376, 860 366 Z")
back_hi = "M 806 304 C 740 284, 640 284, 560 292 C 470 302, 380 314, 300 328"
pect = "M 640 418 Q 600 484, 546 546 Q 630 494, 700 410 Z"
pelvic = "M 470 420 L 446 458 L 506 424 Z"
dorsal2 = "M 360 314 L 344 288 L 392 308 Z"
anal = "M 368 392 L 352 420 L 398 398 Z"
teeth = []
for i in range(6):
    x = 846 + i * 9
    teeth.append(f"M {x:.0f} {365 - i*0.6:.1f} l 4 9 l 4 -9")
    teeth.append(f"M {x+2:.0f} {370 + i*0.4:.1f} l 4 -8 l 4 8")
gills = "".join(f'<path d="M {760 - k*14} 314 q -12 34 -2 70"/>' for k in range(5))


def note(x, y, s, double=False):
    """An eighth (or a pair of eighths) note."""
    g = [f'<ellipse cx="{x}" cy="{y}" rx="{13*s}" ry="{9.5*s}" transform="rotate(-22 {x} {y})"/>',
         f'<rect x="{x + 9*s:.1f}" y="{y - 58*s:.1f}" width="{4.5*s:.1f}" height="{58*s:.1f}"/>']
    if double:
        x2 = x + 44 * s
        g += [f'<ellipse cx="{x2}" cy="{y - 6*s}" rx="{13*s}" ry="{9.5*s}" transform="rotate(-22 {x2} {y - 6*s})"/>',
              f'<rect x="{x2 + 9*s:.1f}" y="{y - 64*s:.1f}" width="{4.5*s:.1f}" height="{58*s:.1f}"/>',
              f'<polygon points="{x + 9*s:.1f},{y - 58*s:.1f} {x2 + 13.5*s:.1f},{y - 64*s:.1f} {x2 + 13.5*s:.1f},{y - 52*s:.1f} {x + 9*s:.1f},{y - 46*s:.1f}"/>']
    else:
        g.append(f'<path d="M {x + 13.5*s:.1f} {y - 58*s:.1f} q {22*s:.1f} {14*s:.1f} {16*s:.1f} {38*s:.1f} '
                 f'q {-2*s:.1f} {-16*s:.1f} {-16*s:.1f} {-22*s:.1f} z"/>')
    return "".join(g)


def knob(x, y):
    ticks = "".join(
        f'<line x1="{x + 30*math.cos(a):.1f}" y1="{y + 30*math.sin(a):.1f}" x2="{x + 37*math.cos(a):.1f}" y2="{y + 37*math.sin(a):.1f}"/>'
        for a in [math.radians(d) for d in range(135, 406, 45)])
    return (f'<g stroke="{INK}" stroke-width="3" stroke-linecap="round">{ticks}</g>'
            f'<circle cx="{x}" cy="{y}" r="22" fill="{ACC}" stroke="{INK}" stroke-width="5"/>'
            f'<line x1="{x}" y1="{y}" x2="{x}" y2="{y - 16}" stroke="{INK}" stroke-width="5" stroke-linecap="round"/>')


word = word_path()
BX, BY, BW, BH = 512 - 150, 600, 300, 104
svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">
<defs><clipPath id="ring"><circle cx="{CX}" cy="{CY}" r="{R - 14}"/></clipPath></defs>
<rect width="{W}" height="{H}" fill="{BG}"/>
<circle cx="{CX}" cy="{CY}" r="{R}" fill="#07090d" stroke="{ACC}" stroke-width="11"/>
<g clip-path="url(#ring)" fill="{ACC}">{"".join(bars)}</g>

<!-- the box with three knobs, wired to the ring -->
<path d="M {BX} {BY + BH/2} C {BX - 80} {BY + BH/2}, {BX - 110} {BY - 20}, {BX - 120} {BY - 70}" fill="none" stroke="{ACC}" stroke-width="7"/>
<path d="M {BX + BW} {BY + BH/2} C {BX + BW + 80} {BY + BH/2}, {BX + BW + 110} {BY - 20}, {BX + BW + 120} {BY - 70}" fill="none" stroke="{ACC}" stroke-width="7"/>
<rect x="{BX}" y="{BY}" width="{BW}" height="{BH}" rx="16" fill="{CREAM}" stroke="{ACC}" stroke-width="9"/>
{knob(BX + 70, BY + 54)}{knob(BX + 150, BY + 54)}{knob(BX + 230, BY + 54)}

<!-- the mako -->
<g transform="rotate(-7 520 360) translate(-6 12)">
  <path d="{body}" fill="none" stroke="{BG}" stroke-width="26" stroke-linejoin="round"/>
  <path d="{dorsal2}" fill="{INK}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <path d="{anal}" fill="{INK}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <path d="{body}" fill="{INK}" stroke="{ACC}" stroke-width="10" stroke-linejoin="round"/>
  <path d="{belly}" fill="{CREAM}"/>
  <path d="{back_hi}" fill="none" stroke="{CREAM}" stroke-width="7" stroke-linecap="round"/>
  <path d="{back_hi}" fill="none" stroke="{INK}" stroke-width="2" stroke-dasharray="3 9" transform="translate(0 1)"/>
  <path d="M 548 274 Q 590 226, 606 170" fill="none" stroke="{CREAM}" stroke-width="5" stroke-linecap="round"/>
  <path d="M 262 318 Q 210 262, 162 214" fill="none" stroke="{CREAM}" stroke-width="5" stroke-linecap="round"/>
  <path d="{pect}" fill="{CREAM}" stroke="{ACC}" stroke-width="7" stroke-linejoin="round"/>
  <path d="M 646 424 L 582 502 M 662 422 L 604 490 M 678 418 L 626 476" stroke="{INK}" stroke-width="3" opacity="0.55"/>
  <path d="{pelvic}" fill="{CREAM}" stroke="{ACC}" stroke-width="5" stroke-linejoin="round"/>
  <g fill="none" stroke="{ACC}" stroke-width="5" stroke-linecap="round">{gills}</g>
  <path d="{jaw_top}" fill="{BG}" stroke="{ACC}" stroke-width="5" stroke-linejoin="round"/>
  <path d="{"".join(teeth)}" fill="none" stroke="{CREAM}" stroke-width="3" stroke-linejoin="round"/>
  <circle cx="818" cy="322" r="17" fill="{CREAM}" stroke="{ACC}" stroke-width="5"/>
  <circle cx="822" cy="321" r="8" fill="{INK}"/>
</g>

<g fill="{CREAM}">{note(756, 214, 0.75)}{note(806, 180, 0.9, True)}{note(874, 254, 0.7)}</g>

<path d="{word}" fill="none" stroke="{BG}" stroke-width="22" stroke-linejoin="round"/>
<path d="{word}" fill="{CREAM}" fill-rule="evenodd"/>
</svg>'''
open(OUT, "w").write(svg)
print("ok", OUT)
