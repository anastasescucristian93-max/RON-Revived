"""Dutch, Gunpowder and Enlightenment ages, battle: The Sea Beggars.

The Eighty Years' War: the Geuzen (the "Beggars") who took Brielle in 1572 and broke the
siege of Leiden. Drums and trumpets of the States army, a fife over them, the low brass
under. D minor. Original.
"""
from kit import CRASH, KICK, SNARE, Piece, bass, pad, ROOT, FIFTH

TITLE = "SeaBeggars"
NATION, AGES, MOOD = "Dutch", (3, 4), "BATTLE_VICTORY"

A = ("D5:1 D5:0.5 F5:0.5 A5:1 A5:1 | Bb5:1 A5:0.5 G5:0.5 A5:2 | F5:1 G5:0.5 A5:0.5 Bb5:1 G5:1 | E5:1 C#5:1 D5:2 | "
     "A5:1 A5:0.5 Bb5:0.5 C6:1 A5:1 | D6:1 C6:0.5 Bb5:0.5 A5:2 | G5:0.5 F5:0.5 E5:0.5 F5:0.5 G5:1 E5:1 | D5:4")
B = ("F5:1 F5:1 G5:1 A5:1 | Bb5:2 A5:2 | G5:1 G5:1 A5:1 Bb5:1 | C6:2 A5:2 | "
     "D6:1 C6:1 Bb5:1 A5:1 | G5:1 A5:1 F5:2 | E5:1 F5:0.5 G5:0.5 E5:1 C#5:1 | D5:4")


def build():
    p = Piece(112, 4)
    tpt = p.track("trumpets", 56, gain=0.9, pan=-0.15, legato=0.85)
    fife = p.track("fife", 72, gain=0.6, pan=0.2, legato=0.85)
    sack = p.track("sackbuts", 57, gain=0.55, pan=0.15, rev=0.3)
    tuba = p.track("bass", 58, gain=0.5, legato=0.7)
    snare = p.track("drums", 48, drums=True, gain=0.6, rev=0.2, swing=0.004)
    bdrum = p.track("bass drum", 48, drums=True, gain=0.7, rev=0.25)
    cym = p.track("cymbal", 48, drums=True, gain=0.3, rev=0.3)
    timp = p.track("kettledrums", 47, gain=0.5, rev=0.3)
    p.strain("A", A, ["Dm", "Gm", "Gm", "A", "F", "Gm", "A", "Dm"])
    p.strain("B", B, ["F", "Bb", "C", "F", "Bb", "Gm", "A", "Dm"])
    p.add(pad(sack, low=50, vel=58), lo=2)
    p.add(bass(tuba, [(0, ROOT), (2, FIFTH)], low=31, vel=80), lo=2)
    p.add(bass(timp, [(0, ROOT), (2, FIFTH)], low=38, vel=84), lo=3)
    p.perc(bdrum, [(0, KICK, 104), (2, KICK, 92)])
    p.perc(snare, [(0, SNARE, 110), (0.75, SNARE, 64), (1, SNARE, 90), (2, SNARE, 106), (2.5, SNARE, 70),
                   (2.75, SNARE, 70), (3, SNARE, 96), (3.5, SNARE, 72)])

    p.roll(snare, SNARE, 4, 40, 110, step=0.125)
    p.t += 4
    p.groove([None], 1)
    for strain, leads, lev in (("A", [(tpt, 88)], 1), ("A", [(tpt, 90), (fife, 70, 12)], 2),
                               ("B", [(tpt, 90)], 2), ("B", [(tpt, 92), (fife, 74, 12)], 2)):
        p.play(strain, leads, lev)
    p.roll(snare, SNARE, 4, 50, 112, step=0.125)
    p.groove([None], 1)
    cym.add(p.t, 1, CRASH, 96)
    p.play("A", [(tpt, 96), (fife, 82, 12), (sack, 70, -12)], 3)
    p.play("B", [(tpt, 96), (fife, 84, 12)], 3)
    p.ritard(1, 92)
    p.final([(tpt, "A4+E5:2 D5+A5:2!", 100), (tuba, "A1:2 D2:2", 96), (timp, "A2:2 D2:2", 104)])
    cym.add(p.t + 2, 2, CRASH, 110)
    return p.s
