"""Cuts a picture's background with the BRIA RMBG model installed in the local ComfyUI, and writes
the soft matte (white = object) as a PNG of the picture's size. The colour keying of
make_sprite.py / make_sprite_white.py cut buildings ragged; this matte follows the real outline.

Run it with ComfyUI's own Python (it has onnxruntime):

    C:\\AI\\ComfyUI-Easy-Install\\ComfyUI-Easy-Install\\python_embeded\\python.exe rmbg_matte.py <picture> <matte.png>

then give the matte to make_sprite_white.py --matte.
"""
import sys

import numpy as np
import onnxruntime as ort
from PIL import Image

MODEL = r"C:\AI\ComfyUI-Easy-Install\ComfyUI-Easy-Install\ComfyUI\models\rembg\models\bria-rmbg\bria-rmbg.onnx"
SIZE = 1024
MEAN, STD = np.array([0.485, 0.456, 0.406]), np.array([0.229, 0.224, 0.225])


def main(src, out):
    pic = Image.open(src).convert("RGB")
    x = np.asarray(pic.resize((SIZE, SIZE), Image.BICUBIC), np.float32) / 255
    x = ((x - MEAN) / STD).transpose(2, 0, 1)[None].astype(np.float32)
    so = ort.SessionOptions()
    so.log_severity_level = 3
    sess = ort.InferenceSession(MODEL, so, providers=["CPUExecutionProvider"])
    a = sess.run(None, {sess.get_inputs()[0].name: x})[0][0, 0]
    if a.min() < 0 or a.max() > 1:  # logits
        a = 1 / (1 + np.exp(-a))
    m = Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8)).resize(pic.size, Image.BICUBIC)
    m.save(out)
    print(f"{out}: {pic.size}, object {np.mean(np.asarray(m) >= 128):.1%} of the picture")


if __name__ == "__main__":
    main(*sys.argv[1:3])
