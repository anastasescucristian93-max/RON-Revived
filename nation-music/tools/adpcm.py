"""Microsoft ADPCM WAV (format tag 2): a quarter of the size of 16-bit PCM.

The game's sound library (xasl.dll, on XAudio2) plays ADPCM WAVs when a block holds 32, 64,
128, 256 or 512 samples per channel ("Read an ADPCM WAVE file with an invalid sample per block
count! This must be one of 32, 64, 128, 256, or 512 due to XAudio2 limitations"). ffmpeg's
encoder only makes power-of-two block sizes in bytes, never those, so the encoder is here.

    python adpcm.py in.wav out.wav [samples per block, default 512]

Every block starts fresh (its predictor, step and first two samples in the header), so all blocks
are encoded at once with numpy, one sample position at a time. For each block and channel all 7
standard predictors and a few starting steps are tried and the one with the least error is kept.
"""
import struct
import sys

import numpy as np
from scipy.io import wavfile

COEF = np.array([(256, 0), (512, -256), (0, 0), (192, 64), (240, 0), (460, -208), (392, -232)], np.int64)
ADAPT = np.array([230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230], np.int64)
STEP_SCALES = (0.5, 1.0, 2.0)  # starting step = scale x the block's mean prediction error / 4


def _predict(s1, c1, s2, c2):
    """(s1 * c1 + s2 * c2) / 256 rounded toward zero, as Microsoft's decoder does in C"""
    a = s1 * c1 + s2 * c2
    return np.where(a < 0, -((-a) >> 8), a >> 8)


def _encode_blocks(x, spb):
    """x: (blocks, spb) int16 of one channel -> header fields and nibbles (blocks, spb - 2)"""
    nb = x.shape[0]
    x = x.astype(np.int64)
    trials = [(p, s) for p in range(7) for s in STEP_SCALES]
    t = len(trials)
    c1 = np.array([COEF[p, 0] for p, _ in trials])[None, :]
    c2 = np.array([COEF[p, 1] for p, _ in trials])[None, :]
    # a starting step from how well each predictor guesses this block
    ideal = (x[:, 2:, None] - ((x[:, 1:-1, None] * c1[:, None, :] + x[:, :-2, None] * c2[:, None, :]) >> 8))
    mean_err = np.abs(ideal[:, :16, :]).mean(1)
    scale = np.array([s for _, s in trials])[None, :]
    delta = np.maximum(16, (mean_err * scale / 4).astype(np.int64))
    delta0 = delta.copy()
    s1 = np.repeat(x[:, 1:2], t, 1)
    s2 = np.repeat(x[:, 0:1], t, 1)
    nib = np.zeros((nb, t, spb - 2), np.int64)
    err2 = np.zeros((nb, t))
    for i in range(2, spb):
        pred = _predict(s1, c1, s2, c2)
        target = x[:, i:i + 1]
        n = np.clip(np.round((target - pred) / delta), -8, 7).astype(np.int64)
        new = np.clip(pred + n * delta, -32768, 32767)
        err2 += (new - target) ** 2
        nib[:, :, i - 2] = n & 0xF
        delta = np.maximum(16, (ADAPT[n & 0xF] * delta) >> 8)
        s2, s1 = s1, new
    best = np.argmin(err2, 1)
    rows = np.arange(nb)
    pred_idx = np.array([p for p, _ in trials])[best]
    return pred_idx, delta0[rows, best], x[:, 1], x[:, 0], nib[rows, best]


def encode(pcm, spb=512):
    """pcm: (samples, channels) int16 -> (fmt chunk bytes, data bytes, block align)"""
    if pcm.ndim == 1:
        pcm = pcm[:, None]
    ch = pcm.shape[1]
    nb = -(-len(pcm) // spb)
    pad = np.zeros((nb * spb, ch), np.int16)
    pad[:len(pcm)] = pcm
    blocks = pad.reshape(nb, spb, ch)
    parts = [_encode_blocks(blocks[:, :, c], spb) for c in range(ch)]
    align = ch * (spb // 2 + 6)
    out = np.zeros((nb, align), np.uint8)
    # header: predictor indexes, then the steps, then sample 1, then sample 0, channel by channel
    for c, (pidx, d0, smp1, smp2, _) in enumerate(parts):
        out[:, c] = pidx
        for k, field in enumerate((d0, smp1, smp2)):
            v = np.asarray(field, np.int64).astype(np.int16).view(np.uint8).reshape(nb, 2)
            o = ch + k * 2 * ch + 2 * c
            out[:, o:o + 2] = v
    # the nibbles: high nibble first; for stereo left (high) and right (low) in each byte
    nibs = np.stack([p[4] for p in parts], 2).reshape(nb, -1)
    out[:, 7 * ch:] = (nibs[:, 0::2] << 4 | nibs[:, 1::2]).astype(np.uint8)
    return align, out.tobytes()


def write(path, pcm, sr, spb=512):
    ch = 1 if pcm.ndim == 1 else pcm.shape[1]
    align, data = encode(pcm, spb)
    extra = struct.pack("<HH", spb, 7) + b"".join(struct.pack("<hh", a, b) for a, b in COEF)
    fmt = struct.pack("<HHIIHHH", 2, ch, sr, sr * align // spb, align, 4, len(extra)) + extra
    body = b"WAVE" + b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(data)) + data
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", len(body)) + body)


def decode(path):
    """Our own decoder, to measure what the encoding loses: -> (sr, int16 (samples, channels))"""
    b = open(path, "rb").read()
    i = b.index(b"fmt ")
    tag, ch, sr, _, align, _, _, spb = struct.unpack_from("<HHIIHHHH", b, i + 8)
    assert tag == 2
    j = b.index(b"data")
    n = struct.unpack_from("<I", b, j + 4)[0]
    blocks = np.frombuffer(b[j + 8:j + 8 + n], np.uint8).reshape(-1, align)
    nb = len(blocks)
    hdr = blocks[:, :7 * ch]
    pidx = hdr[:, :ch].astype(np.int64)
    field = lambda k: hdr[:, ch + k * 2 * ch:ch + (k + 1) * 2 * ch].copy().view(np.int16).astype(np.int64)
    delta, s1, s2 = field(0), field(1), field(2)
    raw = blocks[:, 7 * ch:]
    nibs = np.stack([raw >> 4, raw & 0xF], 2).reshape(nb, -1).astype(np.int64)
    nibs = nibs.reshape(nb, spb - 2, ch)
    signed = np.where(nibs >= 8, nibs - 16, nibs)
    c1, c2 = COEF[pidx, 0], COEF[pidx, 1]
    out = np.zeros((nb, spb, ch), np.int64)
    out[:, 0], out[:, 1] = s2, s1
    for i in range(spb - 2):
        pred = _predict(s1, c1, s2, c2)
        new = np.clip(pred + signed[:, i] * delta, -32768, 32767)
        out[:, i + 2] = new
        delta = np.maximum(16, (ADAPT[nibs[:, i]] * delta) >> 8)
        s2, s1 = s1, new
    return sr, out.reshape(-1, ch).astype(np.int16)


def main(src, dst, spb=512):
    sr, pcm = wavfile.read(src)
    write(dst, pcm, sr, spb)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], *(int(a) for a in sys.argv[3:4]))
