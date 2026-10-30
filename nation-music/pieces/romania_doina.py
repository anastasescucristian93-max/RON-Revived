"""Romania, Ancient to Medieval ages, economy: a doina.

The doina is the old free-rhythm lament of the Romanian countryside: a nai (pan
flute) or a fiddle sings long held notes that fall in ornamented steps to the
tonic over a drone. Mode: D with a raised fourth (D E F G# A B C), the
"Romanian minor" of the lautari.
"""
from score import Score, chord, transpose

TITLE = "Doina"
NATION, AGES, MOOD = "Romania", (0, 1, 2), "ECONOMIC"

# (melody, chord under it); the four phrases of the lament
P1 = [("r:1 A5:3 B5:g A5:0.5 G#5:0.5 A5:0.5 B5:0.5", "Dm"),
      ("A5:2.5 G#5:0.5", "E"),
      ("F5:0.5 G#5:g F5:0.5 E5:0.5 F5:0.25 E5:0.25 D5:4", "Dm")]
P2 = [("D5:0.5 E5:0.5 F5:0.5 G#5:0.5 A5:2", "Dm"),
      ("C6:0.5 B5:0.5 A5:0.5 G#5:0.5 A5:1~B5", "E"),
      ("G#5:0.5 F5:0.5 E5:1 D5:0.5 E5:0.5 F5:2", "Dm"),
      ("E5:0.5 F5:0.5 E5:0.5 D5:0.5", "A7"),
      ("C#5:g D5:4", "Dm")]
P3 = [("A5:0.5 C6:0.5 D6:4", "Dm"),
      ("E6:g D6:0.5 C6:0.5 Bb5:1 A5:0.5 G5:0.5 A5:2", "Gm"),
      ("G#5:g F5:1 E5:0.5 F5:0.5 G#5:1 A5:1", "E"),
      ("E5:3", "A")]
P4 = [("D5:0.5 E5:0.5 F5:0.5 G#5:0.5 A5:1 G#5:0.5 F5:0.5", "Dm"),
      ("E5:0.5 F5:0.5 E5:0.5 D5:0.5 E5:1.5~F5", "A7"),
      ("D5:6", "Dm")]


def build():
    s = Score(64)
    drone = s.track("drone", 49, gain=0.18, rev=0.4, legato=1.0)
    nai = s.track("nai", 75, gain=1.0, rev=0.38, vib=(14, 5.0, 0.35), legato=0.98, swing=0.015)
    vln = s.track("vioara", 40, gain=0.9, rev=0.35, vib=(22, 5.6, 0.2), legato=0.98, swing=0.015)
    tam = s.track("tambal", 15, gain=0.42, rev=0.3, pan=-0.3, ring=1.6)
    pad = s.track("strings", 49, gain=0.3, rev=0.45, legato=1.0, pan=0.2)

    def phrase(segs, at, lead, octave=0, accomp=None, vel=78):
        t = at
        for mel, ch in segs:
            if octave:
                mel = transpose(mel, octave)
            end = lead.play(mel, t, vel)
            if accomp:
                accomp(ch, t, end)
            t = end
        # the lament breathes: slow down into the last note, then pick up again
        s.tempo_at(t - 5, 64)
        s.tempo_at(t - 3, 50)
        s.tempo_at(t + 0.5, 64)
        return t + 1

    def tambal(ch, a, b):
        notes = chord(ch, 62) + [chord(ch, 62)[0] + 12]
        t, i = a, 0
        while t < b - 0.01:
            tam.add(t, 0.25, notes[[0, 1, 2, 3, 2, 1][i % 6]], 46 + (10 if i % 6 == 0 else 0))
            t, i = t + 0.25, i + 1

    def strings(ch, a, b):
        for k in chord(ch, 55):
            pad.add(a, b - a, k, 52)

    t = 2.0
    start = t
    # I: the nai alone over the drone
    t = phrase(P1, t, nai)
    t = phrase(P2, t, nai)
    # II: the fiddle takes it up, the tambal under it
    t = phrase(P1, t, vln, octave=-12, accomp=tambal, vel=74)
    t = phrase(P2, t, vln, accomp=tambal, vel=76)
    # III: the cry, both together, the strings come in
    a = t
    t = phrase(P3, a, nai, accomp=lambda c, x, y: (tambal(c, x, y), strings(c, x, y)), vel=86)
    phrase(P3, a, vln, octave=-12, vel=70)
    t = phrase(P4, t, nai, accomp=strings, vel=72)
    for k in (50, 57):  # D3 + A3 under everything
        drone.add(start - 2, t - start + 2, k, 54)
    return s
