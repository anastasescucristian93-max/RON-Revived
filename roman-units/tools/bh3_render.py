"""Renders a Rise of Nations .bh3 model with its texture to a PNG, for use as a reference image
(Gemini, Tripo). Static mesh only: the pose stored in the file, no animation.

    python bh3_render.py <model.bh3> <texture.tga> <out.png> [azimuth] [elevation] [size]

BH3 layout (as far as needed here): nested chunks of [u32 size incl. header][u16 type][u16 n].
Type 1 holds the mesh: 2 = vertices (u32 count, count x float4), 3 = normals, 4 = UVs
(u32 count, count x float2), 5 = triangles (u32 index count, u16 indices). Type 6 = nodes.
"""
import math
import struct
import sys

import numpy as np
from PIL import Image


def chunks(d, off, end):
    while off + 8 <= end:
        size, typ, n = struct.unpack_from("<IHH", d, off)
        if size < 8 or off + size > end:
            return
        yield off, size, typ, n
        off += size


def load(path):
    d = open(path, "rb").read()
    mesh = {}
    for off, size, typ, n in chunks(d, 8, len(d)):
        if typ != 1:
            continue
        for coff, csize, ctyp, _ in chunks(d, off + 8, off + size):
            count = struct.unpack_from("<I", d, coff + 8)[0]
            if ctyp == 2:
                mesh["v"] = np.frombuffer(d, np.float32, count * 4, coff + 12).reshape(count, 4)[:, :3]
            elif ctyp == 4:
                mesh["uv"] = np.frombuffer(d, np.float32, count * 2, coff + 12).reshape(count, 2)
            elif ctyp == 5:
                mesh["f"] = np.frombuffer(d, np.uint16, count, coff + 12).reshape(-1, 3)
        break
    return mesh


def render(mesh, tex, az, el, size, bg=(255, 255, 255)):
    v, uv, f = mesh["v"].astype(np.float64), mesh["uv"], mesh["f"]
    a, e = math.radians(az), math.radians(el)
    rz = np.array([[math.cos(a), -math.sin(a), 0], [math.sin(a), math.cos(a), 0], [0, 0, 1]])
    rx = np.array([[1, 0, 0], [0, math.cos(e), -math.sin(e)], [0, math.sin(e), math.cos(e)]])
    p = v @ rz.T @ rx.T  # x right, y into the screen (depth), z up
    span = max(np.ptp(p[:, 0]), np.ptp(p[:, 2])) * 1.08
    cx, cz = (p[:, 0].max() + p[:, 0].min()) / 2, (p[:, 2].max() + p[:, 2].min()) / 2
    sx = (p[:, 0] - cx) / span * size + size / 2
    sy = size / 2 - (p[:, 2] - cz) / span * size
    depth = p[:, 1]

    t = np.asarray(tex.convert("RGB"), np.float64)
    th, tw = t.shape[:2]
    img = np.zeros((size, size, 3)) + bg
    zbuf = np.full((size, size), np.inf)
    light = np.array([-0.4, -0.5, 0.75])
    light /= np.linalg.norm(light)
    for tri in f:
        n = np.cross(v[tri[1]] - v[tri[0]], v[tri[2]] - v[tri[0]])
        ln = np.linalg.norm(n)
        if ln == 0:
            continue
        shade = 0.45 + 0.55 * abs(n @ light / ln)  # two-sided: winding is not trusted
        x, y = sx[tri], sy[tri]
        x0, x1 = max(0, int(x.min())), min(size - 1, int(x.max()) + 1)
        y0, y1 = max(0, int(y.min())), min(size - 1, int(y.max()) + 1)
        if x0 > x1 or y0 > y1:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        den = (y[1] - y[2]) * (x[0] - x[2]) + (x[2] - x[1]) * (y[0] - y[2])
        if abs(den) < 1e-9:
            continue
        w0 = ((y[1] - y[2]) * (gx - x[2]) + (x[2] - x[1]) * (gy - y[2])) / den
        w1 = ((y[2] - y[0]) * (gx - x[2]) + (x[0] - x[2]) * (gy - y[2])) / den
        w2 = 1 - w0 - w1
        inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if not inside.any():
            continue
        z = w0 * depth[tri[0]] + w1 * depth[tri[1]] + w2 * depth[tri[2]]
        zb = zbuf[y0:y1 + 1, x0:x1 + 1]
        draw = inside & (z < zb)
        if not draw.any():
            continue
        u = w0 * uv[tri[0], 0] + w1 * uv[tri[1], 0] + w2 * uv[tri[2], 0]
        vv = w0 * uv[tri[0], 1] + w1 * uv[tri[1], 1] + w2 * uv[tri[2], 1]
        tx = np.clip((u % 1) * tw, 0, tw - 1).astype(int)
        ty = np.clip((vv % 1) * th, 0, th - 1).astype(int)
        col = t[ty, tx] * shade
        img[y0:y1 + 1, x0:x1 + 1][draw] = col[draw]
        zb[draw] = z[draw]
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8))


if __name__ == "__main__":
    model, texture, out = sys.argv[1:4]
    az = float(sys.argv[4]) if len(sys.argv) > 4 else 35
    el = float(sys.argv[5]) if len(sys.argv) > 5 else 35
    size = int(sys.argv[6]) if len(sys.argv) > 6 else 1024
    render(load(model), Image.open(texture), az, el, size).save(out)
    print(out)
