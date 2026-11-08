r"""Build the nuke-fx data files into nuke-fx\mod_override\.

    python build_data.py

- Data\effects_graphics.xml: a copy of the game's file with
  - NUCLEAR SMOKE (the fires left burning after a blast) emitting 3 times as long (11 s -> 33 s);
  - two underlay types appended at the end of <UNDERLAYS> (the code uses the first 20 by index,
    so appending keeps them): NukeTarget (the radiation sign over a nuke's target) and NukeHaze
    (the green haze of a radiation zone), and NukeTargetGlow (the sign's glow, in the game's
    Shockwave Blend, which adds light), all drawn by the DLL (nukefx.c), and their textures
    appended after the last <TEXTURE> of that section.
- art\nuke_target.tga (from make_target_decal.py) and art\nuke_haze.tga (made here).
- The Interceptor, trained at the Missile Silo. The game refuses extra unit types, so a
  campaign-only slot is taken over: unit 337 "SAS" (type 0x183), with the Cruise Missile's
  fields, under GRAPH SCHWARZENBERG (a campaign hero's graphics entry, help and sounds, used by
  nobody else), whose graphics entry becomes a copy of the Cruise Missile's (its model), with an
  icon of its own (2026-11-08): icons\interceptor.png (cut from source\interceptor_icon_gemini.png)
  in the free cell (4, 11) of art\iface_units_03.tga (TEX_UNITS_2, + _half), built on the General's
  Heal version of that page, which holds the Heal and Entrench icons. The
  DLL (nukefx.c) takes every finished Interceptor out of its silo at once and keeps a count per
  silo (at most 2), which it fires at enemy nukes. Data\unitrules.xml, typenames.xml, help.xml
  and unit_graphics.xml are built on the other mods' newest versions (MOD_ORDER); citizen-icons
  and then Romania build on these, so build them again afterwards and install Romania last.
- Data\soundfiles.xml: the game's, with two sounds of the Interceptor at the end (the DLL finds them
  by tag): interceptorlaunch (sounds\interceptor_launch.wav, as it leaves the silo) and
  interceptorhit (sounds\interceptor_hit.wav, the nuke shot down). make_interceptor_sounds.py
  makes the candidates and copies the chosen ones into mod_override\sounds\ (run it first).
- Data\help.xml also gets SETUPCHECK14, the tooltip of the Nukes slider in the game setup
  (camera-smooth\src\nonukes.c, drawn over check box 14).
"""
import math
import os
import random
import re
import shutil

from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.dirname(HERE)
ROOT = os.path.dirname(MOD)
GAME = os.environ.get("RON_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations")
OUT = os.path.join(MOD, "mod_override")
MOD_ORDER = ["general-heal", "entrenchment", "paradrop", "data-center"]  # newest first
SMOKE_FACTOR = 3

INTERCEPTOR_UNIT = 337
INTERCEPTOR_GRAPH = "SCHWARZENBERG"
# its icon: a cell of TEX_UNITS_2 that is flat grey in the game's page and named by no data file
ICON = os.path.join(MOD, "icons", "interceptor.png")
ICON_ATLAS, ICON_CELL = "iface_units_03.tga", (4, 11)
INTERCEPTOR_FIELDS = {
    "GRAPH": INTERCEPTOR_GRAPH,
    "OBJ_MASK": "SBX32",      # the Cruise Missile's: a missile, kept inside the silo
    "FLAGS": "j",
    "WHERE": "Missile Silo",
    "ATTACK": "0",
    "HITS": "2",
    "MOVES": "115",
    "SUPPORT": "support",
    "COST": "20k/25o",        # tens: 200 knowledge, 250 oil (a Nuclear Missile 500 / 600)
    "JOB_TIME": "1500",       # 2.5 times a Nuclear Missile's 600 (the user: 2-3 times)
    "PREQ0": "Modern Age",
    "PREQ1": "none",
    "FROM": "none",
    "JUMP": "disable",
    "GRAFT": "none",
    "RANGE": "0-0rng",
    "LOS": "0",
    "SCIENCE_LOS": "0",
    "RECHARGE": "999",
    "DOMAIN": "Air",
    "CAT": "Air",
    "SPLASH": "0",
    "PROJ_SPEED": "450",
    "CARRY_SIZE": "1",
    "POP": "0",
    "MANA": "0",
    "GRID_X": "3",            # (2, 0) is the silo's launch button
    "GRID_Y": "0",
    "UBER_SIZE": "1",
}
NAMES = {"SAS": "Interceptor"}
HELP = {
    INTERCEPTOR_GRAPH: "Shoots down enemy nuclear missiles. Each Missile Silo holds up to 2 and fires one "
                       "by itself at every enemy Nuclear Missile or ICBM that flies within reach, if it can "
                       "still catch it. A nuke shot down does no damage and does not count for Armageddon.",
}
NO_NUKES_HELP = ("{NUKES}: How many nuclear weapons may go off in this game. Drag the slider:\n"
                 "<BULLET/>Far left, None: nobody can build Nuclear Missiles, ICBMs or Interceptors; Cruise Missiles stay.\n"
                 "<BULLET/>1 to 250: the Armageddon clock starts at that number. Once that many nuclear weapons have gone off or are in the air, no more can be launched. The steps get bigger towards the right.\n"
                 "<BULLET/>Far right, Unlimited: no Armageddon clock at all.\n"
                 "Right-click it to go back to the game's rule: 4, plus 1 per nation and 2 per team. In a multiplayer game only the host can change it.")

UNDERLAYS = (
    '    <UNDERLAY name="NukeTarget" tu="0" tv="0" twidth="512" renderstate="Interface Blend" growth="1" tex0="{t}" tex1="-1" passnum="0"/>\r\n'
    '    <UNDERLAY name="NukeHaze" tu="0" tv="0" twidth="256" renderstate="Interface Blend" growth="1" tex0="{h}" tex1="-1" passnum="0"/>\r\n'
    '    <UNDERLAY name="NukeTargetGlow" tu="0" tv="0" twidth="512" renderstate="Shockwave Blend" growth="1" tex0="{g}" tex1="-1" passnum="0"/>\r\n'
    '    <UNDERLAY name="NukeCore" tu="0" tv="0" twidth="512" renderstate="Interface Blend" growth="1" tex0="{c}" tex1="-1" passnum="0"/>\r\n'
    '    <UNDERLAY name="NukeCoreGlow" tu="0" tv="0" twidth="512" renderstate="Shockwave Blend" growth="1" tex0="{cg}" tex1="-1" passnum="0"/>\r\n'
)
TEXTURES = ('    <TEXTURE file="./art/nuke_target.tga"/>\r\n    <TEXTURE file="./art/nuke_haze.tga"/>\r\n'
            '    <TEXTURE file="./art/nuke_target_glow.tga"/>\r\n    <TEXTURE file="./art/nuke_core.tga"/>\r\n'
            '    <TEXTURE file="./art/nuke_core_glow.tga"/>\r\n')


def read(path):
    with open(path, encoding="latin-1", newline="") as fh:
        return fh.read()


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="latin-1", newline="") as fh:
        fh.write(text)
    print(path)


def base(name):
    """The newest version of a data file among the mods this one builds on, else the game's."""
    for mod in MOD_ORDER:
        p = os.path.join(ROOT, mod, "mod_override", "Data", name)
        if os.path.exists(p):
            return p
    return os.path.join(GAME, "Data", name)


def set_fields(block, fields, what):
    for tag, value in fields.items():
        block, n = re.subn(rf"<{tag}>[^<]*</{tag}>|<{tag}/>", lambda m: f"<{tag}>{value}</{tag}>", block)
        if n != 1:
            raise SystemExit(f"{what}: {tag} found {n} times")
    return block


def unit_rules(text):
    spans = [m.span() for m in re.finditer(r"<UNIT>.*?</UNIT>", text, re.S)]
    if len(spans) != 364:
        raise SystemExit(f"unitrules.xml: {len(spans)} units, expected 364")
    s, e = spans[INTERCEPTOR_UNIT]
    block = text[s:e]
    if "<NAME>SAS</NAME>" not in block or "<WHERE>None</WHERE>" not in block:
        raise SystemExit(f"unitrules.xml: unit {INTERCEPTOR_UNIT} is no longer the free SAS slot (taken by another mod?)")
    mask = re.search(r"<TRIBE_MASK>(\d+)</TRIBE_MASK>", block).group(1)
    fields = dict(INTERCEPTOR_FIELDS, TRIBE_MASK="1" * len(mask))
    return text[:s] + set_fields(block, fields, f"unit {INTERCEPTOR_UNIT}") + text[e:]


def type_names(text):
    # the hash is the game's hash of the internal name, so it stays; only the shown text changes
    for old, new in NAMES.items():
        text, n = re.subn(rf'(<STRING hash="\d+" needed="1">){re.escape(old)}(</STRING>)', rf"\g<1>{new}\g<2>", text)
        if n != 1:
            raise SystemExit(f"typenames.xml: {old} found {n} times")
    return text


def help_text(text):
    for graph, desc in HELP.items():
        new = f'<ENTRY name="{graph}">\r\n\t\t\t<STRING>\r\n\t\t\t\t<BULLET/>{desc}</STRING>\r\n\t\t</ENTRY>'
        text, n = re.subn(rf'<ENTRY name="{graph}">.*?</ENTRY>', lambda m: new, text, flags=re.S)
        if n != 1:
            raise SystemExit(f"help.xml: entry {graph} found {n} times")
    return text


def setup_help(text):
    """The tooltip of the Nukes slider of the game setup (camera-smooth\\src\\nonukes.c, box
    id 14: the game looks up SETUPCHECK<id>), after the game's last one."""
    m = re.search(r'(\s*)<ENTRY name="SETUPCHECK13">(\r?\n).*?</ENTRY>', text, re.S)
    if not m or 'name="SETUPCHECK14"' in text:
        raise SystemExit("help.xml: SETUPCHECK13 not found, or SETUPCHECK14 already there")
    ind, nl = m.group(1).lstrip("\r\n"), m.group(2)
    entry = (f"{nl}{ind}<ENTRY name=\"SETUPCHECK14\">{nl}{ind}\t<STRING>{nl}{NO_NUKES_HELP}{nl}"
             f"{ind}\t</STRING>{nl}{ind}</ENTRY>")
    return text[:m.end()] + entry + text[m.end():]


SOUNDS = [("interceptorlaunch", "interceptor_launch.wav"), ("interceptorhit", "interceptor_hit.wav")]


def sound_files(text):
    """The Interceptor's two sounds after the game's last SOUNDFILE (their place does not matter:
    the DLL and the game look sounds up by tag)."""
    for tag, wav in SOUNDS:
        if not os.path.exists(os.path.join(OUT, "sounds", wav)):
            raise SystemExit(f"mod_override\\sounds\\{wav} is missing: run tools\\make_interceptor_sounds.py use L<n> I<n>")
        if f'tag="{tag}"' in text:
            raise SystemExit(f"soundfiles.xml: {tag} is already there")
    end = "\t</SOUNDFILES>"
    if text.count(end) != 1:
        raise SystemExit("soundfiles.xml: </SOUNDFILES> not found once")
    nl = "\r\n" if "\r\n" in text else "\n"
    lines = "".join(f'\t\t<SOUNDFILE priority="100" tag="{tag}" file=".\\sounds\\{wav}"/>{nl}' for tag, wav in SOUNDS)
    return text.replace(end, lines + end)


def unit_graphics(text):
    """The interceptor's graphics entry becomes a copy of the Cruise Missile's (model, icon)."""
    def block(name):
        m = re.search(r'[ \t]*<UNIT name="%s".*?</UNIT>\r?\n' % re.escape(name), text, re.S)
        if not m:
            raise SystemExit(f"unit_graphics.xml: {name} not found")
        return m
    cruise = block("CRUISEMISSILE-DEFAULT-AGE0").group(0)
    old = block(f"{INTERCEPTOR_GRAPH}-DEFAULT-AGE0")
    new = cruise.replace('name="CRUISEMISSILE-DEFAULT-AGE0"', f'name="{INTERCEPTOR_GRAPH}-DEFAULT-AGE0"')
    tex = 'tex_id="TEX_UNITS_0" tex_col="5" tex_row="10"'
    if new.count(tex) != 1:
        raise SystemExit("unit_graphics.xml: the Cruise Missile's icon cell not found once")
    new = new.replace(tex, f'tex_id="TEX_UNITS_2" tex_col="{ICON_CELL[0]}" tex_row="{ICON_CELL[1]}"')
    return text[:old.start()] + new + text[old.end():]


def icon():
    """The Interceptor's icon into its cell of the unit icon page (61 x 39 cells 62 x 40 apart;
    _half: 30 x 19, 31 x 20 apart), on the newest version of the page among MOD_ORDER."""
    for name, step, size in ((ICON_ATLAS, (62, 40), (61, 39)),
                             (ICON_ATLAS.replace(".tga", "_half.tga"), (31, 20), (30, 19))):
        src = os.path.join(GAME, "art", name)
        for mod in MOD_ORDER:
            p = os.path.join(ROOT, mod, "mod_override", "art", name)
            if os.path.exists(p):
                src = p
                break
        atlas = Image.open(src).convert("RGBA")
        cell = Image.open(ICON).convert("RGBA").resize(size, Image.LANCZOS)
        atlas.paste(cell, (ICON_CELL[0] * step[0], ICON_CELL[1] * step[1]))
        path = os.path.join(OUT, "art", name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        atlas.save(path)
        print(path)


def effects():
    src = read(os.path.join(GAME, "Data", "effects_graphics.xml"))

    def smoke(m):
        return re.sub(r'game_frame_emit_time="(\d+)"',
                      lambda t: f'game_frame_emit_time="{int(t.group(1)) * SMOKE_FACTOR}"', m.group(0), count=1)

    src, n = re.subn(r'<PARTICLESYSTEM name="NUCLEAR SMOKE">.*?</PARTICLESYSTEM>', smoke, src, flags=re.S)
    if n != 1:
        raise SystemExit("effects_graphics.xml: NUCLEAR SMOKE not found")

    start = src.index("<UNDERLAYS>")
    sec = src[start:src.index("</UNDERLAYS>")]
    textures = len(re.findall(r"<TEXTURE file=", sec))
    cut = start + sec.index("\n", sec.rindex("<UNDERLAY ")) + 1
    src = src[:cut] + UNDERLAYS.format(t=textures, h=textures + 1, g=textures + 2, c=textures + 3, cg=textures + 4) + src[cut:]
    end = src.index("</UNDERLAYS>")
    line = src.rindex("\n", 0, end) + 1
    src = src[:line] + TEXTURES + src[line:]
    write(os.path.join(OUT, "Data", "effects_graphics.xml"), src)
    print(f"  smoke x{SMOKE_FACTOR}, underlays NukeTarget/NukeHaze/NukeTargetGlow/NukeCore/NukeCoreGlow on textures {textures}..{textures + 4}")


def haze():
    """A soft green cloud: radial falloff times blurred noise, transparent at the edge."""
    S = 256
    rnd = random.Random(7)
    noise = Image.new("L", (S // 8, S // 8))
    noise.putdata([rnd.randint(90, 255) for _ in range(noise.width * noise.height)])
    noise = noise.resize((S, S), Image.BICUBIC).filter(ImageFilter.GaussianBlur(10))
    img = Image.new("RGBA", (S, S))
    px, nz = img.load(), noise.load()
    c = (S - 1) / 2
    for y in range(S):
        for x in range(S):
            d = math.hypot(x - c, y - c) / (S / 2 * 0.94)
            fall = max(0.0, 1.0 - d) ** 0.8
            px[x, y] = (90, 150 + int(70 * nz[x, y] / 255), 40, int(160 * fall * nz[x, y] / 255))
    out = os.path.join(OUT, "art", "nuke_haze.tga")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    img.save(out)
    img.save(os.path.join(MOD, "work", "nuke_haze_preview.png"))
    print(out)


def main():
    data = os.path.join(OUT, "Data")
    write(os.path.join(data, "unitrules.xml"), unit_rules(read(base("unitrules.xml"))))
    write(os.path.join(data, "typenames.xml"), type_names(read(base("typenames.xml"))))
    write(os.path.join(data, "help.xml"), setup_help(help_text(read(base("help.xml")))))
    write(os.path.join(data, "unit_graphics.xml"), unit_graphics(read(base("unit_graphics.xml"))))
    write(os.path.join(data, "soundfiles.xml"), sound_files(read(base("soundfiles.xml"))))
    effects()
    haze()
    icon()
    for name in ("nuke_target.tga", "nuke_target_glow.tga", "nuke_core.tga", "nuke_core_glow.tga"):
        shutil.copy(os.path.join(MOD, "art", name), os.path.join(OUT, "art", name))


if __name__ == "__main__":
    main()
