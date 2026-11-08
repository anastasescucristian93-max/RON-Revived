r"""Build the citizen-icons data files into citizen-icons\mod_override\.

    python build_data.py

The Citizen's icon of ages 6-8 (the worker with the yellow hard hat) gets one look per unit style
(UNIT_CONTINENT), cut from the Gemini collage gemini\collage_v2.png (3 x 2 panels):

    panel 1 European        DEFAULT                 TEX_UNITS_0 (7, 8)   the game's own cell, redrawn
    panel 2 African         ARAB (Bantu, Nubians)   TEX_UNITS_2 (7, 9)
    panel 3 East Asian      ASIAN + CITIZENSKOREAN  TEX_UNITS_2 (5, 11)
    panel 4 South Asian     INDIA (Indians, Persians) TEX_UNITS_2 (6, 11)
    panel 5 N. American     NA (Iroquois, Lakota)   TEX_UNITS_2 (7, 11)
    panel 6 Latin American  AMERICAN (Aztecs, Inca, Maya) TEX_UNITS_0 (3, 8)

The TEX_UNITS_2 cells are flat grey and named by no data file; TEX_UNITS_0 (3, 8) is an old biplane
picture named by no data file (the Biplane uses (2, 8)). See ENGINE_NOTES.md.

Writes:
- icons\<style>.png (61 x 39, the game's cell size) and icons\large\<style>.png (the crop itself);
- art\iface_units_01.tga + _half (TEX_UNITS_0), art\iface_units_03.tga + _half (TEX_UNITS_2), each
  on the newest version among MOD_ORDER (nuke-fx's page 03 holds the Interceptor, Heal and Entrench);
- Data\unit_graphics.xml: the newest among MOD_ORDER, with the CITIZENS*-AGE5 entries (male and
  female) pointed at their style's cell. Romania builds on this, so build it again afterwards.
"""
import os
import re

from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.dirname(HERE)
ROOT = os.path.dirname(MOD)
GAME = os.environ.get("RON_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations")
OUT = os.path.join(MOD, "mod_override")
MOD_ORDER = ["nuke-fx", "general-heal", "entrenchment", "paradrop", "data-center"]  # newest first

COLLAGE = os.path.join(MOD, "gemini", "collage_v2.png")
# panel boxes in the collage (white gaps between them)
COLS = [(0, 338), (342, 683), (686, 1024)]
ROWS = [(0, 341), (344, 687)]
# the crop inside a panel: as wide as this, from (x, y); the face fills it as in the game's icon
CROP_W, CROP_X, CROP_Y = 290, 18, 48

CELL, STEP = (61, 39), (62, 40)
HALF_CELL, HALF_STEP = (30, 19), (31, 20)
ATLAS = {"TEX_UNITS_0": "iface_units_01.tga", "TEX_UNITS_2": "iface_units_03.tga"}

# name, panel index, atlas, (col, row), unit_graphics entry prefixes
STYLES = [
    ("european", 0, "TEX_UNITS_0", (7, 8), ["CITIZENS-DEFAULT-AGE5"]),
    ("african", 1, "TEX_UNITS_2", (7, 9), ["CITIZENS-ARAB-AGE5"]),
    ("east_asian", 2, "TEX_UNITS_2", (5, 11), ["CITIZENS-ASIAN-AGE5", "CITIZENSKOREAN-DEFAULT-AGE5"]),
    ("south_asian", 3, "TEX_UNITS_2", (6, 11), ["CITIZENS-INDIA-AGE5"]),
    ("north_american", 4, "TEX_UNITS_2", (7, 11), ["CITIZENS-NA-AGE5"]),
    ("latin_american", 5, "TEX_UNITS_0", (3, 8), ["CITIZENS-AMERICAN-AGE5"]),
]


def read(path):
    with open(path, "rb") as f:
        return f.read().decode("utf-8")


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(text.encode("utf-8"))
    print(path)


def newest(sub, name):
    for mod in MOD_ORDER:
        p = os.path.join(ROOT, mod, "mod_override", sub, name)
        if os.path.exists(p):
            return p
    return os.path.join(GAME, sub, name)


def icons():
    """the six crops, at full size and at the game's cell size"""
    collage = Image.open(COLLAGE).convert("RGB")
    panels = [(x0, y0) for y0, _ in ROWS for x0, _ in COLS]
    h = round(CROP_W * CELL[1] / CELL[0])
    out = {}
    for name, panel, *_ in STYLES:
        x0, y0 = panels[panel]
        big = collage.crop((x0 + CROP_X, y0 + CROP_Y, x0 + CROP_X + CROP_W, y0 + CROP_Y + h))
        os.makedirs(os.path.join(MOD, "icons", "large"), exist_ok=True)
        big.save(os.path.join(MOD, "icons", "large", name + ".png"))
        cells = {}
        for size in (CELL, HALF_CELL):
            small = big.resize(size, Image.LANCZOS)
            cells[size] = small.filter(ImageFilter.UnsharpMask(radius=1, percent=50, threshold=2))
        cells[CELL].save(os.path.join(MOD, "icons", name + ".png"))
        out[name] = cells
    return out


def atlases(cells):
    for tex, name in ATLAS.items():
        for atlas_name, size, step in ((name, CELL, STEP),
                                       (name.replace(".tga", "_half.tga"), HALF_CELL, HALF_STEP)):
            atlas = Image.open(newest("art", atlas_name)).convert("RGBA")
            for style, _, t, (col, row), _ in STYLES:
                if t == tex:
                    atlas.paste(cells[style][size].convert("RGBA"), (col * step[0], row * step[1]))
            path = os.path.join(OUT, "art", atlas_name)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            atlas.save(path)
            print(path)


def unit_graphics():
    text = read(newest("Data", "unit_graphics.xml"))
    for _, _, tex, (col, row), prefixes in STYLES:
        for prefix in prefixes:
            pat = re.compile(r'(<UNIT name="%s(?:-FEMALE)?"[^>]*?)tex_id="[^"]*" tex_col="\d+" tex_row="\d+"'
                             % re.escape(prefix))
            text, n = pat.subn(r'\g<1>tex_id="%s" tex_col="%d" tex_row="%d"' % (tex, col, row), text)
            if n != 2:
                raise SystemExit(f"unit_graphics.xml: {prefix} (male + female) found {n} times, not 2")
    write(os.path.join(OUT, "Data", "unit_graphics.xml"), text)


def main():
    atlases(icons())
    unit_graphics()


if __name__ == "__main__":
    main()
