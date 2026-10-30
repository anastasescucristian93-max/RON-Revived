"""Scores as text, rendered with sf2.py to the game's WAV format (and to MIDI).

Notation, one token per note, spaces between, '|' is only a bar line for the eye:
  A5:1.5   pitch and length in beats (a/b allowed: E5:1/3); the length carries on
  r:2      rest
  A3+E4:2  chord
  A5:2!    accent (louder), A5:2? soft
  B5:g     grace note: takes a short slice from the start of the next note
  A5:2~B5  trill between A5 and B5 for the whole length, ending on A5
"""
import os
import random
import re

import mido
import numpy as np
from scipy.signal import butter, fftconvolve, lfilter
from scipy.ndimage import maximum_filter1d

import adpcm
from sf2 import SoundFont

SR = 44100
HERE = os.path.dirname(os.path.abspath(__file__))
_sf = None
NAMES = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def soundfont():
    global _sf
    if _sf is None:
        _sf = SoundFont(os.path.join(HERE, "soundfont", "GeneralUser-GS.sf2"))
    return _sf


def midi(name):
    m = re.fullmatch(r"([A-G])(#|b)?(-?\d)", name)
    if not m:
        raise ValueError(f"bad pitch {name!r}")
    n = NAMES[m.group(1)] + {"#": 1, "b": -1, None: 0}[m.group(2)]
    return n + 12 * (int(m.group(3)) + 1)


QUALITIES = {"": [0, 4, 7], "m": [0, 3, 7], "7": [0, 4, 7, 10], "m7": [0, 3, 7, 10],
             "5": [0, 7], "sus4": [0, 5, 7], "dim": [0, 3, 6]}


def chord(name, low):
    """Close chord, root on the first such note at or above midi key 'low'.
    name: root plus quality, e.g. Am, E7, Bb, F#m, D5 (root and fifth), Asus4."""
    m = re.fullmatch(r"([A-G][#b]?)(.*)", name)
    pc = midi(m.group(1) + "0") % 12
    root = low + (pc - low) % 12
    return [root + i for i in QUALITIES[m.group(2)]]


SHARPS = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"]


def note_name(n):
    return f"{SHARPS[n % 12]}{n // 12 - 1}"


def transpose(text, semis):
    """Move every pitch of a line of notation by semis semitones."""
    return re.sub(r"[A-G][#b]?-?\d", lambda m: note_name(midi(m.group(0)) + semis), text)


def diatonic(text, steps, scale):
    """Move every pitch by scale steps (a second voice in thirds: steps=-2).
    scale: pitch names of the key, e.g. "G A B C D E F#"."""
    pcs = {midi(n + "0") % 12 for n in scale.split()}
    keys = [k for k in range(128) if k % 12 in pcs]

    def move(m):
        n = midi(m.group(0))
        i = max(j for j, k in enumerate(keys) if k <= n)  # a note off the scale counts as the one below
        return note_name(keys[i + steps] + (n - keys[i]))

    return re.sub(r"[A-G][#b]?-?\d", move, text)


def _dur(s):
    if "/" in s:
        a, b = s.split("/")
        return float(a) / float(b)
    return float(s)


class Track:
    def __init__(self, prog, bank=0, gain=1.0, pan=0.0, rev=0.25, vib=None,
                 ring=1.0, swing=0.008, legato=0.92, drums=False):
        self.prog, self.bank, self.gain, self.pan, self.rev = prog, bank, gain, pan, rev
        self.vib, self.ring, self.swing, self.legato, self.drums = vib, ring, swing, legato, drums
        self.notes = []  # (beat, length in beats, midi key, velocity)

    def add(self, beat, length, key, vel):
        self.notes.append((beat, length, key, int(max(1, min(127, vel)))))

    def play(self, text, at, vel=80):
        """Add a line of notation starting at beat 'at'; returns the beat after it."""
        t, d, graces = at, 1.0, []
        for tok in text.split():
            if tok == "|":
                continue
            acc = 0
            while tok[-1] in "!?":
                acc += 18 if tok[-1] == "!" else -18
                tok = tok[:-1]
            trill = None
            if "~" in tok:
                tok, trill = tok.split("~")
            name, _, rest = tok.partition(":")
            if rest == "g":
                graces.append(name)
                continue
            if rest:
                d = _dur(rest)
            start, left = t, d
            for gname in graces:
                gl = min(0.12, left / 4)
                self.add(start, gl, midi(gname), vel - 10)
                start, left = start + gl, left - gl
            graces = []
            if name != "r":
                if trill:
                    a, b, step, k = midi(name), midi(trill), 0.125, 0
                    while left > step * 1.5:
                        self.add(start, step, a if k % 2 == 0 else b, vel + acc - 6)
                        start, left, k = start + step, left - step, k + 1
                    self.add(start, left, a, vel + acc)
                else:
                    for p in name.split("+"):
                        self.add(start, left, midi(p), vel + acc)
            t += d
        return t


class Score:
    def __init__(self, bpm, beats_per_bar=4):
        self.tempo = [(0.0, float(bpm))]  # (beat, bpm) points, linear between them
        self.bar = beats_per_bar
        self.tracks = {}

    def track(self, name, prog, **kw):
        self.tracks[name] = Track(prog, **kw)
        return self.tracks[name]

    def tempo_at(self, beat, bpm):
        self.tempo.append((float(beat), float(bpm)))
        self.tempo.sort()

    def bpm_at(self, beat):
        pts = np.array(self.tempo)
        return float(np.interp(beat, pts[:, 0], pts[:, 1]))

    def seconds(self, beats):
        """beats (array) -> seconds, integrating the piecewise linear tempo"""
        grid = np.arange(0, max(np.max(beats), 1) + 2, 1 / 64)
        pts = np.array(self.tempo + [(grid[-1] + 1, self.tempo[-1][1])])
        bpm = np.interp(grid, pts[:, 0], pts[:, 1])
        secs = np.concatenate(([0], np.cumsum(60.0 / bpm[:-1] / 64)))
        return np.interp(beats, grid, secs)

    def render(self, seed=1, reverb_s=2.0):
        sf = soundfont()
        rnd = random.Random(seed)
        ends = [b + l for t in self.tracks.values() for b, l, _, _ in t.notes]
        total = self.seconds(np.array([max(ends)]))[0] + reverb_s + 4
        n = int(total * SR)
        dry = np.zeros((n, 2), np.float32)
        send = np.zeros((n, 2), np.float32)
        for t in self.tracks.values():
            if not t.notes:
                continue
            bank = 128 if t.drums else t.bank
            # bring every instrument to the same level first; t.gain is then its place in the mix
            keys = sorted(k for _, _, k, _ in t.notes)
            ref = sf.note(bank, t.prog, keys[len(keys) // 2], 100, 0.6, SR)[:SR]
            level = 0.1 / max(float(np.sqrt(np.mean(ref ** 2))), 1e-4)
            starts = self.seconds(np.array([b for b, _, _, _ in t.notes]))
            stops = self.seconds(np.array([b + l * t.legato for b, l, _, _ in t.notes]))
            p = t.pan * 0.5 + 0.5
            lr = np.array([np.cos(p * np.pi / 2), np.sin(p * np.pi / 2)], np.float32) * np.sqrt(2)
            for (b, l, key, vel), s0, s1 in zip(t.notes, starts, stops):
                s0 += rnd.gauss(0, t.swing)
                v = int(max(1, min(127, vel + rnd.gauss(0, 4))))
                dur = max(0.03, (s1 - s0) * t.ring)
                a = sf.note(bank, t.prog, key, v, dur, SR, vib=t.vib) * lr * (t.gain * level)
                i = max(0, int(s0 * SR))
                a = a[:n - i]
                dry[i:i + len(a)] += a
                send[i:i + len(a)] += a * t.rev
        wet = _reverb(send, reverb_s)
        mix = dry + wet[:n]
        return _master(mix)

    def save(self, name, wav_dir, midi_dir, seed=1):
        os.makedirs(wav_dir, exist_ok=True)
        os.makedirs(midi_dir, exist_ok=True)
        mix = self.render(seed)
        # Microsoft ADPCM at 44.1 kHz: half the size of the game's 22 kHz PCM, the full treble,
        # its noise about 40 dB down (nation-music/ENGINE_NOTES.md, "What ADPCM costs")
        path = os.path.join(wav_dir, name + ".wav")
        adpcm.write(path, (np.clip(mix, -1, 1) * 32767).astype(np.int16), SR)
        self.save_midi(os.path.join(midi_dir, name + ".mid"))
        return path, len(mix) / SR

    def save_midi(self, path):
        f = mido.MidiFile(ticks_per_beat=480)
        meta = mido.MidiTrack()
        f.tracks.append(meta)
        last = 0
        for b, bpm in self.tempo:
            tick = int(b * 480)
            meta.append(mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(bpm), time=tick - last))
            last = tick
        ch = 0
        for name, t in self.tracks.items():
            c = 9 if t.drums else ch
            if not t.drums:
                ch += 1 + (ch == 8)
            tr = mido.MidiTrack()
            f.tracks.append(tr)
            tr.append(mido.MetaMessage("track_name", name=name, time=0))
            if not t.drums:
                tr.append(mido.Message("control_change", channel=c, control=0, value=t.bank, time=0))
                tr.append(mido.Message("program_change", channel=c, program=t.prog, time=0))
            ev = []
            for b, l, key, vel in t.notes:
                b = max(b, 0.0)
                ev.append((int(b * 480), 1, key, max(1, min(127, vel))))
                ev.append((int((b + l * t.legato) * 480), 0, key, 0))
            ev.sort()
            last = 0
            for tick, on, key, vel in ev:
                kind = "note_on" if on else "note_off"
                tr.append(mido.Message(kind, channel=c, note=key, velocity=vel, time=tick - last))
                last = tick
        f.save(path)


def _reverb(x, seconds):
    """A hall made of decaying noise: early reflections, then a darker tail."""
    rng = np.random.default_rng(7)
    n = int(seconds * SR)
    t = np.arange(n) / SR
    ir = rng.standard_normal((n, 2)).astype(np.float32) * np.exp(-6.9 * t / seconds)[:, None]
    b, a = butter(1, 4500 / (SR / 2))
    ir = lfilter(b, a, ir, axis=0)
    ir[: int(0.02 * SR)] = 0
    for d, g in ((0.011, 0.5), (0.019, 0.35), (0.027, 0.3), (0.041, 0.22)):
        ir[int(d * SR), 0] += g
        ir[int((d + 0.004) * SR), 1] += g
    ir /= np.sqrt((ir ** 2).sum(0))
    return np.stack([fftconvolve(x[:, 0], ir[:, 0]), fftconvolve(x[:, 1], ir[:, 1])], 1)


def _master(x, rms_db=-17.0):
    """Level like the game's tracks (about -16..-18 dB), a gentle peak limiter,
    then trim the silent tail and fade it."""
    x = x - x.mean(0)
    loud = np.abs(x).max(1)
    on = np.nonzero(loud > loud.max() * 2e-3)[0]  # the tail down to -54 dB
    x = x[: on[-1] + 1] if len(on) else x
    rms = np.sqrt(np.mean(x ** 2))
    x = x * (10 ** (rms_db / 20) / max(rms, 1e-9))
    peak = maximum_filter1d(np.abs(x).max(1), int(0.01 * SR))
    gain = np.minimum(1.0, 0.9 / np.maximum(peak, 1e-9))  # room for the ADPCM error
    b, a = butter(1, 30 / (SR / 2))
    gain = np.minimum(gain, lfilter(b, a, gain))  # smooth, never above the needed cut
    x = x * gain[:, None]
    x = x * min(1.0, 0.9 / np.abs(x).max())  # a fast attack (koto) can get past the smoothed gain
    fade = int(0.5 * SR)
    x[-fade:] *= np.linspace(1, 0, fade)[:, None]
    return x.astype(np.float32)
