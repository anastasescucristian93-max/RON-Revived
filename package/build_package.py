r"""Builds the player package: dist\RON_Revived-<version>.zip, one folder that installs the whole
mod pack with a double-click.

    python package\build_package.py [version]        (default version: today's date, 2026.10.28)

Run camera-smooth\build.ps1 and every data mod's tools\build_data.py first (in the order below);
this script only gathers what they made, it builds nothing.

The zip holds install.bat / uninstall.bat (and their .ps1), README.txt, CHANGES.md and game\,
the files exactly as they go into the game folder:
  - winmm.dll and the settings files next to riseofnations.exe;
  - mod_override\: the mods' mod_override folders laid over each other in their build order, so
    for a file several mods write (help.xml, unitrules.xml, ...) the last one wins, and that is
    the one built on all the others;
  - nation_art\: every <mod>\nation_art\ folder (the buildings per nation).
Romania's Enescu recordings (romania\music\) stay out: they are not free in the US.

For the launcher's updater it also makes dist\RON_Revived-<version>-update\: the pack in three
parts, each a zip of its own (core; art: nation_art\ and mod_override\art\; music:
mod_override\sounds\tracks\), and RON_Revived_update.txt, the list the launcher reads: the
version, where the zips are, and per part a hash of its contents, the zip's SHA-256 and size.
A part whose files did not change keeps its hash, so players do not download it again.
publish_release.py puts it all on GitHub.
"""
import datetime
import hashlib
import os
import re
import shutil
import sys
import zipfile

REPO = "anastasescucristian93-max/RON-Revived"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HERE = os.path.join(ROOT, "package")
DIST = os.path.join(ROOT, "dist")

# the data mods' build order; a later mod's copy of a file replaces an earlier one's
MOD_ORDER = ["data-center", "paradrop", "entrenchment", "general-heal", "nuke-fx", "ui-theme", "romania",
             "translations", "nation-music"]

# next to riseofnations.exe: game file name -> repo file
ROOT_FILES = {
    "winmm.dll": r"camera-smooth\build\winmm.dll",
    "ron_camera.ini": r"camera-smooth\ron_camera.ini",
    "nation_art.ini": r"roman-buildings\nation_art.ini",
    "airdrop.ini": r"paradrop\airdrop.ini",
    "data_center.ini": r"data-center\data_center.ini",
    "entrench.ini": r"entrenchment\entrench.ini",
    "romania.ini": r"romania\romania.ini",
    "romania_units.ini": r"romania\romania_units.ini",
    # the launcher's updater (camera-smooth\src\launcher_update.c starts it)
    "RON_Revived_update.ps1": r"package\update.ps1",
    "RON_Revived_install.ps1": r"package\install.ps1",
}

# the parts the updater downloads one by one; everything else is core (install.ps1 has the same
# rule). The version and parts files are written by the updater itself.
PARTS = ["core", "art", "music"]
META = {"ron_revived_version.txt", "ron_revived_parts.txt"}


def part_of(rel):
    r = rel.lower()
    if r in META:
        return "meta"
    if r.startswith("mod_override\\sounds\\tracks\\"):
        return "music"
    if r.startswith("nation_art\\") or r.startswith("mod_override\\art\\"):
        return "art"
    return "core"

# game scripts (the Script choice in the game setup): repo folder -> game folder
SCRIPT_TREES = {r"survival\scenario": "scenario"}

PLAYER_FILES = ["install.bat", "install.ps1", "uninstall.bat", "uninstall.ps1", "README.txt"]


def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def md5(p):
    with open(p, "rb") as f:
        return hashlib.md5(f.read()).hexdigest()


def walk(base):
    """relative path -> full path of every file under base"""
    out = {}
    for d, _, files in os.walk(base):
        for f in files:
            p = os.path.join(d, f)
            out[os.path.relpath(p, base)] = p
    return out


def fail(msg):
    sys.exit("package: " + msg)


def gather():
    """game-relative path -> repo file"""
    files = {}
    for name, src in ROOT_FILES.items():
        p = os.path.join(ROOT, src)
        if not os.path.isfile(p):
            fail(f"{src} is missing")
        files[name] = p

    seen = {}  # lower-case path -> spelling, so two spellings of one file do not both go in
    for mod in MOD_ORDER:
        base = os.path.join(ROOT, mod, "mod_override")
        if not os.path.isdir(base):
            fail(f"{mod}\\mod_override is missing")
        for rel, p in walk(base).items():
            key = os.path.join("mod_override", rel)
            files.pop(seen.get(key.lower(), key), None)
            files[key] = p
            seen[key.lower()] = key

    for src, dst in SCRIPT_TREES.items():
        base = os.path.join(ROOT, src)
        if not os.path.isdir(base):
            fail(f"{src} is missing")
        for rel, p in walk(base).items():
            files[os.path.join(dst, rel)] = p

    owner = {}
    for mod in sorted(os.listdir(ROOT)):
        base = os.path.join(ROOT, mod, "nation_art")
        if not os.path.isdir(base):
            continue
        for rel, p in walk(base).items():
            key = os.path.join("nation_art", rel)
            if key.lower() in owner and md5(files[owner[key.lower()][1]]) != md5(p):
                fail(f"{key} comes from both {owner[key.lower()][0]} and {mod}, with different pictures")
            owner[key.lower()] = (mod, key)
            files[key] = p
    return files


def check(files):
    # the DLL is older than its sources: it was not rebuilt after the last change
    dll = files["winmm.dll"]
    src = os.path.join(ROOT, "camera-smooth", "src")
    newer = [f for f in os.listdir(src) if os.path.getmtime(os.path.join(src, f)) > os.path.getmtime(dll)]
    if newer:
        fail(f"winmm.dll is older than {', '.join(newer[:3])}: run camera-smooth\\build.ps1 first")

    # every picture nation_art.ini names is in the package
    have = {k.lower() for k in files}
    missing = []
    with open(files["nation_art.ini"], encoding="ascii") as f:
        for line in f:
            line = line.split(";", 1)[0].strip()
            m = re.match(r"([^=\[]+?)\s*=\s*(\S+)$", line)
            if not m:
                continue
            target = m.group(2).lower()
            frames = re.search(r"~(\d+):", m.group(1))
            names = [target % i for i in range(int(frames.group(1)))] if frames and "%d" in target else [target]
            # art\... is found through the game's mod_override\ (the Data Center, the Nest)
            missing += [n for n in names if n not in have and "mod_override\\" + n not in have]
    if missing:
        fail(f"nation_art.ini names {len(missing)} missing pictures, e.g. {missing[:3]}")

    # every track the music list names is in the package (the nation music WAVs are not in git:
    # nation-music\tools\render_all.py makes them)
    xml = files.get(os.path.join("mod_override", "Data", "sound.xml"))
    if xml:
        with open(xml, encoding="utf-8") as f:
            named = set(re.findall(r'file="\.\\(sounds\\tracks\\[^"]+)"', f.read()))
        game_tracks = os.path.join(os.environ.get("RON_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations"))
        lost = [n for n in named if "mod_override\\" + n.lower() not in have
                and not os.path.isfile(os.path.join(game_tracks, n))]
        if lost:
            fail(f"sound.xml names {len(lost)} tracks that are missing, e.g. {sorted(lost)[:3]}: run nation-music\\tools\\render_all.py")

    # the settings files are read with GetPrivateProfile*: plain ASCII
    for name in ROOT_FILES:
        if name.endswith(".ini"):
            with open(files[name], "rb") as f:
                if any(b > 127 for b in f.read()):
                    fail(f"{name} has non-ASCII bytes")


def build_parts(version, files):
    """dist\\RON_Revived-<version>-update\\: a zip per part and RON_Revived_update.txt. Returns each
    part's content hash, made from its files' paths and SHA-256, so it does not change when only
    their dates do."""
    out = os.path.join(DIST, f"RON_Revived-{version}-update")
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    lines = [f"version={version}", f"base=https://github.com/{REPO}/releases/download/v{version}/"]
    content = {}
    for part in PARTS:
        rels = sorted((r for r in files if part_of(r) == part), key=str.lower)
        h = hashlib.sha256()
        for r in rels:
            h.update(f"{r.lower()}\0{sha256(files[r])}\n".encode("utf-8"))
        content[part] = h.hexdigest()[:32]
        name = f"RON_Revived-{version}-{part}.zip"
        zpath = os.path.join(out, name)
        with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for r in rels:
                z.write(files[r], os.path.join("game", r))
        lines.append(f"{part}={content[part]} {sha256(zpath)} {os.path.getsize(zpath)} {name}")
        print(f"    {part}: {len(rels)} files, {os.path.getsize(zpath) / 2**20:.1f} MB zipped")
    with open(os.path.join(out, "RON_Revived_update.txt"), "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"OK: {out}")
    return content


def main():
    version = sys.argv[1] if len(sys.argv) > 1 else datetime.date.today().strftime("%Y.%m.%d")
    files = gather()
    check(files)

    name = f"RON_Revived-{version}"
    stage = os.path.join(DIST, name)
    shutil.rmtree(stage, ignore_errors=True)
    for rel, src in files.items():
        dst = os.path.join(stage, "game", rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
    for f in PLAYER_FILES:
        shutil.copy2(os.path.join(HERE, f), os.path.join(stage, f))
    shutil.copy2(os.path.join(ROOT, "CHANGES.md"), os.path.join(stage, "CHANGES.md"))
    with open(os.path.join(stage, "game", "RON_Revived_version.txt"), "w", encoding="ascii") as f:
        f.write(version + "\n")
    content = build_parts(version, files)
    with open(os.path.join(stage, "game", "RON_Revived_parts.txt"), "w", encoding="ascii") as f:
        f.write("[parts]\n" + "".join(f"{p}={content[p]}\n" for p in PARTS))

    zpath = os.path.join(DIST, name + ".zip")
    if os.path.exists(zpath):
        os.remove(zpath)
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for rel, p in sorted(walk(stage).items()):
            z.write(p, os.path.join(name, rel))
    size = sum(os.path.getsize(p) for p in walk(stage).values())
    print(f"OK: {zpath}")
    print(f"    {len(files)} game files, {size / 2**20:.0f} MB unpacked, {os.path.getsize(zpath) / 2**20:.0f} MB zipped")


if __name__ == "__main__":
    main()
