"""Persians, Ancient to Enlightenment ages, economy: Santur.

Persian classical music: the santur (hammered dulcimer) with its rippling tremolo, the ney
flute, the kamancheh spike fiddle and the tombak goblet drum, in a lilting 6/8. Close to
dastgah Shur on D (D Eb F G A Bb C; the real Shur's second is a quarter tone lower). Original.
"""
from kit import BONGO_HI, CONGA_HI, CONGA_LO, Piece, arp, drone

TITLE = "Santur"
NATION, AGES, MOOD = "Persians", (0, 1, 2, 3, 4), "ECONOMIC"

A = ("A5:2/3 Bb5:1/3 A5:2/3 G5:1/3 | F5:1 G5:1 | Eb5:2/3 F5:1/3 G5:2/3 F5:1/3 | D5:2 | "
     "F5:2/3 G5:1/3 A5:2/3 Bb5:1/3 | C6:1 Bb5:1 | A5:2/3 G5:1/3 F5:2/3 Eb5:1/3 | D5:2")
B = ("D6:2/3 C6:1/3 Bb5:2/3 C6:1/3 | D6:1 A5:1 | Bb5:2/3 A5:1/3 G5:2/3 A5:1/3 | Bb5:2 | "
     "A5:2/3 G5:1/3 F5:2/3 G5:1/3 | A5:1 F5:1 | G5:2/3 F5:1/3 Eb5:2/3 F5:1/3 | D5:2")


def build():
    p = Piece(76, 2)
    santur = p.track("santur", 15, gain=0.65, pan=-0.2, ring=1.2)
    ney = p.track("ney", 77, gain=0.9, pan=0.1, vib=(18, 4.5, 0.35), legato=0.97, rev=0.45)
    kam = p.track("kamancheh", 110, gain=0.8, pan=0.25, vib=(26, 5.5, 0.2), legato=0.97)
    tombak = p.track("tombak", 0, drums=True, gain=0.45, rev=0.2)
    pad = p.track("drone", 49, gain=0.16, rev=0.5, legato=1.0)
    T = 1 / 6  # the santur's tremolo, two strokes per triplet
    p.strain("A", A, ["Dm", "Gm", "Cm", "Dm", "Dm", "Cm", "Gm", "Dm"])
    p.strain("B", B, ["Gm", "Dm", "Gm", "Gm", "Dm", "Dm", "Cm", "Dm"])
    p.add(arp(santur, T, order=(0, 3, 1, 3, 2, 3), low=50, vel=44, accent=12, every=6), lo=1)
    p.add(drone(pad, ["D3", "A3"], 40), lo=2)
    p.perc(tombak, [(0, CONGA_LO, 86), (2 / 3, BONGO_HI, 60), (1, CONGA_HI, 72), (1 + 2 / 3, BONGO_HI, 58)], lo=2)

    santur.play("D5:1/6 Eb5 F5 G5 A5 Bb5 C6 D6 C6 Bb5 A5 G5 | F5:1/6 Eb5 D5 Eb5 F5 Eb5 D5:1", 0, 62)
    p.t += 4
    p.play("A", [(ney, 82)], 1)
    p.play("A", [(ney, 84)], 2)
    p.play("B", [(kam, 84), (ney, 64, -12)], 2)
    p.play("B", [(ney, 86), (kam, 70)], 2)
    p.play("A", [(santur, 78, 12)], 2)
    p.play("A", [(kam, 86), (ney, 72)], 3)
    p.ritard(2, 56)
    p.final([(santur, "D4:1/6 A4 D5 F5 A5 D6:2", 70), (ney, "D5:3", 72), (pad, "D3+A3:3", 40)])
    return p.s
