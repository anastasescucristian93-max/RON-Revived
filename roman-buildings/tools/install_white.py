"""Takes Gemini pictures on white to installed nation_art sprites in one go (2026-11-01, the French
and Russian ages 1-3 sets):

    python install_white.py <mod_dir> <nation> <texture.tga> <picture> [<texture.tga> <picture> ...]

Per building: AI matte (rmbg_matte.py under ComfyUI's Python), make_sprite_white.py --matte,
saturated red -> player colour (alpha 248), a footprint check that scales the art down when it
overflows its diamond more than the original (rescale_sprite.py around the diamond's centre),
then clean/<name>.png, nation_art/<nation>/<name>.tga (write_like.py), the copy in the game and
the rule in roman-buildings/nation_art.ini. A sprite that already has a clean/ copy made by hand
(placed, painted) is kept: pass the clean file itself as <picture>.
Run depth_mesh.py for the textures afterwards.
"""
import os
import re
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
GAME = r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations"
COMFY_PY = r"C:\AI\ComfyUI-Easy-Install\ComfyUI-Easy-Install\python_embeded\python.exe"
INI = os.path.join(REPO, "roman-buildings", "nation_art.ini")


def run(*args, py=sys.executable):
    out = subprocess.run([py, *args], capture_output=True, text=True)
    if out.returncode:
        raise SystemExit(out.stderr)
    return out.stdout


def team(path):
    a = np.array(Image.open(path).convert("RGBA"))
    hsv = np.array(Image.fromarray(a[..., :3]).convert("HSV")).astype(int)
    h, s, v = hsv[..., 0], hsv[..., 1], hsv[..., 2]
    red = ((h < 12) | (h > 244)) & (s > 100) & (v > 70) & (a[..., 3] == 255)
    lab, k = ndimage.label(red)
    sizes = ndimage.sum(red, lab, range(1, k + 1))
    red = np.isin(lab, [i + 1 for i, z in enumerate(sizes) if z >= 6])
    g = np.clip(a[..., :3].astype(int).mean(-1) * 95 // 70, 40, 140).astype(np.uint8)
    a[red, 0] = a[red, 1] = a[red, 2] = g[red]
    a[red, 3] = 248
    Image.fromarray(a).save(path)
    return int(red.sum())


def add_rule(nation, tex, rel):
    text = open(INI, encoding="utf-8").read()
    nl = "\r\n" if "\r\n" in text else "\n"
    lines = text.split(nl)
    start = next(i for i, l in enumerate(lines) if l.strip().lower() == f"[{nation}]")
    end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith("[")), len(lines))
    rule = f"{tex} = {rel}"
    for i in range(start + 1, end):
        if re.match(rf"\s*{re.escape(tex)}\s*=", lines[i], re.I):
            lines[i] = rule
            break
    else:
        j = end
        while j > start + 1 and not lines[j - 1].strip():
            j -= 1
        lines.insert(j, rule)
    open(INI, "w", encoding="utf-8", newline="").write(nl.join(lines))


def main(mod, nation, pairs):
    work, clean = os.path.join(mod, "work"), os.path.join(mod, "clean")
    os.makedirs(work, exist_ok=True)
    os.makedirs(clean, exist_ok=True)
    os.makedirs(os.path.join(mod, "nation_art", nation), exist_ok=True)
    os.makedirs(os.path.join(GAME, "nation_art", nation), exist_ok=True)
    for tex, pic in pairs:
        name = tex[:-4]
        orig = os.path.join(mod, "for-gemini", f"{name}__original.tga")
        if not os.path.exists(orig):
            shutil.copy(os.path.join(GAME, "art", tex), orig)
        out = os.path.join(clean, f"{name}.png")
        if os.path.abspath(pic) != os.path.abspath(out):
            matte = os.path.join(work, f"{name}_matte.png")
            run(os.path.join(HERE, "rmbg_matte.py"), pic, matte, py=COMFY_PY)
            raw = os.path.join(work, f"{name}_white.png")
            run(os.path.join(HERE, "make_sprite_white.py"), pic, orig, raw, "--matte", matte)
            n_team = team(raw)
            rep = run(os.path.join(HERE, "footprint_check.py"), tex, os.path.join(work, f"fp_{name}.png"), orig, raw)
            scale = float(re.findall(r"scale ([\d.]+)", rep)[-1])
            cx, cy = map(int, re.search(r"centre \((\d+), (\d+)\)", rep).groups())
            if scale < 0.97:
                run(os.path.join(HERE, "rescale_sprite.py"), raw, out, f"{scale:.2f}", str(cx), str(cy))
            else:
                shutil.copy(raw, out)
            print(f"{tex}: player colour {n_team} px, scale {scale if scale < 0.97 else 1.0}")
        rel = f"nation_art\\{nation}\\{tex}"
        dst = os.path.join(mod, "nation_art", nation, tex)
        run(os.path.join(HERE, "write_like.py"), out, orig, dst)
        shutil.copy(dst, os.path.join(GAME, rel))
        add_rule(nation, tex, rel)
    shutil.copy(INI, os.path.join(GAME, "nation_art.ini"))


if __name__ == "__main__":
    a = sys.argv[1:]
    main(a[0], a[1], list(zip(a[2::2], a[3::2])))
