"""从 de_ash 布局 JSON 生成 blockout 盒子清单（Godot 坐标，供 Godot 脚本建场景）。

用法：python -X utf8 tools/mapgen/gen_blockout.py
输出：host/scenes/de_ash_boxes.json
"""

from __future__ import annotations

import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAYOUT = ROOT / "content/maps/de_ash.layout.json"
OUT = ROOT / "host/scenes/de_ash_boxes.json"
T = 32.0  # 墙厚


def sim_to_godot(x: float, y: float, z: float) -> list[float]:
    return [-y, z, -x]


def box(cx, cy, cz, sx, sy, sz, color, kind, rot_y=0.0):
    return {
        "center": sim_to_godot(cx, cy, cz + sz / 2.0),
        "size": [sy, sz, sx],
        "rot_y": rot_y,
        "color": color,
        "kind": kind,
    }


def main() -> None:
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    lo = layout["bounds"]["min"]
    hi = layout["bounds"]["max"]
    w = hi[0] - lo[0]
    d = hi[1] - lo[1]
    boxes = [
        box(0, 0, -8, w, d, 16, [0.30, 0.31, 0.33], "floor"),
        box(lo[0] - T / 2, 0, 0, T, d, layout["wall_height"], [0.42, 0.40, 0.36], "wall"),
        box(hi[0] + T / 2, 0, 0, T, d, layout["wall_height"], [0.42, 0.40, 0.36], "wall"),
        box(0, lo[1] - T / 2, 0, w, T, layout["wall_height"], [0.42, 0.40, 0.36], "wall"),
        box(0, hi[1] + T / 2, 0, w, T, layout["wall_height"], [0.42, 0.40, 0.36], "wall"),
    ]
    for lane in layout["lanes"]:
        pts = lane["points"]
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            length = math.hypot(x1 - x0, y1 - y0)
            if length < 1:
                continue
            theta = math.atan2(y1 - y0, x1 - x0)
            boxes.append(box((x0 + x1) / 2, (y0 + y1) / 2, 0, length, lane["width"], 8,
                             [0.36, 0.37, 0.39], "lane", theta + math.pi / 2))
    for site in layout["bombsites"]:
        sx, sy = site["size"]
        boxes.append(box(site["center"][0], site["center"][1], 8, sx, sy, 4,
                         [0.75, 0.25, 0.20], "site"))
    for spawn in layout["spawns"].values():
        sx, sy = spawn["size"]
        boxes.append(box(spawn["center"][0], spawn["center"][1], 12, sx, sy, 4,
                         [0.20, 0.45, 0.75], "spawn"))
    for prop in layout["props"]:
        sx, sy, sz = prop["size"]
        color = [0.50, 0.36, 0.22] if "crate" in prop["type"] else [0.45, 0.45, 0.48]
        boxes.append(box(prop["center"][0], prop["center"][1], prop["center"][2], sx, sy, sz,
                         color, "prop"))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps({"name": layout["name"], "boxes": boxes}, ensure_ascii=False, indent=1),
                   encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)} boxes={len(boxes)}")


if __name__ == "__main__":
    main()
