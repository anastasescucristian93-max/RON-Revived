"""Bantu, Ancient to Enlightenment ages, battle: War Drums.

Drums called the regiments together and carried them into the fight: the big drum and the
talking drum over a 12/8 bell, kudu horns calling the tune, the marimba hammering under it.
F major. Original.
"""
from kit import AGOGO_HI, AGOGO_LO, CABASA, CONGA_HI, CONGA_LO, CONGA_MUTE, KICK, Piece, arp, bass, ROOT

TITLE = "WarDrums"
NATION, AGES, MOOD = "Bantu", (0, 1, 2, 3, 4), "BATTLE_VICTORY"

T = 1 / 3
A = ("F4:1 A4:1 C5:2 | Bb4:1 A4:1 G4:2 | F4:1 A4:1 C5:1 D5:1 | C5:4 | "
     "F5:1 D5:1 C5:2 | Bb4:2/3 A4:1/3 G4:1 F4:2 | G4:1 A4:1 C5:1 A4:1 | F4:4")
B = ("C5:2/3 C5:1/3 D5:2/3 C5:1/3 A4:2 | Bb4:2/3 Bb4:1/3 C5:2/3 Bb4:1/3 G4:2 | "
     "A4:2/3 C5:1/3 D5:2/3 F5:1/3 D5:1 C5:1 | C5:4 | "
     "F5:2/3 D5:1/3 C5:2/3 A4:1/3 C5:2 | D5:2/3 C5:1/3 A4:2/3 G4:1/3 F4:2 | "
     "G4:2/3 A4:1/3 C5:2/3 A4:1/3 G4:1 E4:1 | F4:4")
CYCLE = ["F", "Bb", "F", "C", "F", "Bb", "C", "F"]


def build():
    p = Piece(116, 4)
    horn = p.track("kudu horns", 60, gain=0.95, pan=-0.1, legato=0.9, rev=0.35)
    horn2 = p.track("horns 2", 61, gain=0.6, pan=0.2, legato=0.9)
    mar = p.track("marimba", 12, gain=0.55, pan=0.25)
    b = p.track("bass", 32, gain=0.55, legato=0.6)
    big = p.track("big drum", 0, drums=True, gain=0.7, rev=0.2)
    talk = p.track("talking drum", 0, drums=True, gain=0.5, pan=-0.2, rev=0.15)
    bell = p.track("bells", 0, drums=True, gain=0.32, pan=0.3)
    shaker = p.track("shaker", 0, drums=True, gain=0.28, pan=-0.3)
    p.strain("A", A, CYCLE)
    p.strain("B", B, CYCLE)
    p.add(arp(mar, T, order=(0, 1, 2, 3, 2, 1), low=57, vel=58, accent=12, every=3), lo=2)
    p.add(bass(b, [(0, ROOT), (2, ROOT)], low=36, vel=84), lo=2)
    p.perc(big, [(0, KICK, 112), (1, KICK, 80), (2, KICK, 104), (3, KICK, 84), (3 + 2 * T, KICK, 76)], lo=1)
    p.perc(talk, [(T, CONGA_HI, 70), (2 * T, CONGA_MUTE, 64), (1 + T, CONGA_LO, 84), (2 + 2 * T, CONGA_HI, 76),
                  (3 + T, CONGA_LO, 86), (3 + 2 * T, CONGA_MUTE, 66)], lo=2)
    p.perc(bell, [(i * T, AGOGO_HI if i in (0, 7) else AGOGO_LO, 84 if i == 0 else 66) for i in (0, 2, 4, 5, 7, 9, 11)])
    p.perc(shaker, [(i * T, CABASA, 60 if i % 3 == 0 else 42) for i in range(12)], lo=2)

    p.groove([None] * 2, 1)
    p.roll(talk, CONGA_HI, 4, 50, 110, step=T)
    p.groove([None] * 2, 1)
    p.play("A", [(horn, 92)], 2)
    p.play("A", [(horn, 94), (horn2, 76, 12)], 2)
    p.play("B", [(horn, 94), (mar, 70, 12)], 2)
    p.play("B", [(horn, 96), (horn2, 82, 12)], 2)
    p.roll(talk, CONGA_LO, 8, 60, 120, step=T)
    p.groove([None] * 2, 1)
    p.play("A", [(horn, 100), (horn2, 88, 12)], 3)
    p.final([(big, "C2:1 C2:1 C2:2", 124), (horn, "F4+C5+F5:4", 100), (b, "F2:4", 96)])
    return p.s
