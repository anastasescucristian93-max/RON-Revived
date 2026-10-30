"""Exports building sprites from the game for editing:
    <name>__preview_3x.png     as in the game, on grass, 3x
    <name>__EDIT_THIS_3x.png   building only, transparent background, 3x
    <name>__mask_..._3x.png    player colour (blue) and shadow (red)
    <name>__original.tga       untouched copy

    python export_sprites.py <out_dir> <art_file.tga>=<name> ...
"""
import os
import shutil
import sys

from PIL import Image

ART = r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations\art"


def export(out, src, name):
    path = os.path.join(ART, src)
    im = Image.open(path).convert("RGBA")
    a = im.getchannel("A")
    W, H = im.size
    shutil.copy(path, os.path.join(out, name + "__original.tga"))
    shade = a.point(lambda v: 110 if v == 145 else 0)
    view = Image.composite(Image.new("RGBA", im.size, (0, 0, 0, 255)), Image.new("RGBA", im.size, (96, 104, 70, 255)), shade)
    body = im.copy()
    body.putalpha(a.point(lambda v: 255 if v in (248, 255) else 0))
    view.alpha_composite(body)
    view.resize((W * 3, H * 3), Image.LANCZOS).save(os.path.join(out, name + "__preview_3x.png"))
    body.resize((W * 3, H * 3), Image.NEAREST).save(os.path.join(out, name + "__EDIT_THIS_3x.png"))
    m = Image.new("RGB", im.size)
    mp, ap = m.load(), a.load()
    colours = {255: (200, 200, 200), 248: (40, 110, 255), 145: (230, 40, 40)}
    for y in range(H):
        for x in range(W):
            mp[x, y] = colours.get(ap[x, y], (40, 40, 40))
    m.resize((W * 3, H * 3), Image.NEAREST).save(os.path.join(out, name + "__mask_teamcolor-blue_shadow-red_3x.png"))
    print(f"{name}: {W}x{H}")


if __name__ == "__main__":
    os.makedirs(sys.argv[1], exist_ok=True)
    for item in sys.argv[2:]:
        src, name = item.split("=", 1)
        export(sys.argv[1], src, name)
