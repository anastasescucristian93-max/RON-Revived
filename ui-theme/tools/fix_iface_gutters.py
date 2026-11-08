r"""Removes the pink seams between tiled interface pieces (the user, 2026-11-08).

The game's interface atlas art\iface_AS0_assets.tga (TEX_CIV) keeps its pieces apart with
1-pixel magenta gutters (255, 0, 255, opaque). Drawn enlarged, the texture filter blends the
gutter into a piece's edge, so a piece repeated along a row (the stone frieze "bar" over the
bottom interface) shows a thin pink line at every joint. Every magenta pixel takes the colour of
its non-magenta neighbours instead (opaque ones first; only transparent ones: transparent).
No piece uses magenta on purpose: three rects (building_decoration2, help_button,
thin_vert_bar) reach one pixel into a gutter and lose their pink edge too.

The file is patched in place in a copy, keeping the game's own header, orientation and footer.

    python fix_iface_gutters.py [game folder]

Writes ui-theme/mod_override/art/iface_AS0_assets.tga.
"""
import os
import struct
import sys

GAME = sys.argv[1] if len(sys.argv) > 1 else r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations"
HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(GAME, "art", "iface_AS0_assets.tga")
DST = os.path.join(HERE, "..", "mod_override", "art", "iface_AS0_assets.tga")


def main():
    data = bytearray(open(SRC, "rb").read())
    id_len, cmap_type, img_type = data[0], data[1], data[2]
    w, h, bpp, desc = struct.unpack_from("<HHBB", data, 12)
    if cmap_type or img_type != 2 or bpp != 32:
        sys.exit(f"unexpected TGA (type {img_type}, {bpp} bpp, colour map {cmap_type})")
    off = 18 + id_len

    def at(x, y):  # byte offset of pixel (x, y), y from the top
        row = y if desc & 0x20 else h - 1 - y
        return off + (row * w + x) * 4

    def get(x, y):
        i = at(x, y)
        b, g, r, a = data[i:i + 4]
        return r, g, b, a

    magenta = lambda p: p == (255, 0, 255, 255)
    todo = [(x, y) for y in range(h) for x in range(w) if magenta(get(x, y))]
    print(f"{len(todo)} magenta pixels in {w}x{h}")
    for _ in range(4):
        new, left = {}, []
        for x, y in todo:
            opaque, clear = [], 0
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    if (dx or dy) and 0 <= x + dx < w and 0 <= y + dy < h:
                        p = get(x + dx, y + dy)
                        if magenta(p) or (x + dx, y + dy) in new:
                            continue
                        if p[3] > 0:
                            # straight neighbours count double: an edge copies its own row / column
                            opaque += [p] * (2 if not (dx and dy) else 1)
                        else:
                            clear += 1
            if opaque:
                n = len(opaque)
                new[(x, y)] = tuple(sum(p[k] for p in opaque) // n for k in range(4))
            elif clear:
                new[(x, y)] = (0, 0, 0, 0)
            else:
                left.append((x, y))
        for (x, y), (r, g, b, a) in new.items():
            data[at(x, y):at(x, y) + 4] = bytes((b, g, r, a))
        todo = left
        if not todo:
            break
    for x, y in todo:
        data[at(x, y):at(x, y) + 4] = bytes(4)
    os.makedirs(os.path.dirname(DST), exist_ok=True)
    open(DST, "wb").write(data)
    print(f"wrote {os.path.normpath(DST)}")


if __name__ == "__main__":
    main()
