"""A small SoundFont 2 player in numpy: enough of the SF2 spec to play one note.

Reads presets, instruments, zones and samples; renders a note with the sample's
loop, pitch, volume envelope, pan, vibrato and the velocity low-pass filter.
Modulators other than the SF2 default velocity ones are ignored.
"""
import struct

import numpy as np
from scipy.signal import butter, lfilter

# generator numbers (SF2 2.04, section 8.1.2)
START, END, LSTART, LEND, START_C = 0, 1, 2, 3, 4
VIB_TO_PITCH, FC, Q, END_C = 6, 8, 9, 12
PAN = 17
VIB_DELAY, VIB_FREQ = 23, 24
DELAY, ATTACK, HOLD, DECAY, SUSTAIN, RELEASE = 33, 34, 35, 36, 37, 38
KEY_TO_HOLD, KEY_TO_DECAY = 39, 40
INSTRUMENT, KEY_RANGE, VEL_RANGE, LSTART_C = 41, 43, 44, 45
ATTEN, LEND_C, COARSE, FINE, SAMPLE_ID, MODES = 48, 50, 51, 52, 53, 54
SCALE_TUNING, ROOT_KEY = 56, 58

DEFAULTS = {FC: 13500, DELAY: -12000, ATTACK: -12000, HOLD: -12000,
            DECAY: -12000, RELEASE: -12000, VIB_DELAY: -12000,
            SCALE_TUNING: 100, ROOT_KEY: -1}
# generators that only make sense on an instrument zone (never added from a preset)
INST_ONLY = {0, 1, 2, 3, 4, 12, 45, 46, 47, 50, 53, 54, 57, 58}


def _signed(v):
    return v - 65536 if v >= 32768 else v


def tc(x):
    """timecents -> seconds"""
    return 2.0 ** (x / 1200.0)


class SoundFont:
    def __init__(self, path):
        data = open(path, "rb").read()
        assert data[:4] == b"RIFF" and data[8:12] == b"sfbk", "not a SoundFont"
        chunks = {}
        pos = 12
        while pos < len(data):
            cid, size = struct.unpack_from("<4sI", data, pos)
            if cid == b"LIST":
                kind = data[pos + 8:pos + 12]
                sub, end = pos + 12, pos + 8 + size
                while sub < end:
                    sid, ssize = struct.unpack_from("<4sI", data, sub)
                    chunks[(kind, sid)] = data[sub + 8:sub + 8 + ssize]
                    sub += 8 + ssize + (ssize & 1)
            pos += 8 + size + (size & 1)
        self.smpl = np.frombuffer(chunks[(b"sdta", b"smpl")], "<i2").astype(np.float32) / 32768.0

        def recs(name, fmt):
            raw = chunks[(b"pdta", name)]
            n = struct.calcsize(fmt)
            return [struct.unpack_from(fmt, raw, i) for i in range(0, len(raw), n)]

        phdr = recs(b"phdr", "<20sHHHIII")
        pbag = recs(b"pbag", "<HH")
        pgen = recs(b"pgen", "<HH")
        inst = recs(b"inst", "<20sH")
        ibag = recs(b"ibag", "<HH")
        igen = recs(b"igen", "<HH")
        self.shdr = recs(b"shdr", "<20sIIIIIBbHH")

        def zones(b0, b1, bags, gens, last_gen):
            out = []
            for b in range(b0, b1):
                g0, g1 = bags[b][0], bags[b + 1][0]
                z = {}
                for op, amt in gens[g0:g1]:
                    z[op] = amt if op in (KEY_RANGE, VEL_RANGE) else _signed(amt)
                out.append(z)
            glob = {}
            if out and last_gen not in out[0]:
                glob = out.pop(0)
            return glob, out

        self.instruments = []
        for i in range(len(inst) - 1):
            self.instruments.append(zones(inst[i][1], inst[i + 1][1], ibag, igen, SAMPLE_ID))
        self.presets = {}
        for i in range(len(phdr) - 1):
            name, prog, bank, bag = phdr[i][:4]
            self.presets[(bank, prog)] = (name.split(b"\0")[0].decode("latin1"),
                                          zones(bag, phdr[i + 1][3], pbag, pgen, INSTRUMENT))

    def name(self, bank, prog):
        return self.presets[(bank, prog)][0]

    def voices(self, bank, prog, key, vel):
        """(sample header, merged generators) for every zone the note plays"""
        def inside(z, k, v):
            kr, vr = z.get(KEY_RANGE, 0x7F00), z.get(VEL_RANGE, 0x7F00)
            return (kr & 0xFF) <= k <= (kr >> 8) and (vr & 0xFF) <= v <= (vr >> 8)

        _, (pglob, pzones) = self.presets[(bank, prog)]
        for pz in pzones:
            pz = {**pglob, **pz}
            if INSTRUMENT not in pz or not inside(pz, key, vel):
                continue
            iglob, izones = self.instruments[pz[INSTRUMENT]]
            for iz in izones:
                iz = {**iglob, **iz}
                if SAMPLE_ID not in iz or not inside(iz, key, vel):
                    continue
                g = {**DEFAULTS, **iz}
                for op, amt in pz.items():
                    if op not in INST_ONLY and op not in (INSTRUMENT, KEY_RANGE, VEL_RANGE):
                        g[op] = g.get(op, 0) + amt
                yield self.shdr[g[SAMPLE_ID]], g

    def note(self, bank, prog, key, vel, dur, sr=44100, vib=None, max_release=4.0, bend=0.0):
        """Stereo float32 array (n, 2) of one note held for dur seconds.

        vib: optional extra vibrato (cents, hz, delay s) on top of the preset's own.
        bend: fixed pitch offset in cents.
        """
        parts = []
        for sh, g in self.voices(bank, prog, key, vel):
            parts.append(self._voice(sh, g, key, vel, dur, sr, vib, max_release, bend))
        if not parts:
            return np.zeros((1, 2), np.float32)
        n = max(len(p) for p in parts)
        out = np.zeros((n, 2), np.float32)
        for p in parts:
            out[:len(p)] += p
        return out

    def _voice(self, sh, g, key, vel, dur, sr, vib, max_release, bend):
        _, s0, s1, l0, l1, srate, orig, corr, _, _ = sh
        start = s0 + g.get(START, 0) + 32768 * g.get(START_C, 0)
        end = s1 + g.get(END, 0) + 32768 * g.get(END_C, 0)
        ls = l0 + g.get(LSTART, 0) + 32768 * g.get(LSTART_C, 0)
        le = l1 + g.get(LEND, 0) + 32768 * g.get(LEND_C, 0)
        mode = g.get(MODES, 0) & 3
        looped = mode in (1, 3) and le - ls > 4
        root = g[ROOT_KEY] if g[ROOT_KEY] >= 0 else orig
        cents = (key - root) * g[SCALE_TUNING] + g.get(COARSE, 0) * 100 + g.get(FINE, 0) + corr + bend
        ratio = 2.0 ** (cents / 1200.0) * srate / sr

        release = min(max(tc(g[RELEASE]), 0.02), max_release)
        n = int((dur + release) * sr)
        if not looped:
            n = min(n, int((end - start - 2) / ratio))
        if n <= 1:
            return np.zeros((1, 2), np.float32)
        t = np.arange(n) / sr

        # pitch: preset vibrato plus our own
        steps = np.full(n, ratio)
        depth = g.get(VIB_TO_PITCH, 0)
        if depth:
            f = 8.176 * 2.0 ** (g.get(VIB_FREQ, 0) / 1200.0)
            on = t >= tc(g[VIB_DELAY])
            steps = steps * 2.0 ** (depth * np.sin(2 * np.pi * f * t) * on / 1200.0)
        if vib:
            vc, vf, vd = vib
            ramp = np.clip((t - vd) / 0.4, 0, 1)
            steps = steps * 2.0 ** (vc * ramp * np.sin(2 * np.pi * vf * t) / 1200.0)
        pos = np.concatenate(([0.0], np.cumsum(steps[:-1]))) + start
        if looped:
            over = pos >= le
            pos[over] = ls + np.mod(pos[over] - ls, le - ls)
        i = pos.astype(np.int64)
        fr = (pos - i).astype(np.float32)
        nxt = i + 1
        if looped:
            nxt[nxt >= le] = ls
        sig = self.smpl[i] * (1 - fr) + self.smpl[nxt] * fr

        # volume envelope (dB of attenuation, linear in dB like the spec)
        delay, attack = tc(g[DELAY]), tc(g[ATTACK])
        hold = tc(g[HOLD] + g.get(KEY_TO_HOLD, 0) * (60 - key))
        decay = tc(g[DECAY] + g.get(KEY_TO_DECAY, 0) * (60 - key))
        sus_db = min(max(g.get(SUSTAIN, 0), 0), 1440) / 10.0
        tt = t - delay
        att_db = np.where(tt < attack + hold, 0.0,
                          np.minimum((tt - attack - hold) / decay * 100.0, sus_db))
        lin = np.clip(tt / max(attack, 1e-4), 0, 1)
        off = t >= dur
        if off.any():
            k = np.argmax(off)
            att_db[k:] = att_db[k] + (t[k:] - dur) / release * 100.0
        env = lin * 10.0 ** (-np.minimum(att_db, 100.0) / 20.0)
        env[tt < 0] = 0.0
        sig = sig * env.astype(np.float32)

        # SF2 default modulators: velocity -> loudness (concave) and -> cutoff
        gain = 10.0 ** (-g.get(ATTEN, 0) / 200.0) * (vel / 127.0) ** 2
        fc = g[FC] - 2400.0 * (1 - vel / 127.0)
        hz = 8.176 * 2.0 ** (fc / 1200.0)
        if hz < sr * 0.45:
            b, a = butter(2, hz / (sr / 2))
            sig = lfilter(b, a, sig).astype(np.float32)

        p = min(max(g.get(PAN, 0) / 1000.0, -0.5), 0.5) + 0.5
        return np.stack([sig * np.cos(p * np.pi / 2), sig * np.sin(p * np.pi / 2)], 1) * gain
