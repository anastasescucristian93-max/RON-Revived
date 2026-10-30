"""Indians, Medieval to Enlightenment ages, battle: Rajput Drums.

Rajput and Mughal armies went to war behind the nagara kettledrums carried on elephants and
camels, with shehnai shawms and long horns. Raga Bhairav's scale (C Db E F G Ab B), the
morning raga with the flat second and sixth. Original.
"""
from kit import CRASH2, Piece, drone

TITLE = "Rajput"
NATION, AGES, MOOD = "Indians", (2, 3, 4), "BATTLE_VICTORY"

A = ("C5:0.5 Db5 E5 F5 G5:1 F5:1 | E5:0.5 F5 E5 Db5 C5:2 | G5:0.5 Ab5 B5 C6 Db6:1 C6:1 | B5:0.5 Ab5 G5 F5 G5:2 | "
     "C6:1 B5:0.5 Ab5:0.5 G5:1 F5:1 | E5:0.5 F5 G5 Ab5 G5:2 | F5:0.5 E5 Db5 E5 F5:1 Db5:1 | C5:4")
B = ("G5:1 G5:1 Ab5:1 G5:1 | C6:2 B5:2 | Ab5:1 G5:1 F5:1 E5:1 | F5:2 G5:2 | "
     "Db6:1 C6:1 B5:1 Ab5:1 | G5:1 Ab5:0.5 G5:0.5 F5:2 | E5:1 F5:0.5 E5:0.5 Db5:1 E5:1 | C5:4")


def build():
    p = Piece(132, 4)
    shehnai = p.track("shehnai", 111, gain=0.9, pan=-0.1, legato=0.9, vib=(10, 6, 0.2))
    sitar = p.track("sitar", 104, gain=0.55, pan=0.25)
    horns = p.track("narsingha horns", 57, gain=0.6, rev=0.4, legato=1.0)
    hdrone = p.track("drone", 70, gain=0.35, legato=1.0)
    nagara = p.track("nagara", 116, gain=0.85, rev=0.25)
    dhol = p.track("dhol", 116, gain=0.55, pan=0.2, rev=0.2)
    cym = p.track("jhanjh", 0, drums=True, gain=0.3, pan=-0.25, rev=0.25)
    p.strain("A", A, ["C5"] * 8)
    p.strain("B", B, ["C5"] * 8)
    p.add(drone(hdrone, ["C3", "G3"], 60), lo=1)
    p.perc(nagara, [(0, 40, 114), (1, 40, 84), (2, 40, 104), (2.5, 40, 86), (3, 40, 96)], lo=1)
    p.perc(dhol, [(0.5, 52, 76), (1.5, 52, 80), (1.75, 52, 70), (3.5, 52, 84), (3.75, 52, 76)], lo=2)
    p.perc(cym, [(1, 52, 64), (3, 52, 70)], lo=2)

    horns.play("C3:4 G3:4", 0, 92)
    p.groove([None] * 2, 1)
    p.roll(nagara, 40, 4, 50, 112)
    p.groove([None], 1)
    p.play("A", [(shehnai, 92)], 2)
    p.play("A", [(shehnai, 94), (sitar, 72, -12)], 2)
    p.play("B", [(sitar, 88)], 2)
    p.play("B", [(shehnai, 96), (sitar, 76, -12)], 2)
    horns.play("C3:2 G3:2", p.t, 96)
    p.roll(nagara, 40, 4, 60, 120)
    p.groove([None], 1)
    cym.add(p.t, 1, CRASH2, 96)
    p.play("A", [(shehnai, 100), (sitar, 80, -12)], 3)
    p.final([(nagara, "E2:1 E2:1 E2:2", 124), (horns, "C3+G3:4", 98), (shehnai, "C6:2! r:2", 100)])
    return p.s
