"""Nubians, Ancient to Medieval ages, economy: Kissar.

The kissar (tanbur), the five-string lyre of Nubia, older than the pharaohs' harp, in the
swinging 6/8 of Nubian song; a flute sings over it, frame drums and hand claps keep the
lilt. C major pentatonic (C D E G A). Original.
"""
from kit import CLAP, CONGA_HI, CONGA_LO, Piece, arp, bass, ROOT

TITLE = "Kissar"
NATION, AGES, MOOD = "Nubians", (0, 1, 2), "ECONOMIC"

A = ("E5:2/3 G5:1/3 A5:2/3 G5:1/3 | E5:2/3 D5:1/3 C5:1 | D5:2/3 E5:1/3 G5:2/3 A5:1/3 | G5:2 | "
     "A5:2/3 C6:1/3 A5:2/3 G5:1/3 | E5:2/3 G5:1/3 D5:1 | E5:2/3 D5:1/3 C5:2/3 D5:1/3 | C5:2")
B = ("C6:2/3 C6:1/3 A5:2/3 C6:1/3 | D6:1 C6:1 | A5:2/3 G5:1/3 E5:2/3 G5:1/3 | A5:2 | "
     "G5:2/3 A5:1/3 G5:2/3 E5:1/3 | D5:1 E5:1 | G5:2/3 E5:1/3 D5:2/3 E5:1/3 | C5:2")


def build():
    p = Piece(84, 2)
    kissar = p.track("kissar", 46, gain=0.75, pan=-0.2, ring=1.3)
    lute = p.track("oud", 24, gain=0.6, pan=0.25, ring=1.2)
    flute = p.track("flute", 73, gain=0.9, pan=0.1, vib=(14, 5, 0.3), legato=0.95)
    b = p.track("bass", 32, gain=0.5, legato=0.6)
    drum = p.track("tar drums", 0, drums=True, gain=0.45, rev=0.2)
    clap = p.track("claps", 0, drums=True, gain=0.3, pan=0.3, rev=0.25)
    T = 1 / 3
    p.strain("A", A, ["C", "C", "G", "G", "Am", "C", "G", "C"])
    p.strain("B", B, ["Am", "G", "Am", "Am", "C", "G", "C", "C"])
    p.add(arp(kissar, T, order=(0, 1, 2, 3, 2, 1), low=55, vel=54, accent=12, every=3))
    p.add(bass(b, [(0, ROOT), (1, ROOT)], low=36, vel=72), lo=2)
    p.perc(drum, [(0, CONGA_LO, 84), (2 * T, CONGA_HI, 60), (1, CONGA_LO, 74), (1 + 2 * T, CONGA_HI, 66)], lo=1)
    p.perc(clap, [(1, CLAP, 70), (1 + 2 * T, CLAP, 54)], lo=2)

    p.groove(["C", "G", "C", "G"], 1)
    for strain, leads, lev in (("A", [(flute, 82)], 1), ("A", [(flute, 84), (lute, 60, -12)], 2),
                               ("B", [(lute, 84)], 2), ("B", [(flute, 86), (lute, 66, -12)], 2),
                               ("A", [(kissar, 80, 12)], 2), ("B", [(flute, 86)], 3), ("A", [(flute, 88), (lute, 70)], 3)):
        p.play(strain, leads, lev)
    p.ritard(2, 66)
    p.final([(kissar, "C4:1/3 E4 G4 C5:1", 74), (flute, "C5:2", 74), (b, "C2:2", 72)])
    return p.s
