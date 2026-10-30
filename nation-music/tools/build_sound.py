r"""Writes ..\mod_override\Data\sound.xml: the game's music list with a <TRIBE> block for every
nation that has pieces here, so each nation hears its own tracks among the game's.

    python build_sound.py [game folder]     (default: RON_DIR or the Steam folder)

How the game reads it (romania\ENGINE_NOTES.md, "Music"): JukeBox::init_game takes the <TRIBE>
whose name is the player's nation, else "default"; JukeBox::shuffle plays from the current age's
<AGE> list, or from the nearest earlier age that has one. So a block gets an <AGE> list at age 0
and wherever the set of its tracks changes; each list is the game's own tracks of that mood plus
ours, ours written REPEAT times so they come up often among the game's (20 ECONOMIC, 6
BATTLE_VICTORY, 7 BATTLE_DEFEAT).

The WAVs must have been rendered first (render_all.py); a track the list names but that is
missing stops the build.

Romania's Enescu recordings (romania\music\, not in the mod pack) are merged in when they are
there: then romania\music\mod_override\Data\sound.xml is written too, the same list plus Enescu,
to install after this mod (romania\tools\build_music.py writes that file the same way).
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
REPO = os.path.dirname(ROOT)
sys.path.insert(0, HERE)
GAME = os.environ.get("RON_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations")
TRACKS = os.path.join(ROOT, "mod_override", "sounds", "tracks")
OUT = os.path.join(ROOT, "mod_override", "Data", "sound.xml")
ENESCU_DIR = os.path.join(REPO, "romania", "music", "mod_override")
REPEAT = 3
AGES = range(8)


def our_tracks():
    """nation -> [(track name, ages, moods, times listed)] for every piece"""
    from render_all import pieces, track_name
    out = {}
    for _, piece in pieces():
        moods = piece.MOOD if isinstance(piece.MOOD, tuple) else (piece.MOOD,)
        out.setdefault(piece.NATION, []).append((track_name(piece), set(piece.AGES), moods, REPEAT))
    return out


def tribe_block(nation, entries, moods, indent):
    lines = [f'{indent}<TRIBE tribe="{nation}">']
    last = None
    for age in AGES:
        active = sorted((m, name, rep) for name, ages, ms, rep in entries if age in ages for m in ms)
        if age and active == last:
            continue
        last = active
        lines.append(f'{indent}  <AGE age="{age}">')
        for mood, sounds in moods:
            lines.append(f'{indent}    <MOOD mood="{mood}">')
            lines += [f"{indent}      {s}" for s in sounds]
            for m, name, rep in active:
                if m == mood:
                    lines += [f'{indent}      <SOUND file=".\\sounds\\tracks\\{name}.wav"/>'] * rep
            lines.append(f"{indent}    </MOOD>")
        lines.append(f"{indent}  </AGE>")
    lines.append(f"{indent}</TRIBE>")
    return lines


def write(path, tracks, game=GAME, track_dirs=(TRACKS,)):
    """sound.xml with a block per nation in 'tracks' (nation -> entries as our_tracks gives)"""
    for nation, entries in tracks.items():
        for name, *_ in entries:
            if not any(os.path.isfile(os.path.join(d, name + ".wav")) for d in track_dirs):
                sys.exit(f"build_sound: {name}.wav is missing (run render_all.py)")
    with open(os.path.join(game, "Data", "sound.xml"), encoding="utf-8", newline="") as fh:
        text = fh.read()
    nl = "\r\n" if "\r\n" in text else "\n"
    # the game's default block (the first <TRACKS>; a second one is commented out)
    tracks_end = text.index("</TRACKS>")
    block = text[text.index('<TRIBE tribe="default">'):tracks_end]
    moods = [(m.group(1), re.findall(r"<SOUND [^>]*/>", m.group(2)))
             for m in re.finditer(r'<MOOD mood="(\w+)">(.*?)</MOOD>', block, re.S)]
    lines = []
    for nation in sorted(tracks):
        lines += tribe_block(nation, tracks[nation], moods, "    ")
    start = text.rindex("\n", 0, tracks_end) + 1
    text = text[:start] + nl.join(lines) + nl + text[start:]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as fh:
        fh.write(text)
    count = sum(len(e) for e in tracks.values())
    print(f"{path}: {len(tracks)} nations, {count} tracks")


def enescu():
    """Romania's Enescu entries, if its recordings were built on this PC"""
    sys.path.insert(0, os.path.join(REPO, "romania", "tools"))
    from build_music import ENESCU
    have = all(os.path.isfile(os.path.join(ENESCU_DIR, "sounds", "tracks", e[0] + ".wav")) for e in ENESCU)
    return ENESCU if have else None


def main(game):
    tracks = our_tracks()
    write(OUT, tracks, game)
    extra = enescu()
    if extra:
        merged = {n: list(e) for n, e in tracks.items()}
        merged.setdefault("Romania", []).extend(extra)
        write(os.path.join(ENESCU_DIR, "Data", "sound.xml"), merged, game,
              (TRACKS, os.path.join(ENESCU_DIR, "sounds", "tracks")))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else GAME)
