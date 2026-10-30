"""Renders a textured .glb (from TRELLIS) with the game's camera: orthographic, looking down
from `elev` degrees, turned `azim` degrees around the vertical axis, lit from the upper left.
Run with ComfyUI's Python (it has torch, nvdiffrast and trimesh):

    python_embeded\\python.exe render_iso.py <model.glb> <out.png> [azim=45] [elev=30] [size=1024] [light=1.0]

The result is the building on flat magenta (#FF00FF), ready for make_sprite.py.
azim 0 looks straight at the model's front (the side the source picture showed).
"""
import math
import sys

import numpy as np
import nvdiffrast.torch as dr
import torch
import trimesh
from PIL import Image


def load(path):
    scene = trimesh.load(path)
    mesh = list(scene.geometry.values())[0] if hasattr(scene, "geometry") else scene
    tex = np.asarray(mesh.visual.material.baseColorTexture.convert("RGB"), np.float32) / 255
    return mesh.vertices.astype(np.float32), mesh.faces.astype(np.int32), mesh.visual.uv.astype(np.float32), tex


def render(path, out, azim=45.0, elev=30.0, size=1024, light=1.0):
    v, f, uv, tex = load(path)
    v = v - (v.min(0) + v.max(0)) / 2                       # centre the model
    a, e = math.radians(azim), math.radians(elev)
    # view: turn around Y (up), then tilt down
    ry = np.array([[math.cos(a), 0, math.sin(a)], [0, 1, 0], [-math.sin(a), 0, math.cos(a)]], np.float32)
    rx = np.array([[1, 0, 0], [0, math.cos(e), -math.sin(e)], [0, math.sin(e), math.cos(e)]], np.float32)
    rot = rx @ ry
    p = v @ rot.T
    # centre on screen after the turn and tilt (the 3D centre is not the screen centre, which cut
    # the front corner of the Nubian pyramids off), then fit the view with a small margin
    p = p - (p.min(0) + p.max(0)) / 2
    s = 1.9 / max(np.ptp(p[:, 0]), np.ptp(p[:, 1]))
    p = p * s
    zs = 1.8 / max(np.ptp(p[:, 2]), 1e-6)                    # depth inside the clip range, never clipped
    clip = np.concatenate([p[:, :1], p[:, 1:2], -p[:, 2:3] * zs, np.ones((len(p), 1), np.float32)], 1)

    dev = "cuda"
    ctx = dr.RasterizeCudaContext()
    pos = torch.tensor(clip, device=dev)[None]
    tri = torch.tensor(f, device=dev)
    rast, _ = dr.rasterize(ctx, pos, tri, resolution=[size, size])
    texc, _ = dr.interpolate(torch.tensor(uv, device=dev)[None], rast, tri)
    texc = torch.stack([texc[..., 0], 1 - texc[..., 1]], -1)
    color = dr.texture(torch.tensor(tex, device=dev)[None], texc, filter_mode="linear")

    # flat shading from the face normals in view space; light from the upper left, towards the viewer
    vr = torch.tensor(p, device=dev)
    n = torch.cross(vr[tri[:, 1]] - vr[tri[:, 0]], vr[tri[:, 2]] - vr[tri[:, 0]], dim=1)
    n = n / n.norm(dim=1, keepdim=True).clamp_min(1e-9)
    ldir = torch.tensor([-0.55, 0.65, 0.52], device=dev)
    ldir = ldir / ldir.norm()
    lam = (n @ ldir).abs()                                  # TRELLIS winding is not always outward
    tid = rast[..., 3].long() - 1
    shade = torch.where(tid >= 0, lam[tid.clamp_min(0)], torch.zeros_like(tid, dtype=torch.float32))
    shade = 0.55 + 0.6 * shade * light + (1 - light) * 0.3
    color = color * shade[..., None].clamp(0, 1.25)
    color = dr.antialias(color, rast, pos, tri)

    mask = (rast[..., 3] > 0).float()[0].cpu().numpy()
    img = color[0].clamp(0, 1).cpu().numpy()
    img = np.flipud(img)
    mask = np.flipud(mask)
    bg = np.array([1.0, 0, 1.0], np.float32)
    rgb = img * mask[..., None] + bg * (1 - mask[..., None])
    Image.fromarray((rgb * 255).round().astype(np.uint8)).save(out)
    print(out, f"azim {azim} elev {elev}")


if __name__ == "__main__":
    args = sys.argv[3:]
    render(sys.argv[1], sys.argv[2], *(float(x) for x in args[:2]), *(int(x) for x in args[2:3]), *(float(x) for x in args[3:4]))
