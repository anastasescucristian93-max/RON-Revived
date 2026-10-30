r"""The Data Center Expansions' art from one Gemini image (gemini\expansion.jpg): the same tower for
all three, its lights in the kind's colour, the lights flickering like a server's and the roof
fans turning.

    python expansion_art.py [gemini image]

Writes, for every kind, into work\art: dc_exp_<kind>.tga (frame 0), dc_exp_<kind>_<f>.tga for
f = 0 .. FRAMES - 1 (nation_art.c plays them in a loop: rules in roman-buildings\nation_art.ini,
[all]), dc_exp_<kind>z.bh3 (depth mesh) and work\offsets_dc_exp_<kind>.txt; previews in work\.
Then run build_data.py.

The image is the edited camera template (make_expansion.py template): the tower's concrete plinth
is the one-tile footprint. Everything that changes between frames is done on the full-size image
(1024 px); each frame is then cut out and scaled the same way, and gets the same wear.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter
from scipy import ndimage
from scipy.spatial import ConvexHull

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
sys.path.insert(0, os.path.join(ROOT, "roman-buildings", "tools"))
sys.path.insert(0, os.path.join(ROOT, "entrenchment", "tools"))
sys.path.insert(0, os.path.join(ROOT, "paradrop", "tools"))
import make_sprite as ms  # noqa: E402
from make_expansion import box_mesh, A, C, E  # noqa: E402
from make_nest import keyed, write_bh3, write_tga  # noqa: E402
from weather import weather  # noqa: E402

GEMINI = os.path.join(HERE, "..", "gemini", "expansion.jpg")
WORK = os.path.join(HERE, "..", "work")
WORK_ART = os.path.join(WORK, "art")
TEAM, SHADOW, SOLID = 248, 145, 255

FRAMES = 24            # one loop (one breath, one turn of the fans); nation_art.ini plays it in 2.4 s
TILE_HALF = 21.0       # world units from the centre to a side of one tile (a 2x2 building is 42)
PLINTH_FILL = 0.96     # the plinth's width as a share of the tile's
MARGIN = 8             # sprite pixels around the tower; more is added on the left for the shadow
# The game's sun: a point E * z pixels above the ground casts its shadow SUN times that away on
# screen, to the left and a little up (towards the back). Fitted on the game's towers, lookout and
# temple: their depth meshes cast this way match the shadows painted in their sprites.
SUN = (-0.78, -0.33)
PLINTH_HEIGHT = 11.0   # world units, measured on the sprite
BODY_FILL = 0.82       # the body's width as a share of the tile's (narrower than the plinth)
SEED = 7
BREATH_LOW = 0.12      # the lights' brightness at the bottom of a breath
WEAR = 0.5             # weather.py strength: the Gemini image is already worn, so light

# kind -> colour of its lights (the image's own are gold: commerce keeps them)
KINDS = {
    "commerce": None,
    "population": (80, 255, 120),
    "efficiency": (70, 170, 255),
}

# The roof fans in the image: centre and radii (the opening of each fan, an ellipse: a circle on
# the flat roof seen by the camera); the blades inside FAN_INNER of it turn. One full turn per
# loop: Gemini's five blades are not exactly alike, so a part of a turn jumps when the loop
# starts again.
FANS = [(510, 139), (407, 186), (613, 186), (510, 236)]
FAN_RX, FAN_RY, FAN_INNER = 46.0, 25.0, 0.80


def lights_mask(rgb, alpha):
    """The lit parts (LED rows, strips): bright, warm, not the red band. Their edges next to the
    background are tinted by the magenta, so blue is allowed to be high."""
    r, g, b = (rgb[..., i].astype(int) for i in range(3))
    m = (r > 200) & (g > 150) & (r - b > 45) & (g > 0.6 * r) & (alpha > 128)
    return ndimage.binary_closing(m, iterations=1)


def glow_mask(rgb, alpha, lights):
    """The lights' warm glow on the walls around them, 0..1."""
    r, g, b = (rgb[..., i].astype(np.float32) for i in range(3))
    near = ndimage.binary_dilation(lights, iterations=18)
    warm = np.clip((r - b - 10) / 45, 0, 1)
    red = (g < 0.55 * r) & (r - g > 40)  # the red band is warm too, but not glow
    return warm * near * (~red) * (~lights) * (alpha > 128)


def recolour(rgb, lights, glow, colour):
    """Lights and glow in another colour, the same brightness."""
    if colour is None:
        return rgb.copy()
    out = rgb.astype(np.float32)
    target = np.array(colour, np.float32)
    lum = out.mean(axis=2, keepdims=True)
    # lights: the colour scaled to the pixel's brightness, the white-hot cores stay white
    hi = out.max(axis=2, keepdims=True)
    white = np.clip((out.min(axis=2, keepdims=True) / np.maximum(hi, 1) - 0.55) / 0.4, 0, 1)
    lit = target / target.max() * hi
    lit = lit * (1 - white) + hi * white
    k = lights[..., None].astype(np.float32)
    out = out * (1 - k) + lit * k
    # glow: the warm tint on the walls becomes a tint of the new colour, as strong
    tint = np.linalg.norm(out - lum, axis=2, keepdims=True)
    t = (target - target.mean()) / np.linalg.norm(target - target.mean())
    g = glow[..., None]
    out = out * (1 - g) + (lum + t * tint) * g
    return np.clip(out, 0, 255)


def flicker_plan(lights, rng):
    """Per light part, its brightness in every frame. Every light breathes: from dim
    (BREATH_LOW) up to full and back, once per loop, smoothly. The small parts (LEDs) also
    twinkle a little on top, like a server's."""
    labels, n = ndimage.label(lights)
    sizes = ndimage.sum(lights, labels, range(1, n + 1))
    t = np.arange(FRAMES) / FRAMES
    breath = BREATH_LOW + (1 - BREATH_LOW) * (0.5 - 0.5 * np.cos(2 * np.pi * t))  # dim at frame 0
    plan = np.tile(breath, (n + 1, 1)).astype(np.float32)
    for i, size in enumerate(sizes, start=1):
        if size < 150:
            plan[i] *= 0.8 + 0.2 * rng.random(FRAMES)
    return labels, plan, breath.astype(np.float32)


def light_frame(rgb, labels, plan, glow, glow_level, f):
    """One frame: every light part at its brightness, dark glass when off; the glow with them."""
    out = rgb.astype(np.float32)
    level = plan[:, f][labels][..., None]
    lit = labels[..., None] > 0
    off = out.mean(axis=2, keepdims=True) * 0.22 + out * 0.08
    out = np.where(lit, off + (out - off) * level, out)
    lum = out.mean(axis=2, keepdims=True)
    g = glow[..., None] * (1 - glow_level[f])
    return out * (1 - g) + lum * 0.92 * g


def turn_fans(rgb, angle):
    """The fan blades turned by `angle` degrees: each pixel inside a fan's opening takes the colour
    from the same radius, `angle` back, on the circle the ellipse is."""
    out = rgb.copy()
    h, w = rgb.shape[:2]
    a = np.radians(angle)
    for cx, cy in FANS:
        x0, x1 = int(cx - FAN_RX - 2), int(cx + FAN_RX + 3)
        y0, y1 = int(cy - FAN_RY - 2), int(cy + FAN_RY + 3)
        yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        u, v = (xx - cx) / FAN_RX, (yy - cy) / FAN_RY
        inside = u * u + v * v < FAN_INNER * FAN_INNER
        su = u * np.cos(a) + v * np.sin(a)
        sv = -u * np.sin(a) + v * np.cos(a)
        sx, sy = cx + su * FAN_RX, cy + sv * FAN_RY
        for c in range(3):
            vals = ndimage.map_coordinates(rgb[..., c].astype(np.float32), [sy, sx], order=1)
            out[y0:y1, x0:x1, c] = np.where(inside, vals, out[y0:y1, x0:x1, c])
    return out


class Cut:
    """Cuts the tower out of the full-size image and scales it to the game, the same for every
    frame: the plinth is one tile wide, the origin is the centre of its base."""

    def __init__(self, alpha):
        ys, xs = np.nonzero(alpha >= 128)
        self.box = (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)
        bottom = ys.max()
        tile_w = 2 * A * 2 * TILE_HALF * PLINTH_FILL       # sprite pixels across the plinth
        self.s = tile_w / (xs.max() - xs.min())
        bw, bh = self.box[2] - self.box[0], self.box[3] - self.box[1]
        self.tw, self.th = round(bw * self.s), round(bh * self.s)
        self.W, self.H = self.tw + 2 * MARGIN, self.th + MARGIN + 2
        self.left, self.top = MARGIN, MARGIN
        bottom_x = xs[ys == bottom].mean()
        self.x0 = round(self.left + (bottom_x - self.box[0]) * self.s)
        # the plinth's lowest point is its front corner: the centre is 2 * C * TILE_HALF higher
        self.y0 = round(self.top + (bottom - self.box[1]) * self.s - 2 * C * TILE_HALF * PLINTH_FILL)
        self.alpha = self.scale(Image.fromarray(alpha))

    def scale(self, img):
        fit = img.crop(self.box).resize((self.tw, self.th), Image.LANCZOS)
        canvas = Image.new(img.mode, (self.W, self.H))
        canvas.paste(fit, (self.left, self.top))
        return canvas

    def widen(self, left, alpha):
        """More room on the left (for the shadow); the origin moves with the picture."""
        self.W += left
        self.left += left
        self.x0 += left
        self.alpha = self.scale(Image.fromarray(alpha))


def cast_shadow(cut, height):
    """The tower's shadow on the ground, cast in the game's sun: the plinth and the body as two
    prisms, each the hull of its base and its base moved by SUN per pixel of its screen height.
    Returns the mask and how many pixels it reaches past the left edge."""
    prisms = ((TILE_HALF * PLINTH_FILL, PLINTH_HEIGHT), (TILE_HALF * BODY_FILL, height))
    hulls = []
    for half, z in prisms:
        base = [(cut.x0 + A * (x - y), cut.y0 + C * (x + y))
                for x, y in ((-half, -half), (half, -half), (half, half), (-half, half))]
        pts = np.array(base + [(sx + SUN[0] * E * z, sy + SUN[1] * E * z) for sx, sy in base])
        hulls.append(pts[ConvexHull(pts).vertices])
    over = max(0, int(np.ceil(-min(h[:, 0].min() for h in hulls))) + 1)
    im = Image.new("L", (cut.W + over, cut.H), 0)
    d = ImageDraw.Draw(im)
    for h in hulls:
        d.polygon([(x + over, y) for x, y in h], fill=255)
    return np.asarray(im) > 0, over


def to_sprite(cut, rgb, team, body, shadow):
    """A full-size frame (premultiplied over black) as a game sprite."""
    pre = cut.scale(Image.fromarray(rgb.astype(np.uint8)))
    p = np.asarray(pre).astype(np.float32)
    a = np.asarray(cut.alpha).astype(np.float32)[..., None]
    col = np.clip(p * 255 / np.maximum(a, 1), 0, 255)
    out = np.zeros((cut.H, cut.W, 4), np.uint8)
    out[..., :3] = col.astype(np.uint8)
    out[..., 3] = np.where(body, SOLID, 0)
    lum = (col * [0.299, 0.587, 0.114]).sum(axis=2)
    grey = np.clip(lum * 95 / 70, 40, 140).astype(np.uint8)
    for c in range(3):
        out[..., c] = np.where(team, grey, out[..., c])
    out[..., 3] = np.where(team, TEAM, out[..., 3])
    sh = shadow & ~body
    out[sh] = (0, 0, 0, SHADOW)
    # thin edges keep magenta from the backdrop: red far above green and blue not below it.
    # Repaint them grey-brown of the same brightness, as make_nest.py does.
    r, g, b = (out[..., i].astype(int) for i in range(3))
    pink = (out[..., 3] == SOLID) & (r - g > 12) & (b > g - 4) & (g > 30)
    out[..., 1] = np.where(pink, r * 80 // 100, g).astype(np.uint8)
    out[..., 2] = np.where(pink, r * 70 // 100, b).astype(np.uint8)
    return out


def team_mask(sprite_rgb, body):
    hsv = np.asarray(Image.fromarray(sprite_rgb).convert("HSV")).astype(int)
    h, s, v = hsv[..., 0], hsv[..., 1], hsv[..., 2]
    red = ((h < 12) | (h > 244)) & (s > 140) & (v > 55) & body
    red = np.asarray(ms.drop_specks(Image.fromarray((red * 255).astype(np.uint8)), ms.MIN_TEAM_AREA)) > 0
    red = ndimage.binary_closing(red, iterations=1) & body
    return red


def main(src):
    big = Image.open(src).convert("RGB")
    a_big = keyed(big)
    big = ms.unmix(big, a_big)
    alpha = np.asarray(a_big)
    rgb = np.asarray(big).astype(np.float32)
    pre_k = (alpha.astype(np.float32) / 255)[..., None]

    cut = Cut(alpha)
    body = np.asarray(cut.alpha) >= 128
    # the depth box's height: as high as the sprite reaches above the base's centre
    top = np.nonzero(body.any(axis=1))[0].min()
    height = (cut.y0 - 2 * C * TILE_HALF * 0.9 - top) / E
    shadow, over = cast_shadow(cut, height)
    cut.widen(over, alpha)
    body = np.asarray(cut.alpha) >= 128
    base = to_sprite(cut, rgb * pre_k, np.zeros_like(body), body, shadow)
    team = team_mask(base[..., :3], body)

    lights = lights_mask(rgb, alpha)
    glow = glow_mask(rgb, alpha, lights)
    labels, plan, glow_level = flicker_plan(lights, np.random.default_rng(SEED))
    light_small = np.asarray(cut.scale(Image.fromarray((lights * 255).astype(np.uint8)))) > 60

    # the wear of frame 0, as a factor per pixel, given to every frame (lights stay clean)
    clean0 = to_sprite(cut, rgb * pre_k, team, body, shadow)
    worn0 = np.asarray(weather(Image.fromarray(clean0), WEAR, 1).convert("RGBA")).astype(np.float32)
    factor = np.where((clean0[..., 3] == SOLID)[..., None],
                      worn0[..., :3] / np.maximum(clean0[..., :3].astype(np.float32), 1), 1.0)
    factor = np.where(light_small[..., None], 1.0, np.clip(factor, 0.3, 1.5))

    os.makedirs(WORK_ART, exist_ok=True)
    half_h = int(np.ceil(cut.y0 - 2 * C * TILE_HALF * PLINTH_FILL))
    for kind, colour in KINDS.items():
        name = f"dc_exp_{kind}"
        coloured = recolour(rgb, lights, glow, colour)
        frames = []
        for f in range(FRAMES):
            fr = light_frame(coloured, labels, plan, glow, glow_level, f)
            fr = turn_fans(fr, f * 360.0 / FRAMES)
            sp = to_sprite(cut, fr * pre_k, team, body, shadow).astype(np.float32)
            sp[..., :3] = np.clip(sp[..., :3] * factor, 0, 255)
            sp = sp.astype(np.uint8)
            img = Image.fromarray(sp, "RGBA")
            write_tga(img, os.path.join(WORK_ART, f"{name}_{f}.tga"))
            frames.append(img)
        write_tga(frames[FRAMES // 2], os.path.join(WORK_ART, f"{name}.tga"))
        # depth: a box over the tower
        verts, tris = box_mesh(TILE_HALF * 0.9, height)
        write_bh3(verts, tris, os.path.join(WORK_ART, f"{name}z.bh3"))
        open(os.path.join(WORK, f"offsets_{name}.txt"), "w").write(f"{cut.x0} {cut.y0}\n")
        strip = Image.new("RGBA", (cut.W * 4, cut.H), (90, 120, 60, 255))
        for i, f in enumerate(range(0, FRAMES, FRAMES // 4)):
            strip.alpha_composite(frames[f], (i * cut.W, 0))
        strip.resize((strip.width * 3, strip.height * 3), Image.NEAREST).save(
            os.path.join(WORK, f"preview_{name}.png"))
        print(f"{name}: {cut.W}x{cut.H}, spritexoff={cut.x0} spriteyoff={cut.y0}, {FRAMES} frames, "
              f"depth box {height:.0f} units high")
    print(f"lights: {labels.max()} parts, scale {cut.s:.4f}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else GEMINI)
