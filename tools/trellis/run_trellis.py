"""Turns one picture of a building into a textured 3D model with TRELLIS.2, through a running
ComfyUI (C:\\AI\\ComfyUI-Easy-Install, port 8188).

    python run_trellis.py <image.png> <name> [seed] [faces]

The picture should be the building alone on a transparent background. It is copied to
ComfyUI's input folder. The result is ComfyUI/output/<name>_Textured_00001_.glb (and a
_WhiteMesh one without texture). Uses the node pack's own example workflow
MeshWithTexturing.json, converted to the API format with the server's /object_info.
"""
import json
import shutil
import sys
import time
import urllib.request

COMFY = r"C:\AI\ComfyUI-Easy-Install\ComfyUI-Easy-Install\ComfyUI"
WORKFLOW = COMFY + r"\custom_nodes\ComfyUI-Trellis2\example_workflows\MeshWithTexturing.json"
URL = "http://127.0.0.1:8188"
SKIP = {"Preview3D", "Note", "MarkdownNote"}


def get(path):
    with urllib.request.urlopen(URL + path) as r:
        return json.load(r)


def post(path, data):
    req = urllib.request.Request(URL + path, json.dumps(data).encode(), {"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req) as r:
            return json.load(r)
    except urllib.error.HTTPError as e:
        sys.exit("server refused the prompt:\n" + e.read().decode()[:4000])


def to_api(workflow, info):
    """UI graph -> API prompt: links become [node, slot], widget values are matched to the
    node's widget inputs in order (an INT seed is followed by its 'control_after_generate')."""
    links = {l[0]: l for l in workflow["links"]}
    prompt = {}
    for n in workflow["nodes"]:
        if n.get("mode", 0) != 0 or n["type"] in SKIP or n["type"] not in info:
            continue
        spec = info[n["type"]]["input"]
        order = list(spec.get("required", {}).items()) + list(spec.get("optional", {}).items())
        linked = {i["name"]: links[i["link"]] for i in n.get("inputs", []) if i.get("link") is not None}
        inputs = {name: [str(l[1]), l[2]] for name, l in linked.items()}
        values = list(n.get("widgets_values") or [])
        vi = 0
        for name, s in order:
            kind = s[0]
            is_widget = isinstance(kind, list) or kind in ("INT", "FLOAT", "STRING", "BOOLEAN", "COMBO")
            if not is_widget:
                continue
            if vi >= len(values):
                break
            if name not in linked:
                inputs[name] = values[vi]
            vi += 1
            if kind == "INT" and len(s) > 1 and isinstance(s[1], dict) and s[1].get("control_after_generate"):
                vi += 1
            elif kind == "INT" and vi < len(values) and values[vi] in ("fixed", "randomize", "increment", "decrement"):
                vi += 1
        # widgets added to the node after the example was saved: take their defaults
        for name, s in spec.get("required", {}).items():
            if name not in inputs and len(s) > 1 and isinstance(s[1], dict) and "default" in s[1]:
                inputs[name] = s[1]["default"]
            elif name not in inputs and isinstance(s[0], list) and s[0]:
                inputs[name] = s[0][0]
        prompt[str(n["id"])] = {"class_type": n["type"], "inputs": inputs}
    return prompt


def main(image, name, seed=12345, faces=200000):
    shutil.copy(image, COMFY + r"\input\ron_" + name + ".png")
    info = get("/object_info")
    workflow = json.load(open(WORKFLOW, encoding="utf-8"))
    prompt = to_api(workflow, info)
    for node in prompt.values():
        t, i = node["class_type"], node["inputs"]
        if t == "Trellis2LoadImageWithTransparency":
            i["image"] = "ron_" + name + ".png"
        elif t == "PrimitiveString":
            i["value"] = name
        elif t == "PrimitiveInt" and i.get("value") == 500000:
            i["value"] = faces
        if "seed" in i and not isinstance(i["seed"], list):
            i["seed"] = seed
    res = post("/prompt", {"prompt": prompt})
    pid = res["prompt_id"]
    print("queued", pid, flush=True)
    t0 = time.time()
    while True:
        time.sleep(5)
        h = get("/history/" + pid)
        if pid in h:
            st = h[pid]["status"]
            print(st["status_str"], round(time.time() - t0), "s")
            if st["status_str"] != "success":
                print(json.dumps(st["messages"])[-3000:])
            break


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], *(int(a) for a in sys.argv[3:]))
