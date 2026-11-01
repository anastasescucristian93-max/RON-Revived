"""Writes a sprite as a .tga with exactly the original file's header and pixel layout
(uncompressed 32-bit, same origin), so the game's loader reads it like its own files.

    python write_like.py <sprite.tga> <original.tga> <out.tga> [--any-size]

--any-size: the sprite may be bigger than the original (a canvas grown for bigger art; its
nation_art.ini rule then says how much further left and up it reaches, see nation_art.c).
"""
import struct
import sys

from PIL import Image


def main(src, original, out, any_size=False):
    hdr = open(original, "rb").read(18)
    id_len, cmap_type, img_type = hdr[0], hdr[1], hdr[2]
    w, h, bpp, desc = struct.unpack("<HHBB", hdr[12:18])
    if img_type not in (2, 10) or bpp != 32 or cmap_type != 0:
        sys.exit(f"unexpected original format: type {img_type}, {bpp} bpp")
    im = Image.open(src).convert("RGBA")
    if im.size != (w, h):
        if not any_size:
            sys.exit(f"size {im.size} does not match original {(w, h)}")
        w, h = im.size
        hdr = hdr[:12] + struct.pack("<HHBB", w, h, bpp, desc)
    top_down = bool(desc & 0x20)
    rows = range(h) if top_down else range(h - 1, -1, -1)
    px = im.load()
    data = bytearray()
    for y in rows:
        line = [bytes((px[x, y][2], px[x, y][1], px[x, y][0], px[x, y][3])) for x in range(w)]
        if img_type == 2:
            data += b"".join(line)
            continue
        # RLE (type 10), packets never cross a scanline: run = 0x80|n-1 + pixel, raw = n-1 + pixels
        x = 0
        while x < w:
            run = 1
            while x + run < w and run < 128 and line[x + run] == line[x]:
                run += 1
            if run > 1:
                data += bytes((0x80 | (run - 1),)) + line[x]
                x += run
                continue
            raw = 1
            while x + raw < w and raw < 128 and not (x + raw + 1 < w and line[x + raw] == line[x + raw + 1]):
                raw += 1
            data += bytes((raw - 1,)) + b"".join(line[x:x + raw])
            x += raw
    ident = open(original, "rb").read(18 + id_len)[18:]
    with open(out, "wb") as f:
        f.write(hdr[:18] + ident + bytes(data))
    print(f"{out}: {w}x{h}, {'top-down' if top_down else 'bottom-up'}")


if __name__ == "__main__":
    main(*sys.argv[1:4], any_size="--any-size" in sys.argv)
