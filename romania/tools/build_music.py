r"""Romania's music: George Enescu's Romanian Rhapsodies among the game's own tracks.

    python build_music.py <game folder> <out mod_override folder>

The game's music list is Data\sound.xml: <TRACKS> holds a <TRIBE tribe="..."> per nation (the
game has only "default"), each with <AGE age="0..7"> lists of three moods (ECONOMIC,
BATTLE_VICTORY, BATTLE_DEFEAT). JukeBox::init_game takes the block whose tribe is the player's
nation name ("Romania"), else "default"; JukeBox::shuffle plays from the list of the current age,
or of the nearest earlier age that has one (romania\ENGINE_NOTES.md, "Music"). So Romania gets
two lists, each the game's own tracks plus Enescu:

- ages 1-3 (AGE 0): Rhapsody No. 2 (slow, lyrical) and the first half of Rhapsody No. 1;
- from the Gunpowder Age (AGE 3) on: the second half of Rhapsody No. 1, its fast dances.

Sources (music\source\, not part of the mod pack): Constantin Silvestri and the Czech Philharmonic,
Supraphon 1956, from IMSLP, marked "Public Domain - Non-PD US" (free in the EU, not in the US).
The tracks are cut, brought to the game's loudness and written as the game's tracks are: WAV,
stereo, 16 bit, 22050 Hz, in sounds\tracks\.

sound.xml is written by nation-music\tools\build_sound.py with these tracks added to Romania's
block, so it also holds every nation's own music (render nation-music first). Install this
folder after nation-music's mod_override.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MUSIC = os.path.join(HERE, "..", "music", "source")
LATE_AGE = 3  # Gunpowder: from here the fast list replaces the slow one
REPEAT = 2    # each Enescu track is listed this many times, so it comes up among the game's 20-odd

# (output name, source, start s, end s or None, fade in s, fade out s)
TRACKS = [
    ("Romania_Rhapsody2", "silvestri_rhapsody_2.mp3", 0.0, 668.0, 0.5, 4.0),
    ("Romania_Rhapsody1_Slow", "silvestri_rhapsody_1.mp3", 0.0, 365.2, 0.5, 1.5),  # a pause at 6:05
    ("Romania_Rhapsody1_Dances", "silvestri_rhapsody_1.mp3", 365.2, None, 0.2, 2.0),
]
# (track, ages, moods, times listed), the form nation-music\tools\build_sound.py takes
ENESCU = [("Romania_Rhapsody2", set(range(LATE_AGE)), ("ECONOMIC", "BATTLE_DEFEAT"), REPEAT),
          ("Romania_Rhapsody1_Slow", set(range(LATE_AGE)), ("ECONOMIC",), REPEAT),
          ("Romania_Rhapsody1_Dances", set(range(LATE_AGE, 8)), ("ECONOMIC", "BATTLE_VICTORY"), REPEAT)]
NATION_MUSIC = os.path.join(HERE, "..", "..", "nation-music", "tools")


def cut(src, out, start, end, fade_in, fade_out):
    length = end - start if end else None
    if length is None:
        probe = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of",
                                "default=nw=1:nk=1", src], capture_output=True, text=True, check=True)
        length = float(probe.stdout) - start
    filters = "afade=t=in:d=%.2f,afade=t=out:st=%.2f:d=%.2f,loudnorm=I=-17:TP=-1:LRA=16" % (
        fade_in, length - fade_out, fade_out)
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-ss", "%.2f" % start, "-t", "%.2f" % length,
                    "-i", src, "-af", filters, "-ac", "2", "-ar", "22050", "-sample_fmt", "s16",
                    "-map_metadata", "-1", "-fflags", "+bitexact", "-flags:a", "+bitexact", out], check=True)
    # only fmt and data, like the game's own tracks (ffmpeg adds a LIST chunk otherwise)


def main(game, out):
    os.makedirs(os.path.join(out, "sounds", "tracks"), exist_ok=True)
    for name, src, start, end, fi, fo in TRACKS:
        dst = os.path.join(out, "sounds", "tracks", name + ".wav")
        cut(os.path.join(MUSIC, src), dst, start, end, fi, fo)
        print("%s.wav  %.1f MB" % (name, os.path.getsize(dst) / 1e6))
    # sound.xml: every nation's own music (nation-music\) with Enescu added to Romania's, so this
    # file, installed after nation-music's, keeps both
    sys.path.insert(0, NATION_MUSIC)
    import build_sound
    tracks = build_sound.our_tracks()
    tracks.setdefault("Romania", []).extend(ENESCU)
    build_sound.write(os.path.join(out, "Data", "sound.xml"), tracks, game,
                      (build_sound.TRACKS, os.path.join(out, "sounds", "tracks")))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
