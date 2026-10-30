"""Britain, Enlightenment age, battle: a fife and drum march.

The redcoat regiments of the 18th century marched and gave signals to fifes
and side drums; a quick march in G in the manner of "The British Grenadiers"
(an original tune, not that one). Two fifes in thirds over rudimental snare
drums and the bass drum; the second time the regiment's trumpets and the
kettledrums join.
"""
from score import Score, chord, diatonic, transpose

TITLE = "March"
NATION, AGES, MOOD = "British", (4, 5), "BATTLE_VICTORY"

A = ("G5:0.75 A5:0.25 B5:0.5 G5:0.5 | D6:1 B5:0.5 G5:0.5 | A5:0.75 B5:0.25 C6:0.5 A5:0.5 | F#5:0.5 A5:0.5 D5:1 | "
     "G5:0.75 A5:0.25 B5:0.5 G5:0.5 | C6:0.5 E6:0.5 D6:0.5 B5:0.5 | A5:0.5 G5:0.5 F#5:0.5 A5:0.5 | G5:1.5 D5:0.5",
     ["G", "G", "D7", "D", "G", "C G", "D", "G"])
B = ("B5:0.5 B5:0.5 C6:0.5 D6:0.5 | E6:1 D6:0.5 C6:0.5 | B5:0.5 G5:0.5 A5:0.5 B5:0.5 | A5:1.5 D5:0.5 | "
     "B5:0.5 B5:0.5 C6:0.5 D6:0.5 | E6:0.5 F#6:0.5 G6:0.5 E6:0.5 | D6:0.5 B5:0.5 A5:0.5 F#5:0.5 | G5:1.5 D5:0.5",
     ["G", "C", "G", "D", "G", "C", "D7", "G"])
G_MAJOR = "G A B C D E F#"
BAR = 2


def build():
    s = Score(112, BAR)
    fife1 = s.track("fife 1", 72, gain=0.9, pan=-0.15, legato=0.85, rev=0.2)
    fife2 = s.track("fife 2", 72, gain=0.6, pan=0.15, legato=0.85, rev=0.2)
    snare = s.track("side drum", 48, drums=True, gain=0.6, rev=0.2, pan=0.1, swing=0.004)
    bdrum = s.track("bass drum", 48, drums=True, gain=0.7, rev=0.25)
    cym = s.track("cymbals", 48, drums=True, gain=0.3, rev=0.3, pan=-0.2)
    tpt = s.track("trumpets", 56, gain=0.6, pan=0.2, legato=0.85)
    timp = s.track("timpani", 47, gain=0.55, rev=0.3)

    def drums(at, bars, roll_last=True):
        for i in range(bars):
            a = at + i * BAR
            bdrum.add(a, 0.5, 36, 100)
            bdrum.add(a + 1, 0.5, 36, 88)
            if roll_last and i == bars - 1:
                # a five-stroke roll into the next strain
                for j in range(12):
                    snare.add(a + j * 0.125, 0.125, 38, 50 + j * 3)
                snare.add(a + 1.5, 0.5, 38, 118)
                continue
            snare.add(a - 0.04, 0.04, 38, 60)  # the flam
            for off, v in ((0, 112), (0.5, 70), (0.75, 72), (1, 104), (1.5, 74), (1.75, 72)):
                snare.add(a + off, 0.25, 38, v)

    def strain(p, at, full):
        mel, chords = p
        fife1.play(mel, at, 88)
        fife2.play(diatonic(mel, -2, G_MAJOR), at, 76)
        drums(at, len(chords))
        if full:
            tpt.play(transpose(mel, -12), at, 82)
            for i, bar in enumerate(chords):
                for j, ch in enumerate(bar.split()):
                    root = chord(ch, 43)[0]
                    timp.add(at + i * BAR + j, 0.5, root if root <= 50 else root - 12, 88)
        return at + len(chords) * BAR

    t = 0.0
    # the drums beat the call
    cym.add(t, 2, 57, 70)
    drums(t, 4)
    t += 4 * BAR
    fife1.play("D5:0.5", t - 0.5, 84)  # the upbeat; each strain ends with the next one's
    for p in (A, A, B, B):
        t = strain(p, t, full=False)
    # the drums alone, then the regiment
    drums(t, 4)
    t += 4 * BAR
    cym.add(t, 2, 57, 90)
    fife1.play("D5:0.5", t - 0.5, 88)
    tpt.play("D4:0.5", t - 0.5, 80)
    for p in (A, A, B, B):
        t = strain(p, t, full=True)
    s.tempo_at(t, 112)
    s.tempo_at(t + BAR, 96)
    # the end: the tune's last call, a roll and the final blow
    fife1.play("G5:0.5 D5:0.5 G5:1!", t, 96)
    fife2.play("B4:0.5 B4:0.5 B4:1!", t, 84)
    tpt.play("G4+B4:0.5 D4+B4:0.5 G4+B4:1!", t, 92)
    for j in range(8):
        snare.add(t + j * 0.125, 0.125, 38, 60 + j * 5)
    snare.add(t + 1, 0.5, 38, 120)
    bdrum.add(t + 1, 1, 36, 120)
    timp.add(t + 1, 1, 43, 110)
    cym.add(t + 1, 2, 57, 100)
    return s
