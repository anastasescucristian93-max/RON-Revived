"""Spanish, Gunpowder and Enlightenment ages, battle: The Tercios.

The tercios, the pike-and-shot infantry that made Spain the great power of the 16th and 17th
centuries, marched to the "atambor" (side drum) and the fife, their captains signalled with
trumpets. D minor; the first strain stops on the A major chord, the Phrygian half close of
Spanish music. Original.
"""
from kit import CRASH, KICK, SNARE, Piece, bass, pad, ROOT, FIFTH

TITLE = "Tercios"
NATION, AGES, MOOD = "Spanish", (3, 4), "BATTLE_VICTORY"

A = ("A4:1 D5:1 F5:1.5 E5:0.5 | D5:1 E5:1 F5:2 | G5:1 F5:0.5 E5:0.5 F5:1 D5:1 | E5:4 | "
     "A5:1 G5:0.5 F5:0.5 G5:1 E5:1 | F5:1 E5:0.5 D5:0.5 E5:1 C5:1 | D5:1 C5:0.5 Bb4:0.5 C5:1 Bb4:1 | A4:4")
B = ("D5:1 F5:1 A5:1 D6:1 | C#6:1.5 Bb5:0.5 A5:2 | G5:1 Bb5:1 A5:1 G5:1 | F5:2 E5:2 | "
     "D5:1 F5:1 A5:1 F5:1 | Bb5:1.5 A5:0.5 G5:1 F5:1 | E5:1 F5:0.5 E5:0.5 D5:1 C#5:1 | D5:4")


def build():
    p = Piece(116, 4)
    tpt = p.track("trumpets", 56, gain=0.9, pan=-0.15, legato=0.88)
    fife = p.track("fife", 72, gain=0.6, pan=0.2, legato=0.85)
    shawm = p.track("shawms", 111, gain=0.45, pan=0.25, legato=0.9)
    sack = p.track("sackbuts", 57, gain=0.5, rev=0.3)
    b = p.track("bass", 58, gain=0.5, legato=0.6)
    snare = p.track("atambores", 48, drums=True, gain=0.6, rev=0.2, swing=0.004)
    bdrum = p.track("bass drum", 48, drums=True, gain=0.65, rev=0.25)
    cym = p.track("cymbal", 48, drums=True, gain=0.3, rev=0.3)
    p.strain("A", A, ["Dm", "Dm", "Gm", "A", "C", "C", "Bb", "A"])
    p.strain("B", B, ["Dm", "A", "Gm", "Dm A", "Dm", "Gm", "A", "Dm"])
    p.add(pad(sack, low=50, vel=56), lo=2)
    p.add(bass(b, [(0, ROOT), (2, FIFTH)], low=31, vel=80), lo=2)
    p.perc(bdrum, [(0, KICK, 104), (2, KICK, 94)])
    p.perc(snare, [(0, SNARE, 108), (0.5, SNARE, 66), (1, SNARE, 96), (1.5, SNARE, 70), (1.75, SNARE, 70),
                   (2, SNARE, 104), (3, SNARE, 94), (3.5, SNARE, 72)])

    p.roll(snare, SNARE, 4, 40, 110, step=0.125)
    p.t += 4
    p.groove([None], 1)
    for strain, leads, lev in (("A", [(tpt, 88)], 1), ("B", [(tpt, 90), (fife, 70, 12)], 2),
                               ("A", [(fife, 86, 12), (shawm, 70)], 2), ("B", [(tpt, 92), (shawm, 76)], 2)):
        p.play(strain, leads, lev)
    p.roll(snare, SNARE, 4, 50, 114, step=0.125)
    p.groove([None], 1)
    cym.add(p.t, 1, CRASH, 96)
    p.play("A", [(tpt, 96), (fife, 82, 12), (sack, 70, -12)], 3)
    p.play("B", [(tpt, 98), (fife, 84, 12)], 3)
    p.ritard(1, 92)
    p.final([(tpt, "A4+E5:2 D5+A5:2!", 100), (b, "A1:2 D2:2", 96), (sack, "A3+C#4:2 A3+D4+F4:2", 84)])
    cym.add(p.t + 2, 2, CRASH, 110)
    return p.s
