"""MP3 copies of the rendered tracks in ../share/, for sending (WhatsApp and the like take MP3,
not the game's WAV). Needs ffmpeg on the PATH.

python share_mp3.py [word ...]   only the tracks whose name contains a given word
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TRACKS = os.path.join(ROOT, "mod_override", "sounds", "tracks")
SHARE = os.path.join(ROOT, "share")


def main(words):
    os.makedirs(SHARE, exist_ok=True)
    for f in sorted(os.listdir(TRACKS)):
        if not f.endswith(".wav") or (words and not any(w in f for w in words)):
            continue
        out = os.path.join(SHARE, f[:-4] + ".mp3")
        subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", os.path.join(TRACKS, f),
                        "-codec:a", "libmp3lame", "-b:a", "160k", out], check=True)
        print(out)


if __name__ == "__main__":
    main(sys.argv[1:])
