"""de_ash 关键视线（LOS）校验：8 条设计视线不被误挡 / 关键墙体确实遮挡。

用法：python -X utf8 tools/mapgen/check_sightlines.py
数据：content/maps/de_ash.layout.json 的 `sightlines`（from/to/expect）。
实现：对 geom.derive 的碰撞盒做 AABB 射线求交（视线高度 64），不依赖渲染。
"""

from __future__ import annotations

import json
import sys

import geom
from paths import LAYOUT

EYE_Z = 64.0


def ray_hits_box(origin, direction, box) -> bool:
    x0, x1, y0, y1 = _rect(box)
    z0, z1 = box["bottom"], box["top"]
    lo = (x0, y0, z0)
    hi = (x1, y1, z1)
    o = (origin[0], origin[1], origin[2])
    d = (direction[0], direction[1], direction[2])
    t_min, t_max = 0.0, 1.0
    for axis in range(3):
        if abs(d[axis]) < 1.0e-9:
            if o[axis] < lo[axis] or o[axis] > hi[axis]:
                return False
            continue
        inv = 1.0 / d[axis]
        t0 = (lo[axis] - o[axis]) * inv
        t1 = (hi[axis] - o[axis]) * inv
        if t0 > t1:
            t0, t1 = t1, t0
        t_min = max(t_min, t0)
        t_max = min(t_max, t1)
        if t_min > t_max:
            return False
    return True


def _rect(box):
    cx, cy, _ = box["center"]
    sx, sy, _ = box["size"]
    return (cx - sx / 2, cx + sx / 2, cy - sy / 2, cy + sy / 2)


def blocked(layout, boxes, sightline) -> str | None:
    start = (sightline["from"][0], sightline["from"][1], sightline.get("eye_z", EYE_Z))
    end = (sightline["to"][0], sightline["to"][1], sightline.get("eye_z", EYE_Z))
    direction = (end[0] - start[0], end[1] - start[1], end[2] - start[2])
    for box in boxes:
        if not box["collision"]:
            continue
        if ray_hits_box(start, direction, box):
            return box["id"]
    return None


def check(layout, boxes) -> list[str]:
    issues = []
    sightlines = layout.get("sightlines", [])
    if len(sightlines) < 8:
        issues.append(f"视线数量应 >= 8，实际 {len(sightlines)}")
    for sightline in sightlines:
        hit = blocked(layout, boxes, sightline)
        expect = sightline.get("expect", "clear")
        if expect == "clear" and hit is not None:
            issues.append(f"视线被误挡 {sightline['id']} by {hit}")
        elif expect == "blocked" and hit is None:
            issues.append(f"应被遮挡却通视 {sightline['id']}")
    return issues


def main() -> int:
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    boxes = geom.derive(layout)
    issues = check(layout, boxes)
    if issues:
        for issue in issues:
            print(" -", issue)
        print(f"sightlines FAIL issues={len(issues)}")
        return 1
    print(f"sightlines PASS count={len(layout['sightlines'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
