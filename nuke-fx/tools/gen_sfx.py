r"""Generate nuke sound effects with ElevenLabs and convert them to the game's wav format.

    python gen_sfx.py [boom|alarm ...] [--variants N] [--only K]

Reads the prompts from prompts.json next to this script. The API key comes from the
ELEVENLABS_API_KEY environment variable (user level is enough: setx ELEVENLABS_API_KEY ...).
Every generation costs ElevenLabs credits.

Output, per group and prompt k, variant v:
    nuke-fx/source/elevenlabs/<group>_<k><v>.mp3   what ElevenLabs returned (kept)
    nuke-fx/work/<group>_<k><v>.wav                22050 Hz, 16-bit, stereo, like the game's own sounds
Existing files are skipped, so a rerun only fills in what is missing.
"""
import json
import os
import subprocess
import sys
import urllib.error
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.dirname(HERE)
SRC = os.path.join(MOD, "source", "elevenlabs")
WORK = os.path.join(MOD, "work")
URL = "https://api.elevenlabs.io/v1/sound-generation"


def api_key():
    key = os.environ.get("ELEVENLABS_API_KEY")
    if not key and os.name == "nt":
        import winreg
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, "Environment") as k:
                key = winreg.QueryValueEx(k, "ELEVENLABS_API_KEY")[0]
        except OSError:
            pass
    if not key:
        sys.exit("ELEVENLABS_API_KEY is not set")
    return key


def generate(key, text, seconds, path):
    body = json.dumps({"text": text, "duration_seconds": seconds, "prompt_influence": 0.5}).encode()
    req = urllib.request.Request(URL, body, {"xi-api-key": key, "Content-Type": "application/json",
                                            "Accept": "audio/mpeg"})
    try:
        with urllib.request.urlopen(req, timeout=180) as r:
            data = r.read()
    except urllib.error.HTTPError as e:
        sys.exit(f"ElevenLabs: HTTP {e.code} {e.read().decode(errors='replace')[:400]}")
    with open(path, "wb") as f:
        f.write(data)


def to_wav(mp3, wav):
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", mp3, "-ar", "22050", "-ac", "2",
                    "-sample_fmt", "s16", wav], check=True)


def main():
    args = sys.argv[1:]
    variants = only = None
    if "--variants" in args:
        i = args.index("--variants"); variants = int(args[i + 1]); del args[i:i + 2]
    if "--only" in args:
        i = args.index("--only"); only = int(args[i + 1]); del args[i:i + 2]
    cfg = json.load(open(os.path.join(HERE, "prompts.json"), encoding="utf-8"))
    groups = args or list(cfg)
    key = api_key()
    os.makedirs(SRC, exist_ok=True)
    os.makedirs(WORK, exist_ok=True)
    for g in groups:
        c = cfg[g]
        for k, text in enumerate(c["prompts"], 1):
            if only and k != only:
                continue
            for v in "abcdefgh"[:variants or c["variants"]]:
                name = f"{g}_{k}{v}"
                mp3 = os.path.join(SRC, name + ".mp3")
                wav = os.path.join(WORK, name + ".wav")
                if not os.path.exists(mp3):
                    print("generating", name)
                    generate(key, text, c["duration"], mp3)
                if not os.path.exists(wav):
                    to_wav(mp3, wav)
                print("ok", wav)


if __name__ == "__main__":
    main()
