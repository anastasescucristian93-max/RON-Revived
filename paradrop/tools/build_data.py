r"""Builds the Airdrop mod's data files into mod_override\.

    python build_data.py <game folder> <out mod_override folder> [--test]

The game refuses extra unit types, so two campaign-only units are taken over (like the Data
Center's upgrades):
  - unit 330 "Guerilla Fighters" (type 0x17C) becomes the Paratroopers: elite stats, Assault
    Infantry's model, animations and sounds in a black uniform, under GRAPH FOUCHE (the campaign-only
    "Advisor Fouche"'s graphics entry, sounds and help, taken over: new graphics families cannot be
    added safely, and every family is in use);
  - unit 338 "Harrier Jet" (type 0x184) becomes the Transport Helicopter: GRAPH FIGHTERBOMBER,
    whose one graphics entry (used only by the campaign Fighter Bomber) now draws the Chinook.
Neither is trained anywhere: the mod's winmm.dll creates them. With --test both can be trained
(Paratroopers at the Barracks, the helicopter at the Airbase) to look at them in game.

Files that the Data Center mod also changes (unitrules, typenames, help) are built on top of its
versions in data-center\mod_override\ when they are there, so both mods' changes are in them.

The Airbase's button is the game's own unfinished "Paradrop" craft (type 0x284), switched on in
craftrules.xml (PREQ0 None; FROM stays None, so no unit gets it) and renamed "Airdrop"; the DLL
puts it on the Airbase and carries it out. Its icon is cell (4, 4) of art\iface_unit_orders.tga.

Writes Data\unitrules.xml, typenames.xml, help.xml, unit_graphics.xml, anim_graphics.xml,
soundtypes.xml, craftrules.xml and art\iface_unit_orders.tga.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA_CENTER = os.path.join(HERE, "..", "..", "data-center", "mod_override", "Data")

PARA_UNIT, HELI_UNIT = 330, 338
HELI_GRAPH = "FIGHTERBOMBER"
ALL_TRIBES = "1" * 24

PARA_FIELDS = {
    "GRAPH": "FOUCHE",        # Assault Infantry's model, animations and sounds, in black (para_graphics)
    "FLAGS": "lmji",          # garrisons, infantry family; the guerillas' stealth (s) dropped
    "WHERE": "None",
    "ATTACK": "30",           # Assault Infantry: 25
    "HITS": "230",            # 180
    "MOVES": "38",            # 34
    "ARMOR": "5",             # 4
    "RANGE": "0-13rng",
    "LOS": "16",
    "TO_HIT": "300",
    "SUPPORT": "support",
    "COST": "8f/7t",
    "PREQ0": "Information Age",
    "FROM": "none",
    "JUMP": "disable",
    "GRAFT": "none",
    "TRIBE_MASK": ALL_TRIBES,
    "TYPENAME": "Guerilla Fighters",  # shown name = typenames.xml text of the internal name
}

HELI_FIELDS = {
    "GRAPH": HELI_GRAPH,
    "OBJ_MASK": "3T",         # air, war machine; no weapon
    "FLAGS": "fh",            # flies like a helicopter
    "WHERE": "None",
    "ATTACK": "0",
    "HITS": "300",
    "MOVES": "75",
    "ARMOR": "3",
    "RANGE": "0-0rng",
    "LOS": "12",
    "FLY_HIGH": "10%",
    "FLY_LOW": "20%",
    "AMMO_PER_ATT": "2",      # never 0: Object::do_damage divides by it when the unit is hit (crash 2026-10-25)
    "TURN_SPEED": "65",
    "SUPPORT": "support",
    "COST": "9g/9o",
    "PREQ0": "Information Age",
    "FROM": "none",
    "JUMP": "disable",
    "GRAFT": "none",
    "MANA": "0",
    "CARRY_SIZE": "6",
    "TRIBE_MASK": ALL_TRIBES,
}

NAMES = {"Guerilla Fighters": "Paratroopers", "Harrier Jet": "Transport Helicopter", "Paradrop": "Airdrop"}

HELP = {
    HELI_GRAPH: "Flies an elite squad of Paratroopers from an Airbase to the spot you choose, lands them "
                "and flies home. Part of the price comes back when it lands at its Airbase again.",
    "FOUCHE": "Elite infantry dropped by a transport helicopter from an Airbase (Airdrop). Tougher, faster "
              "and better armored than Assault Infantry; can capture cities.",
    "PARADROP": "A transport helicopter flies 2 squads of elite Paratroopers from this Airbase to the spot you click, "
                "lands them and flies back. A third of the price comes back when the helicopter is home again. "
                "Each Airbase can send one every 45 seconds. From the Information Age.",
}

# FROM stays None: no unit casts it, the DLL puts it on the Airbase. COST2 (the price to cast,
# in tens) is only for the tooltip's resource icons: the DLL charges it, not the game, whose own
# casting code is never reached. Keep it equal to airdrop.ini's cost_food / cost_oil.
CRAFT_FIELDS = {"PREQ0": "None", "COST2": "40f/35o"}
ICON = os.path.join(HERE, "..", "icons", "airdrop.png")
ICON_CELL = (4, 4)  # the craft's TEX_COL / TEX_ROW in TEX_ORDERS_0; no other icon uses it
CELL_STEP, CELL_SIZE = (62, 40), (61, 39)

# The game may not take more animations than it was built with (it refuses extra buildings), so
# the rotor spin takes over an animation nothing uses: "LightBomber Default" is named nowhere
# else, in no data file, scenario or the exe (names compare without case).
ANIM_SLOT = "LightBomber Default"


def read(path):
    with open(path, encoding="latin-1", newline="") as fh:
        return fh.read()


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="latin-1", newline="") as fh:
        fh.write(text)
    print(path)


def base(game, name):
    """The Data Center's version of a file when it has one, else the game's."""
    dc = os.path.join(DATA_CENTER, name)
    return dc if os.path.exists(dc) else os.path.join(game, "Data", name)


def set_fields(block, fields, what):
    for tag, value in fields.items():
        block, n = re.subn(rf"<{tag}>[^<]*</{tag}>|<{tag}/>", f"<{tag}>{value}</{tag}>" if value else f"<{tag}/>",
                           block)
        if n != 1:
            sys.exit(f"{what}: {tag} found {n} times")
    return block


def unit_rules(text, test):
    spans = [m.span() for m in re.finditer(r"<UNIT>.*?</UNIT>", text, re.S)]
    if len(spans) != 364:
        sys.exit(f"unitrules.xml: {len(spans)} units, expected 364")
    edits = {PARA_UNIT: dict(PARA_FIELDS), HELI_UNIT: dict(HELI_FIELDS)}
    if test:
        edits[PARA_UNIT]["WHERE"] = "Barracks"
        edits[HELI_UNIT]["WHERE"] = "Airbase"
    for unit in sorted(edits, reverse=True):
        s, e = spans[unit]
        block = text[s:e]
        if "<WHERE>None</WHERE>" not in block:
            sys.exit(f"unitrules.xml: unit {unit} is no longer a free campaign slot (taken by another mod?)")
        text = text[:s] + set_fields(block, edits[unit], f"unit {unit}") + text[e:]
    return text


def type_names(text):
    # the hash is the game's hash of the internal name, so it stays; only the shown text changes
    for old, new in NAMES.items():
        text, n = re.subn(rf'(<STRING hash="\d+" needed="1">){re.escape(old)}(</STRING>)', rf"\g<1>{new}\g<2>", text)
        if n != 1:
            sys.exit(f"typenames.xml: {old} found {n} times")
    return text


def help_text(text):
    for graph, desc in HELP.items():
        new = f'<ENTRY name="{graph}">\r\n\t\t\t<STRING>\r\n\t\t\t\t<BULLET/>{desc}</STRING>\r\n\t\t</ENTRY>'
        text, n = re.subn(rf'<ENTRY name="{graph}">.*?</ENTRY>', lambda m: new, text, flags=re.S)
        if n != 1:
            sys.exit(f"help.xml: entry {graph} found {n} times")
    return text


def unit_block(text, name):
    m = re.search(r'    <UNIT name="%s".*?</UNIT>\r?\n' % re.escape(name), text, re.S)
    if not m:
        sys.exit(f"unit_graphics.xml: {name} not found")
    return m


def unit_graphics(text):
    """FIGHTERBOMBER-DEFAULT-AGE0 becomes the gunship's entry (icon, shadow, events) drawn as the
    Chinook, with the main rotor's disc on both hubs (nodes 2 and 3) and the reused spin."""
    block = unit_block(text, "HELICOPTER-DEFAULT-AGE0").group(0)
    block = block.replace('name="HELICOPTER-DEFAULT-AGE0"', f'name="{HELI_GRAPH}-DEFAULT-AGE0"')
    block = re.sub(r'model="[^"]*"', r'model=".\\art\\chinook.bh3"', block, count=1)
    block = re.sub(r'texture="[^"]*"', r'texture=".\\art\\chinook.tga"', block, count=1)
    block = re.sub(r'scale="[^"]*"', 'scale="1"', block, count=1)
    block = re.sub(r'shadowwidth="[^"]*" shadowheight="[^"]*"', 'shadowwidth="90" shadowheight="160"', block)
    block = block.replace('file="Helicopter Propspin"', f'file="{ANIM_SLOT}"')
    block = block.replace('file="Helicopter Attack"', f'file="{ANIM_SLOT}"')  # the same .bha in the game
    block = block.replace('type="B-25Prop0" node="3"', 'type="CopterBlade" node="3"')
    # no weapon: no rockets, no muzzle smoke
    block = re.sub(r'\s*<(RELEASEEVENT|PARTICLEEVENT)[^>]*anim="CHAR_ATTACK2"[^>]*/>', "", block)
    m = unit_block(text, f"{HELI_GRAPH}-DEFAULT-AGE0")
    return text[:m.start()] + block + text[m.end():]


def anim_graphics(text):
    old = f'<ANIM name="{ANIM_SLOT}" file=".\\art\\lightbomber.BHa"/>'
    if old not in text:
        sys.exit("anim_graphics.xml: anim slot not found")
    return text.replace(old, f'<ANIM name="{ANIM_SLOT}" file=".\\art\\chinook_propspin.bha"/>')


def sound_types(text):
    heli = re.search(r"  <HELICOPTER>\r?\n(.*?)  </HELICOPTER>", text, re.S).group(1)
    new, n = re.subn(rf"(  <{HELI_GRAPH}>\r?\n).*?(  </{HELI_GRAPH}>)", lambda m: m.group(1) + heli + m.group(2),
                     text, flags=re.S)
    if n != 1:
        sys.exit(f"soundtypes.xml: {HELI_GRAPH} found {n} times")
    return new


PARA_GRAPH = "FOUCHE"  # campaign-only "Advisor Fouche" (unit 321): one graphics entry, on foot
PARA_TEX = "paratrooper.tga"


def dark_uniform(game, out):
    """assault_infantry.tga with the uniform, gear and gun turned charcoal black. Skin (faces,
    arms) and the player-colour pixels (alpha under 255) stay as they are."""
    import numpy as np
    from PIL import Image
    a = np.asarray(Image.open(os.path.join(game, "art", "assault_infantry.tga")).convert("RGBA")).astype(np.float64)
    r, g, b, al = a[..., 0], a[..., 1], a[..., 2], a[..., 3]
    # skin colours only where the texture has skin (the tan camouflage is the same colour):
    # the two faces at the top left and the arms at the bottom left (128 x 128 texture)
    where = np.zeros(r.shape, bool)
    s = r.shape[0] / 128.0
    for x0, y0, x1, y1 in ((0, 0, 17, 15), (32, 0, 49, 15), (0, 64, 49, 99)):
        where[int(y0 * s):int(y1 * s), int(x0 * s):int(x1 * s)] = True
    skin = where & (r > 110) & (r > g + 12) & (g > b + 4) & (r - b > 35)
    keep = skin | (al < 255)
    lum = 0.30 * r + 0.59 * g + 0.11 * b
    dark = 14 + lum * 0.30  # 14 .. ~90: black with the camouflage still visible
    out_rgb = np.stack([dark * 0.95, dark * 0.97, dark * 1.05], axis=-1)  # a hint of blue-grey
    res = a.copy()
    res[..., :3] = np.where(keep[..., None], a[..., :3], out_rgb)
    path = os.path.join(out, "art", PARA_TEX)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    Image.fromarray(np.clip(res, 0, 255).astype(np.uint8), "RGBA").save(path)
    print(path, "(%d skin pixels kept)" % int(skin.sum()))


def para_graphics(text):
    """The FOUCHE entry becomes Assault Infantry's (model, animations, events) in black."""
    block = unit_block(text, "ASSAULTINFANTRY-DEFAULT-AGE0").group(0)
    block = block.replace('name="ASSAULTINFANTRY-DEFAULT-AGE0"', f'name="{PARA_GRAPH}-DEFAULT-AGE0"')
    block = re.sub(r'texture="[^"]*"', rf'texture=".\\art\\{PARA_TEX}"', block, count=1)
    m = unit_block(text, f"{PARA_GRAPH}-DEFAULT-AGE0")
    return text[:m.start()] + block + text[m.end():]


def para_sounds(text):
    inf = re.search(r"  <ASSAULTINFANTRY>\r?\n(.*?)  </ASSAULTINFANTRY>", text, re.S).group(1)
    new, n = re.subn(rf"(  <{PARA_GRAPH}>\r?\n).*?(  </{PARA_GRAPH}>)", lambda m: m.group(1) + inf + m.group(2),
                     text, flags=re.S)
    if n != 1:
        sys.exit(f"soundtypes.xml: {PARA_GRAPH} found {n} times")
    return new


def craft_rules(text):
    m = re.search(r"<CRAFT>\s*<NAME>Paradrop</NAME>.*?</CRAFT>", text, re.S)
    if not m:
        sys.exit("craftrules.xml: Paradrop not found")
    return text[:m.start()] + set_fields(m.group(0), CRAFT_FIELDS, "Paradrop") + text[m.end():]


def order_icons(game, out):
    from PIL import Image
    atlas = Image.open(os.path.join(game, "art", "iface_unit_orders.tga"))
    icon = Image.open(ICON).convert("RGBA").resize(CELL_SIZE, Image.LANCZOS)
    if atlas.mode == "RGBA":
        icon.putalpha(255)
    else:
        icon = icon.convert(atlas.mode)
    atlas.paste(icon, (ICON_CELL[0] * CELL_STEP[0], ICON_CELL[1] * CELL_STEP[1]))
    path = os.path.join(out, "art", "iface_unit_orders.tga")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    atlas.save(path)
    print(path)


def main():
    args = [a for a in sys.argv[1:] if a != "--test"]
    if len(args) != 2:
        sys.exit(__doc__)
    game, out = args
    test = "--test" in sys.argv
    data, out_data = os.path.join(game, "Data"), os.path.join(out, "Data")
    write(os.path.join(out_data, "unitrules.xml"), unit_rules(read(base(game, "unitrules.xml")), test))
    write(os.path.join(out_data, "typenames.xml"), type_names(read(base(game, "typenames.xml"))))
    write(os.path.join(out_data, "help.xml"), help_text(read(base(game, "help.xml"))))
    write(os.path.join(out_data, "unit_graphics.xml"),
          para_graphics(unit_graphics(read(os.path.join(data, "unit_graphics.xml")))))
    write(os.path.join(out_data, "anim_graphics.xml"), anim_graphics(read(os.path.join(data, "anim_graphics.xml"))))
    write(os.path.join(out_data, "soundtypes.xml"), para_sounds(sound_types(read(os.path.join(data, "soundtypes.xml")))))
    dark_uniform(game, out)
    write(os.path.join(out_data, "craftrules.xml"), craft_rules(read(os.path.join(data, "craftrules.xml"))))
    order_icons(game, out)
    if test:
        print("TEST: Paratroopers trainable at the Barracks, Transport Helicopter at the Airbase")


if __name__ == "__main__":
    main()
