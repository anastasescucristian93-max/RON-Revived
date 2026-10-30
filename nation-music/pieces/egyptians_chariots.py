"""Egyptians, Ancient to Medieval ages, battle: Chariots of Kadesh.

Ramesses II's army at Kadesh (1274 BC) moved to trumpets (two were found in Tutankhamun's
tomb, each with only a few notes) and drums. Trumpet calls over war drums, the ney racing in
the second strain. A with the hijaz colour (A Bb C# D E F G). Original.
"""
from kit import CONGA_LO, CRASH2, KICK, TAMB, Piece, bass, ROOT, FIFTH

TITLE = "Chariots"
NATION, AGES, MOOD = "Egyptians", (0, 1, 2), "BATTLE_VICTORY"

CALL = ("A4:1 A4:0.5 A4:0.5 E5:2 | D5:1 C#5:0.5 D5:0.5 E5:2 | F5:1 E5:1 D5:1 C#5:1 | Bb4:2 A4:2 | "
        "A4:1 C#5:1 E5:1 A5:1 | G5:1 F5:0.5 E5:0.5 F5:2 | E5:1 D5:1 C#5:0.5 Bb4:0.5 C#5:1 | A4:4")
RUN = ("A5:0.5 Bb5 C#6 D6 E6:1 D6:1 | C#6:0.5 D6 C#6 Bb5 A5:2 | D6:0.5 E6 F6 E6 D6:1 C#6:1 | Bb5:0.5 C#6 Bb5 A5 G5:2 | "
       "F5:0.5 G5 A5 Bb5 C#6:1 A5:1 | Bb5:0.5 A5 G5 F5 E5:2 | F5:0.5 E5 D5 C#5 D5:1 E5:1 | A5:4")


def build():
    p = Piece(128, 4)
    tpt = p.track("trumpets", 56, gain=0.9, pan=-0.1, legato=0.88, rev=0.35)
    ney = p.track("ney", 73, gain=0.8, pan=0.15, vib=(12, 5.5, 0.3), legato=0.9)
    oboe = p.track("double pipe", 68, gain=0.6, pan=0.25, legato=0.9)
    low = p.track("low strings", 48, gain=0.45, legato=0.8)
    drum = p.track("war drums", 116, gain=0.85, rev=0.25)
    kick = p.track("big drum", 0, drums=True, gain=0.6, rev=0.2)
    frame = p.track("frame drums", 0, drums=True, gain=0.45, pan=-0.2, rev=0.2)
    sistrum = p.track("sistra", 0, drums=True, gain=0.3, pan=0.3, rev=0.2)
    cym = p.track("cymbal", 0, drums=True, gain=0.3, rev=0.3)
    p.strain("CALL", CALL, ["A", "A", "Dm", "A", "A", "Dm", "A", "A"])
    p.strain("RUN", RUN, ["A", "A", "Dm", "Gm", "Dm", "Gm", "Dm", "A"])
    p.add(bass(low, [(0, ROOT), (1.5, ROOT), (2, FIFTH), (3, ROOT)], low=33, vel=82, dur=0.4), lo=2)
    p.perc(drum, [(0, 45, 110), (1, 45, 86), (2, 45, 104), (2.5, 45, 84), (3, 45, 96)], lo=1)
    p.perc(kick, [(0, KICK, 104), (2, KICK, 96)], lo=2)
    p.perc(frame, [(0.5, CONGA_LO, 70), (1.5, CONGA_LO, 74), (3.5, CONGA_LO, 80)], lo=2)
    p.perc(sistrum, [(i * 0.5, TAMB, 66 if i % 2 else 50) for i in range(8)], lo=2)

    tpt.play("A4:2 E5:2 | A5:4", 0, 92)
    p.groove([None] * 2, 1)
    p.roll(drum, 45, 4, 50, 110)
    p.groove([None], 1)
    p.play("CALL", [(tpt, 92)], 2)
    p.play("CALL", [(tpt, 94), (oboe, 70, 12)], 2)
    p.play("RUN", [(ney, 90)], 2)
    p.play("RUN", [(ney, 92), (oboe, 76)], 2)
    p.roll(drum, 45, 8, 60, 120)
    p.groove([None] * 2, 1)
    cym.add(p.t, 1, CRASH2, 96)
    p.play("CALL", [(tpt, 98), (ney, 80, 12), (oboe, 76, 12)], 3)
    p.final([(drum, "A2:1 A2:1 A2:2", 124), (tpt, "A4+E5:4!", 100), (low, "A1:4", 96)])
    cym.add(p.t + 2, 3, CRASH2, 110)
    return p.s
