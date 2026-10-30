r"""Puts a built pack on GitHub as the latest release, so players get it from the launcher.

    python package\publish_release.py <version>

Run build_package.py <version> first. The release (tag v<version>) gets the whole zip, for a
fresh install, and the three part zips and RON_Revived_update.txt from
dist\RON_Revived-<version>-update\, for the launcher's updater. The text on the release page is
release_notes.md with {version} filled in.

The release is made as a draft with every file on it and only then published: the launcher
reads releases/latest/download/RON_Revived_update.txt, and a release that is already "latest"
while its zips are still uploading would send players to files that are not there yet.
Needs the GitHub CLI (gh), logged in with write access to the repository.
"""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HERE = os.path.join(ROOT, "package")
DIST = os.path.join(ROOT, "dist")
REPO = "anastasescucristian93-max/RON-Revived"


def gh(*args):
    print("gh " + " ".join(args))
    subprocess.run(["gh", *args, "-R", REPO], check=True)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    version = sys.argv[1]
    tag = "v" + version
    full = os.path.join(DIST, f"RON_Revived-{version}.zip")
    upd = os.path.join(DIST, f"RON_Revived-{version}-update")
    index = os.path.join(upd, "RON_Revived_update.txt")
    for p in (full, index):
        if not os.path.isfile(p):
            sys.exit(f"publish: {p} is missing: run python package\\build_package.py {version} first")
    with open(index, encoding="ascii") as f:
        if f"version={version}\n" not in f.read():
            sys.exit(f"publish: {index} is not for {version}")
    parts = sorted(os.path.join(upd, f) for f in os.listdir(upd) if f.endswith(".zip"))

    with open(os.path.join(HERE, "release_notes.md"), encoding="utf-8") as f:
        notes = f.read().replace("{version}", version)
    with tempfile.NamedTemporaryFile("w", suffix=".md", delete=False, encoding="utf-8") as f:
        f.write(notes)
    try:
        gh("release", "create", tag, "--draft", "--title", f"RON Revived {version}", "--notes-file", f.name, full,
           *parts, index)
        gh("release", "edit", tag, "--draft=false", "--latest")
    finally:
        os.remove(f.name)
    print(f"OK: https://github.com/{REPO}/releases/tag/{tag}")


if __name__ == "__main__":
    main()
