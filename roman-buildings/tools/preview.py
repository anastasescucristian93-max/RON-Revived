"""Renders building sprites the way the game shows them: on grass, shadow darkened, player
colour pixels (alpha 248) tinted. Approximation of the game's shading, for comparing.

    python preview.py <out.png> <label>=<sprite.tga>[@<r,g,b>] ...
"""
import sys

from PIL import Image, ImageDraw

GRASS = (96, 104, 70)


def render(path, tint):
    im = Image.open(path).convert("RGBA")
    W, H = im.size
    out = Image.new("RGB", (W, H), GRASS)
    sp, op = im.load(), out.load()
    for y in range(H):
        for x in range(W):
            r, g, b, a = sp[x, y]
            if a == 145:
                op[x, y] = tuple(int(c * 0.55) for c in GRASS)
            elif a == 248:
                op[x, y] = tuple(min(255, r * 2 * t // 255) for t in tint)
            elif a == 255:
                op[x, y] = (r, g, b)
    return out.resize((W * 2, H * 2), Image.LANCZOS)


def main(out_path, items):
    tiles = []
    for item in items:
        label, rest = item.split("=", 1)
        path, _, tint = rest.partition("@")
        tint = tuple(int(v) for v in tint.split(",")) if tint else (255, 255, 255)
        tiles.append((label, render(path, tint)))
    w = max(t.size[0] for _, t in tiles)
    h = sum(t.size[1] + 24 for _, t in tiles)
    sheet = Image.new("RGB", (w, h), (30, 30, 30))
    d = ImageDraw.Draw(sheet)
    y = 0
    for label, t in tiles:
        d.text((6, y + 5), label, fill=(240, 240, 240))
        sheet.paste(t, (0, y + 24))
        y += t.size[1] + 24
    sheet.save(out_path)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:])
