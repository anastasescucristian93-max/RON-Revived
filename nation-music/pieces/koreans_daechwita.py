"""Koreans, Medieval to Enlightenment ages, battle: Daechwita.

Daechwita was the march of the Joseon royal and military processions: taepyeongso shawms on
the tune, the nabal long trumpet and the nagak conch holding single notes, the yonggo dragon
drum, the jabara cymbals and the jing gong. Slow and heavy, as it is still played at the
palace gates. Pyeongjo mode on C (C D F G A). Original tune.
"""
from kit import CHINA, CRASH2, Piece

TITLE = "Daechwita"
NATION, AGES, MOOD = "Koreans", (2, 3, 4), "BATTLE_VICTORY"

A = ("C5:2 D5:1 F5:1 | G5:3 A5:0.5 G5:0.5 | F5:1 G5:1 F5:1 D5:1 | C5:4 | "
     "F5:2 G5:1 A5:1 | C6:3 A5:0.5 C6:0.5 | A5:1 G5:1 F5:1 G5:1 | F5:4")
B = ("A5:2 C6:1 A5:1 | G5:2 F5:2 | D5:1 F5:1 G5:1 A5:1 | G5:4 | "
     "C6:1 A5:1 G5:1 F5:1 | D5:2 F5:2 | G5:1 F5:0.5 D5:0.5 C5:1 D5:1 | C5:4")


def build():
    p = Piece(84, 4)
    shawm = p.track("taepyeongso", 111, gain=0.9, pan=-0.1, vib=(18, 5.5, 0.25), legato=0.95)
    shawm2 = p.track("taepyeongso 2", 111, gain=0.55, pan=0.2, vib=(18, 5.2, 0.3), legato=0.95)
    nabal = p.track("nabal", 56, gain=0.55, rev=0.45, legato=1.0)
    nagak = p.track("nagak", 60, gain=0.5, rev=0.45, legato=1.0)
    yonggo = p.track("yonggo", 116, gain=0.9, rev=0.3)
    jabara = p.track("jabara", 0, drums=True, gain=0.35, pan=0.25, rev=0.3)
    jing = p.track("jing", 0, drums=True, gain=0.45, rev=0.5)
    p.strain("A", A, [None] * 8)
    p.strain("B", B, [None] * 8)
    p.perc(yonggo, [(0, 38, 112), (1.5, 38, 80), (2, 38, 104), (3, 38, 90)], lo=1)
    p.perc(jabara, [(0, CHINA, 80)], lo=2, every=2)
    p.perc(jing, [(0, CRASH2, 92)], lo=1, every=4)

    p.groove([None] * 2, 1)
    nabal.play("C4:4 | C4:4", 0, 90)
    for strain, leads, lev in (("A", [(shawm, 90)], 1), ("B", [(shawm, 92)], 2),
                               ("A", [(shawm, 94), (shawm2, 74, -12)], 2), ("B", [(shawm, 96), (shawm2, 78, -12)], 2)):
        start = p.t
        p.play(strain, leads, lev)
        nabal.play("C4:4", start, 86)
        nagak.play("r:16 G3:4", start, 80)
    p.groove([None] * 2, 2)
    nabal.play("C4:4 G4:4", p.t - 8, 94)
    p.play("A", [(shawm, 100), (shawm2, 84, -12)], 3)
    p.play("B", [(shawm, 100), (shawm2, 86, -12)], 3)
    p.ritard(2, 66)
    p.final([(yonggo, "D2:1 D2:1 D2:2", 120), (nabal, "C4:4", 96), (jing, "A3:4", 110), (shawm, "C5:4", 96)])
    return p.s
