"""从 de_ash.layout.json 经 geom 派生 Godot 盒子清单（唯一派生，禁止手改输出）。

用法：python -X utf8 tools/mapgen/gen_blockout.py
输出：host/scenes/de_ash_boxes.json（Godot 坐标：x_g=-y_sim, y_g=z_sim, z_g=-x_sim）
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import geom

ROOT = Path(__file__).resolve().parents[2]
LAYOUT = ROOT / "content/maps/de_ash.layout.json"
OUT = ROOT / "host/scenes/de_ash_boxes.json"


def to_godot(box):
    cx, cy, cz = box["center"]
    sx, sy, sz = box["size"]
    return {
        "center": [-cy, cz, -cx],
        "size": [sy, sz, sx],
        "color": box["color"],
        "kind": box["kind"],
        "material": box["material"],
        "collision": box["collision"],
    }


def build(layout):
    boxes = geom.derive(layout)
    return {
        "name": layout["name"],
        "schema": "de-ash-boxes/2",
        "source": "tools/mapgen/gen_blockout.py",
        "count": len(boxes),
        "boxes": [to_godot(box) for box in boxes],
    }


def main() -> int:
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    data = build(layout)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")
    kinds = {}
    for box in data["boxes"]:
        kinds[box["kind"]] = kinds.get(box["kind"], 0) + 1
    print(f"wrote {OUT.relative_to(ROOT)} boxes={data['count']} kinds={kinds}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
