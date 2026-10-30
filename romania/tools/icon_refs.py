r"""A reference sheet for Gemini: the game's own icons of some units, enlarged, in a grid on magenta.

    python icon_refs.py <out.png> <columns> <GRAPH> [GRAPH ...]

GRAPH is the unit's graphics family (unitrules.xml <GRAPH>, for example SLINGERS); the icon is the
one unit_graphics.xml gives "<GRAPH>-DEFAULT-AGE0". The order is the order of the sheet Gemini is
asked to paint, so each new icon has the game's icon of the same unit as its model.
"""
import os
import re
import sys

from PIL import Image

GAME = os.environ.get("RON_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations")
ATLAS = {"TEX_UNITS_0": "iface_units_01.tga", "TEX_UNITS_1": "iface_units_02.tga", "TEX_UNITS_2": "iface_units_03.tga"}
SCALE = 5
GAP = 20
MAGENTA = (255, 0, 255, 255)


def main(out, cols, graphs):
    text = open(os.path.join(GAME, "Data", "unit_graphics.xml"), encoding="utf-8", errors="replace").read()
    cells = []
    for g in graphs:
        m = re.search(r'<UNIT name="%s-DEFAULT-AGE0"[^>]*tex_id="(\w+)" tex_col="(\d+)" tex_row="(\d+)"' % re.escape(g), text)
        if not m:
            raise SystemExit("no icon for " + g)
        atlas = Image.open(os.path.join(GAME, "art", ATLAS[m.group(1)])).convert("RGBA")
        x, y = int(m.group(2)) * 62, int(m.group(3)) * 40
        cells.append(atlas.crop((x, y, x + 61, y + 39)).resize((61 * SCALE, 39 * SCALE), Image.LANCZOS))
    rows = (len(cells) + cols - 1) // cols
    w, h = 61 * SCALE, 39 * SCALE
    sheet = Image.new("RGBA", (GAP + cols * (w + GAP), GAP + rows * (h + GAP)), MAGENTA)
    for i, c in enumerate(cells):
        sheet.paste(c, (GAP + (i % cols) * (w + GAP), GAP + (i // cols) * (h + GAP)))
    sheet.convert("RGB").save(out)
    print(out, sheet.size)


if __name__ == "__main__":
    main(sys.argv[1], int(sys.argv[2]), sys.argv[3:])
