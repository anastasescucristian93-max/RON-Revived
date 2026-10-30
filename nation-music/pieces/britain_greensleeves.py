"""Britain, Gunpowder age, economy: Greensleeves.

The Tudor ballad (traditional, 16th century, public domain) over the
passamezzo antico bass it was sung on, played by a consort of the time: the
lute alone first, then a recorder, then a viol with the recorder's descant,
then all of it with the harpsichord. It ends on the bright A major chord
(the "Picardy third") that music of the period closes on.
"""
from score import Score, chord, transpose

TITLE = "Greensleeves"
NATION, AGES, MOOD = "British", (3,), "ECONOMIC"

# 6/8, one beat = one eighth note; the tune starts on an upbeat A
TUNE = ("C5:2 D5:1 E5:1.5 F5:0.5 E5:1 | D5:2 B4:1 G4:1.5 A4:0.5 B4:1 | "
        "C5:2 A4:1 A4:1.5 G#4:0.5 A4:1 | B4:2 G#4:1 E4:2 A4:1 | "
        "C5:2 D5:1 E5:1.5 F5:0.5 E5:1 | D5:2 B4:1 G4:1.5 A4:0.5 B4:1 | "
        "C5:1.5 B4:0.5 A4:1 G#4:1.5 F#4:0.5 G#4:1 | A4:3 A4:3 | "
        "G5:3 G5:1.5 F#5:0.5 E5:1 | D5:2 B4:1 G4:1.5 A4:0.5 B4:1 | "
        "C5:2 A4:1 A4:1.5 G#4:0.5 A4:1 | B4:2 G#4:1 E4:3 | "
        "G5:3 G5:1.5 F#5:0.5 E5:1 | D5:2 B4:1 G4:1.5 A4:0.5 B4:1 | "
        "C5:1.5 B4:0.5 A4:1 G#4:1.5 F#4:0.5 G#4:1 | A4:5")
CHORDS = ["Am", "G", "Am", "E", "Am", "G", "Am E", "Am",
          "C", "G", "Am", "E", "C", "G", "Am E", "Am"]
DESCANT = ("A5:6 | B5:6 | A5:6 | G#5:6 | A5:6 | B5:6 | A5:3 G#5:3 | A5:6 | "
           "C6:6 | B5:6 | A5:6 | G#5:6 | C6:6 | B5:6 | A5:3 G#5:3 | A5:6")
BAR = 6


def build():
    s = Score(150, BAR)
    lute = s.track("lute", 24, gain=0.75, pan=-0.2, ring=1.6, swing=0.01)
    rec = s.track("recorder", 74, gain=0.9, pan=0.15, vib=(8, 5.0, 0.3), legato=0.9)
    viol = s.track("viol", 41, gain=0.85, pan=-0.1, vib=(10, 5.0, 0.25), legato=0.95)
    bass = s.track("bass viol", 42, gain=0.55, pan=0.1, legato=0.97, rev=0.3)
    hpsd = s.track("harpsichord", 6, gain=0.45, pan=0.3)

    def lute_part(at, last=False):
        for i, bar in enumerate(CHORDS):
            names = bar.split()
            each = BAR // len(names)
            for j, ch in enumerate(names):
                a = at + i * BAR + j * each
                low = chord(ch, 45)[0]
                tri = chord(ch, 57)
                pattern = [low, tri[0], tri[1], tri[2], tri[1], tri[0]] if each == 6 else [low, tri[1], tri[2]]
                for k, n in enumerate(pattern):
                    lute.add(a + k, 1, n, 66 if k == 0 else 50)

    def bass_part(at):
        for i, bar in enumerate(CHORDS):
            names = bar.split()
            each = BAR // len(names)
            for j, ch in enumerate(names):
                bass.add(at + i * BAR + j * each, each, chord(ch, 40)[0], 64)

    def harpsichord(at):
        for i, bar in enumerate(CHORDS):
            names = bar.split()
            for j, ch in enumerate(names * (2 // len(names))):
                for k in chord(ch, 60):
                    hpsd.add(at + i * BAR + j * 3, 3, k, 58)

    t = 0.0
    # the lute alone, two bars, the singer's upbeat in the last eighth
    for i, ch in enumerate(["Am", "E"]):
        low, tri = chord(ch, 45)[0], chord(ch, 57)
        for k, n in enumerate([low, tri[0], tri[1], tri[2], tri[1], tri[0]]):
            lute.add(t + i * BAR + k, 1, n, 62 if k else 70)
    t += 2 * BAR
    rec.play("A4:1", t - 1, 72)
    # I: recorder
    rec.play(TUNE, t, 78)
    lute_part(t)
    t += 16 * BAR
    # II: the viol sings it, the recorder above
    viol.play("A4:1", t - 1, 74)
    viol.play(TUNE, t, 80)
    rec.play(DESCANT, t, 58)
    lute_part(t)
    bass_part(t)
    t += 16 * BAR
    # III: all, the viol an octave down under the recorder
    rec.play("A4:1", t - 1, 76)
    viol.play("A3:1", t - 1, 70)
    rec.play(TUNE, t, 82)
    viol.play(transpose(TUNE, -12), t, 70)
    lute_part(t)
    bass_part(t)
    harpsichord(t)
    t += 16 * BAR
    s.tempo_at(t - 2 * BAR, 150)
    s.tempo_at(t, 116)
    # the Picardy third: close on A major
    for tr, notes, v in ((lute, "A2+E3+A3+C#4+E4", 70), (hpsd, "A3+C#4+E4+A4", 62),
                         (bass, "A2", 66), (rec, "C#5", 70), (viol, "A4", 66)):
        tr.play(f"{notes}:7", t, v)
    return s
