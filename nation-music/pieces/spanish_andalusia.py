"""Spanish, Gunpowder to Industrial ages, economy: Andalusia.

The Spanish guitar over the "Andalusian cadence" (A minor, G, F, E), the descending chords
of Spanish music since the Renaissance and the root of flamenco; castanets and palmas
(hand claps) keep the sevillanas' 3/4. The melody sits in the Phrygian mode of the
cadence, with the raised third of its last chord. Original.
"""
from kit import CLAP, Piece, bass, ROOT
from kit import strum as strummed

TITLE = "Andalusia"
NATION, AGES, MOOD = "Spanish", (3, 4, 5), "ECONOMIC"

A = ("A5:1 C6:1 B5:0.5 A5:0.5 | G5:1 B5:1 A5:0.5 G5:0.5 | F5:1 A5:1 G5:0.5 F5:0.5 | E5:2 G#5:1 | "
     "A5:0.5 B5:0.5 C6:1 E6:1 | D6:0.5 C6:0.5 B5:1 D6:1 | C6:0.5 B5:0.5 A5:1 G#5:0.5 F5:0.5 | E5:3")
B = ("E6:1 D6:0.5 C6:0.5 G5:1 | D6:1 C6:0.5 B5:0.5 G5:1 | C6:1 B5:0.5 A5:0.5 F5:1 | B5:1 A5:0.5 G#5:0.5 E5:1 | "
     "A5:1 C6:1 E6:1 | D6:1.5 C6:0.5 A5:1 | B5:1 A5:0.5 G#5:0.5 F5:0.5 E5:0.5 | E5:3")


def build():
    p = Piece(132, 3)
    gtr = p.track("guitar", 24, gain=0.95, pan=-0.1, ring=1.2)
    strum = p.track("rasgueado", 24, gain=0.5, pan=0.25, ring=1.0)
    gtr2 = p.track("second guitar", 25, gain=0.9, pan=0.3, ring=1.1)
    b = p.track("bass", 32, gain=0.5, legato=0.6)
    cast = p.track("castanets", 115, bank=8, gain=0.35, pan=-0.3)
    palmas = p.track("palmas", 0, drums=True, gain=0.3, pan=0.35, rev=0.3)
    p.strain("A", A, ["Am", "G", "F", "E", "Am", "G", "Am E", "E"])
    p.strain("B", B, ["C", "G", "F", "E", "Am", "Dm", "E", "E"])
    # the rasgueado: a fast flick of the fingers, then the beats
    p.add(strummed(strum, [0, 1, 2], low=52, vel=66, dur=0.6), hi=2)
    p.add(strummed(strum, [0, 1, 1.5, 2, 2.5], low=52, vel=70, spread=0.03, dur=0.4), lo=3)
    p.add(bass(b, [(0, ROOT)], low=40, vel=74), lo=2)
    p.perc(cast, [(0, 60, 74), (1, 60, 60), (1.5, 60, 56), (2, 60, 66), (2.5, 60, 56)], lo=2)
    p.perc(palmas, [(1, CLAP, 70), (2, CLAP, 76)], lo=2)

    p.groove(["Am", "G", "F", "E"], 1)
    for strain, leads, lev in (("A", [(gtr, 84)], 1), ("A", [(gtr, 86)], 2), ("B", [(gtr2, 84)], 2),
                               ("B", [(gtr, 86), (gtr2, 66, -12)], 2), ("A", [(gtr, 90), (gtr2, 70, -12)], 3),
                               ("B", [(gtr, 92)], 3), ("A", [(gtr, 88)], 2)):
        p.play(strain, leads, lev)
    p.ritard(2, 96)
    p.final([(strum, "E2:0.04 B2:0.04 E3:0.04 G#3:0.04 B3:0.04 E4:2.8", 92), (gtr, "E5:3", 80), (b, "E2:3", 80)])
    return p.s
