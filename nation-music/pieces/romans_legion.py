"""Romans, Ancient to Medieval ages, battle: The Legion.

The legion was ordered by brass: the tuba (a straight trumpet) for the advance and retreat,
the cornu (the great round horn) for the standards, the bucina for the watches (Vegetius,
De re militari). Cornua and tubae over war drums in a steady march. D minor. Original.
"""
from kit import CRASH2, KICK, SNARE, Piece, bass, pad, ROOT, FIFTH

TITLE = "Legion"
NATION, AGES, MOOD = "Romans", (0, 1, 2), "BATTLE_VICTORY"

CORNU = ("D4:1 D4:0.5 D4:0.5 A4:2 | F4:1 G4:0.5 A4:0.5 D5:2 | C5:1 Bb4:0.5 A4:0.5 G4:1 A4:1 | D4:4 | "
         "A4:1 A4:0.5 Bb4:0.5 C5:1 A4:1 | D5:1 C5:0.5 Bb4:0.5 A4:2 | G4:1 F4:0.5 E4:0.5 F4:1 E4:1 | D4:4")
TUBA = ("A4:1 D5:1 F5:1 D5:1 | G5:2 F5:2 | E5:1 F5:0.5 G5:0.5 A5:1 F5:1 | E5:4 | "
        "D5:1 F5:1 A5:1 D6:1 | C6:1 Bb5:0.5 A5:0.5 G5:2 | F5:1 E5:0.5 D5:0.5 E5:1 C#5:1 | D5:4")


def build():
    p = Piece(108, 4)
    cornu = p.track("cornua", 60, gain=0.9, pan=-0.15, rev=0.4, legato=0.95)
    tuba = p.track("tubae", 56, gain=0.85, pan=0.15, legato=0.88)
    low = p.track("low brass", 57, gain=0.5, rev=0.3)
    bassb = p.track("bass", 58, gain=0.5, legato=0.6)
    drum = p.track("war drum", 116, gain=0.8, rev=0.25)
    kick = p.track("march", 0, drums=True, gain=0.5, rev=0.2)
    snare = p.track("drums", 48, drums=True, gain=0.45, rev=0.2)
    cym = p.track("cymbal", 0, drums=True, gain=0.3, rev=0.35)
    p.strain("C", CORNU, ["Dm", "Dm", "C", "Dm", "F", "Bb", "A", "Dm"])
    p.strain("T", TUBA, ["Dm", "Gm", "C", "A", "Dm", "Gm", "A", "Dm"])
    p.add(pad(low, low=50, vel=56), lo=2)
    p.add(bass(bassb, [(0, ROOT), (2, FIFTH)], low=31, vel=80), lo=2)
    p.perc(drum, [(0, 41, 110), (2, 41, 100)], lo=1)
    p.perc(kick, [(0, KICK, 92), (1, KICK, 74), (2, KICK, 88), (3, KICK, 74)], lo=2)
    p.perc(snare, [(0.5, SNARE, 60), (1.5, SNARE, 66), (2.5, SNARE, 60), (3.5, SNARE, 70), (3.75, SNARE, 64)], lo=2)
    p.perc(cym, [(0, CRASH2, 76)], lo=3, every=4)

    cornu.play("D4:2 A4:2 | D5:4", 0, 94)
    p.groove([None] * 2, 1)
    for strain, leads, lev in (("C", [(cornu, 90)], 1), ("C", [(cornu, 92), (tuba, 72, 12)], 2),
                               ("T", [(tuba, 92)], 2), ("T", [(tuba, 94), (cornu, 76, -12)], 2)):
        p.play(strain, leads, lev)
    cornu.play("D4:2 A4:2", p.t, 96)
    p.roll(drum, 41, 4, 60, 118)
    p.groove([None], 1)
    p.play("C", [(cornu, 98), (tuba, 86, 12)], 3)
    p.play("T", [(tuba, 100), (cornu, 86, -12)], 3)
    p.final([(drum, "F2:1 F2:1 F2:2", 124), (cornu, "D4+A4:4!", 100), (tuba, "D5:4", 100), (bassb, "D2:4", 96)])
    cym.add(p.t + 2, 3, CRASH2, 110)
    return p.s
