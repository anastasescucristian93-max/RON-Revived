"""Dutch, Gunpowder and Enlightenment ages, economy: Carillon.

The Golden Age Republic: the carillon in the church tower playing over the market square,
answered indoors by a consort of recorder, lute, viol and harpsichord in the manner of
Sweelinck's variations. G major, 3/4. Original.
"""
from kit import Piece, bass, pluck, ROOT

TITLE = "Carillon"
NATION, AGES, MOOD = "Dutch", (3, 4), "ECONOMIC"

A = ("G5:1 B5:1 D6:1 | C6:1.5 B5:0.5 A5:1 | B5:1 G5:1 A5:1 | F#5:2 D5:1 | "
     "G5:1 A5:1 B5:1 | C6:1 E6:1 D6:1 | C6:0.5 B5:0.5 A5:1 F#5:1 | G5:3")
B = ("D6:1 D6:1 E6:1 | D6:1.5 C6:0.5 B5:1 | C6:1 C6:1 D6:1 | C6:1.5 B5:0.5 A5:1 | "
     "B5:1 G5:1 C6:1 | B5:1 A5:1 G5:1 | A5:1.5 G5:0.5 F#5:1 | G5:3")
# Sweelinck's way: the tune in running eighths
AV = ("G5:0.5 A5 B5 C6 D6 B5 | C6:0.5 D6 B5 C6 A5 B5 | B5:0.5 A5 G5 B5 A5 G5 | F#5:0.5 E5 D5 E5 F#5 D5 | "
      "G5:0.5 F#5 A5 G5 B5 A5 | C6:0.5 B5 E6 D6 D6 C6 | C6:0.5 B5 A5 G5 F#5 A5 | G5:0.5 D5 B4 D5 G5:1")


def build():
    p = Piece(96, 3)
    bells = p.track("carillon", 14, bank=9, gain=0.85, pan=0.0, rev=0.55, ring=2.0)
    rec = p.track("recorder", 74, gain=0.85, pan=-0.15, vib=(8, 5, 0.3), legato=0.9)
    lute = p.track("lute", 24, gain=0.55, pan=0.25, ring=1.4)
    viol = p.track("bass viol", 42, gain=0.5, pan=0.1, legato=0.95, rev=0.3)
    hpsd = p.track("harpsichord", 6, gain=0.5, pan=-0.3)
    p.strain("A", A, ["G", "C", "Em", "D", "G", "C", "D", "G"])
    p.strain("B", B, ["G", "G", "Am", "D", "C", "G", "D", "G"])
    p.strain("AV", AV, ["G", "C", "Em", "D", "G", "C", "D", "G"])
    p.add(pluck(lute, [(0, 0), (1, 1), (1.5, 2), (2, 3)], low=50, vel=58), lo=1)
    p.add(bass(viol, [(0, ROOT)], low=38, vel=66, dur=3), lo=2)
    p.add(pluck(hpsd, [(0, "c"), (1, "c"), (2, "c")], low=55, vel=50, dur=0.9), lo=3)

    # the tower first, alone over the square
    p.play("A", [(bells, 84)], 0)
    p.play("B", [(bells, 84)], 0)
    p.play("A", [(rec, 82)], 1)
    p.play("B", [(rec, 82)], 2)
    p.play("AV", [(rec, 80)], 2)
    p.play("B", [(rec, 84), (viol, 60, -12)], 3)
    p.play("A", [(bells, 86), (rec, 70)], 3)
    p.ritard(2, 70)
    p.final([(bells, "G4+B4+D5+G5:6", 84), (lute, "G2+D3+G3+B3:4", 64), (viol, "G2:4", 64)])
    return p.s
