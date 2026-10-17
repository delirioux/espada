#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make Espada's site (GitHub Pages, .github/workflows/firmware.yml):

  index.html                    the start page: the logo, Install, Source
  install/index.html            web/espada_installer.html, self-contained (fm1pkg.js and fm1ota.js inlined, as
                                Felucca's installer does)
  firmware/espada-VER.fwsc      the package (+ LICENSE, LICENSING.md, LICENSES/: it holds JieLi SDK files under
                                Apache-2.0, see LICENSING.md)
  espada-logo.png

  python3 web/espada_make_site.py build/espada.fwsc VERSION OUT_DIR

The package must carry the platform's own loader (FELUCCA-LOADER-1, no vendor files); its identity (FM-1_9xx) is what
the FM-1 must report after the install.
"""
import json
import re
import shutil
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F                  # the package's identity bytes (fm1pkg.js productOf)


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def strip_module(src):
    """an ES module as plain script, to inline it"""
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a package of the platform (FM-1_9xx)")
    if b"FELUCCA-LOADER-1" not in raw:              # the platform's loader: never publish a package with vendor files
        raise SystemExit(f"{pkg}: no platform loader in it; the site ships only packages built by tools/build.py")
    html = (HERE / "espada_installer.html").read_text(encoding="utf-8")
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8"))
    safe = re.sub(r"[^A-Za-z0-9.-]", "-", version)
    name = f"espada-{safe}.fwsc"
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"espada_installer.html must contain {mark} once")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)

    inst, fw = out / "install", out / "firmware"
    for d in (inst, fw, fw / "LICENSES"):
        d.mkdir(parents=True, exist_ok=True)
    for old in fw.glob("espada-*.fwsc"):            # one package: the current one
        old.unlink()
    (inst / "index.html").write_text(html, encoding="utf-8")
    shutil.copy(pkg, fw / name)

    # the licence texts that go with the package
    texts = sorted((ROOT / "LICENSES").glob("*.txt")) + [ROOT / "assets/fonts/Terminus-LICENSE.txt"]
    for t in texts:
        shutil.copy(t, fw / "LICENSES" / t.name)
    names = [t.name for t in texts]
    (fw / "LICENSES" / "index.html").write_text(
        '<!doctype html><meta charset="utf-8"><title>Espada licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(ROOT / doc, fw / doc)

    # the logo, small enough for a page
    Image.open(ROOT / "assets/logo.png").convert("RGB").resize((256, 256), Image.LANCZOS) \
        .save(out / "espada-logo.png", optimize=True)

    (out / "index.html").write_text(f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Espada</title><link rel="icon" href="espada-logo.png">
<meta name="description" content="Generative tracker grooves for the FM-1, razor sharp.">
<style>
  :root {{ color-scheme: dark; }}
  body {{ margin: 0; background: #050707; color: #efe6c8; font: 16px/1.6 -apple-system, BlinkMacSystemFont,
         "Segoe UI", Arial, sans-serif; }}
  main {{ max-width: 540px; margin: 0 auto; padding: 40px 16px; text-align: center; }}
  img {{ width: 240px; height: 240px; border-radius: 12px; }}
  h1 {{ font-size: 28px; margin: 12px 0 4px; letter-spacing: .08em; }}
  p {{ color: #8f9a96; margin: 0 0 24px; }}
  a {{ color: #3fe0e0; }}
  .b {{ display: inline-block; margin: 6px; padding: 10px 22px; border-radius: 6px; background: #3fe0e0;
        color: #050707; font-weight: 600; text-decoration: none; }}
  small {{ display: block; margin-top: 28px; color: #8f9a96; }}
</style></head>
<body><main>
<img src="espada-logo.png" alt="Espada">
<h1>ESPADA</h1>
<p>Generative tracker grooves for the M-VAVE FM-1, razor sharp. Four lanes in the style of early-90s PC tracker
music, on FM: queue a profile, blend it in, call the drop.</p>
<a class="b" href="install/">Install on the FM-1</a>
<small>Espada {version} · GPL-3.0 · <a href="https://github.com/delirioux/espada">Source</a> ·
<a href="https://github.com/delirioux/espada/blob/main/docs/MANUAL.md">Manual</a><br>
On the FM-1 platform of Felucca (Leo Kuroshita, Hügelton Instruments), SLOOP and Dinghy</small>
</main></body></html>
""", encoding="utf-8")
    print(f"site: {out}: install ({len(html)} B), firmware/{name} ({len(raw)} B, {product})")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
