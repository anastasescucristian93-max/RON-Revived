"""Inca, Ancient to Gunpowder ages, economy: Huayno of the Andes.

The huayno, the oldest dance of the Andes, in its long-short-short step: siku pan pipes
played in two halves that interlock, the quena flute over them, the bombo drum and the
chajchas (rattles of goat hooves). A minor pentatonic (A C D E G). Original.
"""
from kit import CABASA, KICK, Piece, drone

TITLE = "Huayno"
NATION, AGES, MOOD = "Inca", (0, 1, 2, 3), "ECONOMIC"

A = ("E5:0.5 E5:0.25 E5:0.25 D5:0.5 C5:0.5 | D5:0.5 E5:0.5 A4:1 | C5:0.5 C5:0.25 D5:0.25 E5:0.5 G5:0.5 | "
     "E5:1 r:0.5 E5:0.25 G5:0.25 | A5:0.5 A5:0.25 G5:0.25 E5:0.5 D5:0.5 | E5:0.5 D5:0.5 C5:1 | "
     "D5:0.5 C5:0.5 A4:0.5 G4:0.5 | A4:1.5 r:0.5")
B = ("A5:0.5 G5:0.5 A5:0.5 C6:0.5 | D6:1 C6:1 | A5:0.5 G5:0.5 E5:0.5 G5:0.5 | A5:1.5 r:0.5 | "
     "C6:0.5 A5:0.5 G5:0.5 E5:0.5 | D5:0.5 E5:0.5 G5:1 | E5:0.5 D5:0.5 C5:0.5 D5:0.5 | A4:1.5 r:0.5")


def build():
    p = Piece(100, 2)
    siku = p.track("siku", 75, gain=0.95, pan=-0.15, vib=(8, 5, 0.3), legato=0.9)
    quena = p.track("quena", 73, gain=0.85, pan=0.15, vib=(16, 5.5, 0.25), legato=0.9)
    low = p.track("toyo (low siku)", 75, gain=0.4, pan=0.0, legato=1.0)
    bombo = p.track("bombo", 0, drums=True, gain=0.55, rev=0.25)
    rim = p.track("bombo rim", 0, drums=True, gain=0.3, pan=0.2)
    chaj = p.track("chajchas", 0, drums=True, gain=0.28, pan=-0.3)
    p.strain("A", A, ["Am", "Am", "C", "C", "Am", "Am", "G", "Am"])
    p.strain("B", B, ["Am", "G", "Em", "Am", "C", "G", "C", "Am"])
    p.add(drone(low, ["A3"], 50), lo=2)
    # the huayno step: a strong beat, then two short ones
    p.perc(bombo, [(0, KICK, 100), (1, KICK, 80), (1.5, KICK, 72)])
    p.perc(rim, [(0.5, 37, 60), (1.75, 37, 54)], lo=2)
    p.perc(chaj, [(i * 0.25, CABASA, 56 if i % 2 == 0 else 38) for i in range(8)], lo=2)

    p.groove([None] * 4, 1)
    for strain, leads, lev in (("A", [(siku, 84)], 1), ("A", [(siku, 84)], 2), ("B", [(quena, 86)], 2),
                               ("B", [(quena, 86), (siku, 66, -12)], 2), ("A", [(siku, 88), (quena, 70, 12)], 2),
                               ("A", [(quena, 88), (siku, 74)], 2), ("B", [(siku, 90), (quena, 78, 12)], 3),
                               ("B", [(siku, 90), (quena, 80, 12)], 3), ("A", [(siku, 90), (quena, 80)], 3)):
        p.play(strain, leads, lev)
    p.ritard(2, 76)
    p.final([(siku, "A4:3", 80), (quena, "A5:3", 72), (bombo, "C2:1", 104)])
    return p.s
