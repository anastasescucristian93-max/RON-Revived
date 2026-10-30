"""Japan, Medieval to Enlightenment ages, economy: Sakura Sakura.

The Edo-period koto song (traditional, public domain) in the miyako-bushi
"in" scale on E (E F A B C). The koto sings it first, then the shakuhachi
(the bamboo flute of the komuso monks) over the koto, then both, with soft
strings under it the way the game's own tracks do.
"""
from score import Score, midi, transpose

TITLE = "Sakura"
NATION, AGES, MOOD = "Japanese", (2, 3, 4), "ECONOMIC"

SAKURA = ("A4:1 A4:1 B4:2 | A4:1 A4:1 B4:2 | A4:1 B4:1 C5:1 B4:1 | A4:1 B4:0.5 A4:0.5 F4:2 | "
          "E4:1 C4:1 E4:1 F4:1 | E4:1 E4:0.5 C4:0.5 B3:2 | A4:1 B4:1 C5:1 B4:1 | A4:1 B4:0.5 A4:0.5 F4:2 | "
          "E4:1 C4:1 E4:1 F4:1 | E4:1 E4:0.5 C4:0.5 B3:2 | A4:1 A4:1 B4:2 | A4:1 A4:1 B4:2 | "
          "E4:1 F4:1 B4:0.5 A4:0.5 F4:1 | E4:4")
# the low note of each bar (the koto's open strings), bar by bar
ROOTS = ["A2", "A2", "A2", "F2", "E2", "E2", "A2", "F2", "E2", "E2", "A2", "A2", "F2", "E2"]
FIFTH = {"A2": "E3", "F2": "C3", "E2": "B2"}
BAR = 4


def build():
    s = Score(66, BAR)
    koto = s.track("koto", 107, gain=1.0, pan=-0.15, ring=2.2, swing=0.012, rev=0.4)
    koto2 = s.track("koto bass", 107, gain=0.55, pan=0.15, ring=2.5, swing=0.012, rev=0.4)
    shaku = s.track("shakuhachi", 77, gain=0.9, pan=0.1, vib=(18, 4.5, 0.5), legato=0.97, swing=0.02)
    pad = s.track("strings", 49, gain=0.22, rev=0.5, legato=1.0)
    chime = s.track("chimes", 124, bank=5, gain=0.15, rev=0.5)

    def koto_bars(at, busy):
        for i, r in enumerate(ROOTS):
            a = at + i * BAR
            koto2.play(f"{r}+{FIFTH[r]}:1", a, 66)
            if busy:
                hi = transpose(r, 12)
                koto2.play(f"r:1.5 {hi}:0.5? {FIFTH[r]}:1 {hi}:1?", a, 54)
            else:
                koto2.play(f"r:2 {transpose(r, 12)}:2?", a, 50)

    def strings(at, vel=44):
        for i, r in enumerate(ROOTS):
            for n in (transpose(r, 12), transpose(FIFTH[r], 12)):
                pad.play(f"{n}:{BAR}", at + i * BAR, vel)

    def tremolo(at, mel):
        """the koto player keeps a long note alive by striking it again (a light re-pluck)"""
        t = at
        for tok in mel.split():
            if tok == "|":
                continue
            name, _, d = tok.partition(":")
            d = float(d)
            if d >= 2:
                for j in range(1, int(d / 0.5)):
                    koto.add(t + j * 0.5, 0.5, midi(name), 44 - j * 2)
            t += d

    t = 0.0
    # intro: the koto tunes its strings in the scale, a wind chime
    chime.add(0, 3, 72, 60)
    koto.play("E4:0.25 F4 A4 B4 C5 E5 F5 A5:2.25 | A5:0.25 F5 E5 C5 B4 A4 F4 E4:2.25", t, 64)
    koto2.play("E2+B2:4 | A2+E3:4", t, 60)
    t += 2 * BAR
    # I: the koto alone, an octave up
    koto.play(transpose(SAKURA, 12), t, 76)
    tremolo(t, transpose(SAKURA, 12))
    koto_bars(t, busy=True)
    strings(t, vel=40)
    t += len(ROOTS) * BAR
    # II: the shakuhachi sings it, the koto runs under it, the strings come in
    shaku.play("B5:g " + transpose(SAKURA, 12), t, 82)
    koto_bars(t, busy=True)
    strings(t)
    for i in range(0, len(ROOTS), 2):
        koto.play("r:2 E5:0.25? F5 A5 B5 C6:1.5?", t + i * BAR + BAR, 50)
    t += len(ROOTS) * BAR
    # interlude: the koto plays with the tune's first line
    koto.play("A5:0.5 A5 B5:1 A5:0.5 A5 B5:1 | A5:0.25 B5 C6 B5 A5:0.5 F5 E5:2 | "
              "E5:0.5 F5 A5 B5 C6:1 B5:0.5 A5 | B5:1 A5:0.5 F5 E5:2", t, 70)
    for i, r in enumerate(ROOTS[:4]):
        koto2.play(f"{r}+{FIFTH[r]}:2 {transpose(r, 12)}:2?", t + i * BAR, 60)
        pad.play(f"{transpose(r, 12)}+{transpose(FIFTH[r], 12)}:{BAR}", t + i * BAR, 40)
    chime.add(t, 3, 72, 50)
    t += 4 * BAR
    # III: both, the koto in octaves, the strings, slowing at the end
    shaku.play(transpose(SAKURA, 12), t, 86)
    koto.play(SAKURA, t, 70)
    koto.play(transpose(SAKURA, 12), t, 64)
    koto_bars(t, busy=True)
    strings(t)
    t += len(ROOTS) * BAR
    s.tempo_at(t - 3 * BAR, 66)
    s.tempo_at(t, 50)
    # the last chord: the koto sweeps down to E
    koto.play("E6:0.2 C6 B5 A5 F5 E5 C5 B4 A4 F4 E4:3", t, 60)
    koto2.play("E2+B2+E3:4", t + 1.8, 64)
    pad.play("E3+B3:5", t, 40)
    chime.add(t + 1, 3, 72, 45)
    return s
