#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Fail when the app and the platform declare a file-scope variable of the same name.

espada.c includes the whole platform (platform.c, usb.c, ota.c, ...) into one translation unit. Two
`static volatile uint32_t mo_w;` there are not an error in C: they are tentative definitions of ONE variable,
so the app and the USB driver silently share it. Host mode cannot see it (tests/host.c stands in for the
platform). This check can.

  python3 tools/check_names.py
"""
import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "firmware" / "src"
APP = sorted(SRC.glob("espada*.c"))
PLATFORM = [SRC / n for n in ("platform.c", "usb.c", "ota.c", "lcd.c", "gfx.c", "libc.c", "recovery.c")] + \
    [SRC.parent.parent / "web/emu/espada_web.c", SRC.parent.parent / "tests/host.c"]   # the stand-ins: the same unit


def file_scope_vars(path):
    """names of file-scope variables (static or not) declared at column 0"""
    names = set()
    text = re.sub(r"/\*.*?\*/", "", path.read_text(encoding="utf-8"), flags=re.S)
    for line in text.splitlines():
        if not re.match(r"(static|volatile|const|unsigned|signed|uint\d+_t|int\d+_t|int|char|short|long)\b", line):
            continue
        head = re.split(r"[;{]", line)[0]
        head = re.sub(r"__attribute__\s*\(\(.*?\)\)", "", head)
        if "(" in head.split("=")[0]:                       # a function, not a variable
            continue
        # declarators: drop initialisers (also braces) and array sizes, keep the last word of each
        decl = re.sub(r"=.*", "", head)
        for part in decl.split(","):
            part = re.sub(r"\[.*?\]", "", part).strip()
            m = re.search(r"(\w+)\s*$", part)
            if m:
                names.add(m.group(1))
    return names


def main():
    app = {n: p.name for p in APP for n in file_scope_vars(p)}
    plat = {n: p.name for p in PLATFORM if p.exists() for n in file_scope_vars(p)}
    clash = sorted(set(app) & set(plat))
    for n in clash:
        print(f"{n}: {app[n]} and {plat[n]} (one variable in the firmware: rename the app's)")
    if clash:
        sys.exit(1)
    print(f"names: {len(app)} app variables, none shared with the platform")


if __name__ == "__main__":
    main()
