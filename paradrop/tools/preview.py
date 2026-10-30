r"""Renders a textured model (GLB/OBJ) from four sides into one sheet, for checking.

    python preview.py <model> <out.png> [size]

Axes as Tripo exports them: X length, Y up, Z width. Uses the software renderer of
roman-units\tools\bh3_render.py (Z up, texture V from the top).
"""
import os
import sys

import numpy as np
import trimesh
from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "roman-units", "tools"))
from bh3_render import render  # noqa: E402

VIEWS = [(35, 30), (90, 5), (0, 5), (215, 30)]  # 3/4 front, side, front, 3/4 back


def load(path):
    m = trimesh.load(path, force="mesh")
    v = np.asarray(m.vertices, np.float64)
    tex = m.visual.material.baseColorTexture if hasattr(m.visual.material, "baseColorTexture") else m.visual.material.image
    uv = np.asarray(m.visual.uv, np.float64).copy()
    uv[:, 1] = 1 - uv[:, 1]
    # Y up -> Z up, nose (+X) to the camera's left at azimuth 0
    return {"v": np.c_[v[:, 0], -v[:, 2], v[:, 1]], "uv": uv, "f": np.asarray(m.faces)}, tex


def sheet(mesh, tex, size):
    tex = tex.convert("RGB").resize((1024, 1024))
    out = Image.new("RGB", (size * len(VIEWS), size), "white")
    for i, (az, el) in enumerate(VIEWS):
        out.paste(render(mesh, tex, az, el, size), (i * size, 0))
    return out


if __name__ == "__main__":
    mesh, tex = load(sys.argv[1])
    size = int(sys.argv[3]) if len(sys.argv) > 3 else 512
    sheet(mesh, tex, size).save(sys.argv[2])
    print("%s: %d triangles" % (sys.argv[2], len(mesh["f"])))
