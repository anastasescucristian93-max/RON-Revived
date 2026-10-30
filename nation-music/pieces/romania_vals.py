"""Romania, Industrial to Information ages, economy: Salon waltz.

Belle-epoque Bucharest, the "little Paris" of 1900: the lautar violin of the restaurants and
gardens, the tambal and the accordion, playing the waltzes of Ivanovici's time (his
"Waves of the Danube" went round the world in 1880). An A minor waltz with a C major
middle. Original.
"""
from kit import Piece, bass, pluck, ROOT

TITLE = "Vals"
NATION, AGES, MOOD = "Romania", (5, 6, 7), "ECONOMIC"

A = ("A4:1 C5:1 E5:1 | A5:2 G#5:1 | A5:3 | E5:3 | F5:1 E5:1 D5:1 | B4:2 D5:1 | C5:3 | A4:3 | "
     "B4:1 D5:1 F5:1 | B5:2 A5:1 | G#5:3 | E5:3 | F5:1 E5:1 D5:1 | C5:2 B4:1 | A4:3 | r:3")
B = ("G5:1 E5:1 C5:1 | G5:2 F5:1 | E5:3 | C5:3 | D5:1 E5:1 F5:1 | A5:2 G5:1 | F5:3 | D5:3 | "
     "E5:1 F5:1 G5:1 | C6:2 B5:1 | A5:1 G5:1 F5:1 | E5:3 | D5:1 E5:1 F5:1 | B4:2 D5:1 | C5:3 | r:3")
HA = ["Am", "Am", "Am", "Am", "Dm", "E", "Am", "Am", "E7", "E", "E", "E", "Dm", "E", "Am", "Am"]
HB = ["C", "C", "C", "C", "Dm", "Dm", "G7", "G7", "C", "C", "F", "C", "Dm", "G7", "C", "C"]


def build():
    p = Piece(168, 3)
    vln = p.track("violin", 40, gain=0.95, pan=-0.15, vib=(18, 6, 0.15), legato=0.95)
    tam = p.track("tambal", 15, gain=0.55, pan=-0.35, ring=1.2)
    acc = p.track("accordion", 21, gain=0.7, pan=0.2, legato=0.9)
    pno = p.track("piano", 0, gain=0.55, pan=0.25)
    b = p.track("bass", 32, gain=0.55, legato=0.7)
    strings = p.track("strings", 49, gain=0.3, rev=0.4, legato=1.0)
    p.strain("A", A, HA)
    p.strain("B", B, HB)
    # oom-pah-pah
    p.add(bass(b, [(0, ROOT)], low=36, vel=80, dur=0.9))
    p.add(pluck(pno, [(1, "c"), (2, "c")], low=55, vel=56, dur=0.6))
    p.add(pluck(strings, [(0, "c")], low=60, vel=42, dur=3), lo=2)

    p.groove(["Am", "E", "Am", "E"], 1)
    p.play("A", [(vln, 82)], 1)
    p.play("B", [(vln, 84), (tam, 60, 12)], 2)
    p.play("A", [(acc, 82), (vln, 66, -12)], 2)
    p.play("B", [(acc, 84), (vln, 72)], 2)
    p.play("A", [(vln, 88), (tam, 66, 12)], 3)
    p.ritard(4, 120)
    p.final([(vln, "A5:3", 80), (tam, "A3:1/6 C4 E4 A4 C5 E5 A5:2", 70), (pno, "A2+E3+A3+C4:3", 64), (b, "A1:3", 76)])
    return p.s
