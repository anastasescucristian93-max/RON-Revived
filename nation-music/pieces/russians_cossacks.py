"""Russians, Enlightenment to Modern ages, battle: Cossacks.

The Cossack hosts and the Russian army of Suvorov and Kutuzov: a men's choir singing the
chords as Russian army choirs do, brass on the tune, snare drums and timpani, and the
balalaikas driving underneath. D minor. Original.
"""
from kit import CRASH, KICK, SNARE, Piece, bass, pad, pluck, ROOT, FIFTH

TITLE = "Cossacks"
NATION, AGES, MOOD = "Russians", (4, 5, 6), "BATTLE_VICTORY"

A = ("D5:1 D5:0.5 E5:0.5 F5:1 D5:1 | A5:2 G5:1 F5:1 | E5:1 E5:0.5 F5:0.5 G5:1 E5:1 | A5:2 r:2 | "
     "Bb5:1 A5:0.5 G5:0.5 A5:1 F5:1 | G5:1 F5:0.5 E5:0.5 F5:1 D5:1 | E5:1 C#5:0.5 D5:0.5 E5:1 A4:1 | D5:4")
B = ("A5:1 A5:1 D6:1 A5:1 | Bb5:1 A5:0.5 G5:0.5 A5:2 | G5:1 G5:1 C6:1 G5:1 | A5:1 G5:0.5 F5:0.5 E5:2 | "
     "F5:1 G5:1 A5:1 D6:1 | C#6:1 Bb5:0.5 A5:0.5 G5:2 | F5:1 E5:0.5 D5:0.5 E5:1 C#5:1 | D5:4")


def build():
    p = Piece(126, 4)
    choir = p.track("men's choir", 52, gain=0.65, rev=0.45, legato=0.95)
    tpt = p.track("trumpets", 56, gain=0.85, pan=-0.15, legato=0.88)
    horns = p.track("horns", 60, gain=0.55, pan=0.2, rev=0.35)
    bala = p.track("balalaikas", 105, gain=0.4, pan=0.3, legato=0.3)
    tuba = p.track("tuba", 58, gain=0.55, legato=0.6)
    timp = p.track("timpani", 47, gain=0.5, rev=0.3)
    snare = p.track("snare", 48, drums=True, gain=0.5, rev=0.2)
    bdrum = p.track("bass drum", 48, drums=True, gain=0.65, rev=0.25)
    cym = p.track("cymbals", 48, drums=True, gain=0.35, rev=0.3)
    p.strain("A", A, ["Dm", "Dm", "C", "A", "Gm", "Dm", "A", "Dm"])
    p.strain("B", B, ["Dm", "Gm", "C", "A", "Dm", "A", "A", "Dm"])
    p.add(pad(choir, low=50, vel=66), lo=2)
    p.add(pluck(bala, [(0.5, "c"), (1, "c"), (1.5, "c"), (2.5, "c"), (3, "c"), (3.5, "c")], low=57, vel=58, dur=0.2))
    p.add(bass(tuba, [(0, ROOT), (2, FIFTH)], low=31, vel=82))
    p.add(bass(timp, [(0, ROOT), (2, FIFTH)], low=38, vel=84), lo=3)
    p.perc(bdrum, [(0, KICK, 104), (2, KICK, 96)])
    p.perc(snare, [(1, SNARE, 92), (3, SNARE, 96), (3.5, SNARE, 70), (3.75, SNARE, 74)], lo=2)
    p.perc(cym, [(0, CRASH, 74)], lo=3, every=2)

    p.groove(["Dm", "A"], 1)
    p.play("A", [(tpt, 90)], 1)
    p.play("A", [(tpt, 92), (horns, 72, -12)], 2)
    p.play("B", [(choir, 84)], 2)
    p.play("B", [(tpt, 94), (choir, 80)], 2)
    p.roll(snare, SNARE, 4, 50, 112, step=0.125)
    p.groove([None], 1)
    cym.add(p.t, 1, CRASH, 100)
    p.play("A", [(tpt, 100), (horns, 84, -12), (choir, 80, -12)], 3)
    p.play("B", [(tpt, 100), (choir, 86)], 3)
    p.ritard(1, 100)
    p.final([(choir, "D3+A3+D4+F4+A4:4", 90), (tpt, "D5+A5:4!", 100), (tuba, "D2:4", 96), (timp, "A2:0.25 A2 A2 A2 D2:3", 104)])
    cym.add(p.t, 3, CRASH, 110)
    return p.s
