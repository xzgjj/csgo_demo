"""de_dust2 灰盒自检（不套用 de_ash 的 18 建筑 / ±128 层约束）。

用法：python -X utf8 tools/mapgen/check_dust2.py
检查：schema / 边界 / 材质 / 盒数容量 / 穿插 / 悬浮 / 出生与包点净空 / 对线越界与阻挡 / 时长。
退出码 0 = 全过。
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import geom
from paths import ROOT

LAYOUT = ROOT / "content/maps/de_dust2.layout.json"
BOXES = ROOT / "host/scenes/de_dust2_boxes.json"
CAPACITY = 512
TOLERANCE = 0.35


def rect(box):
    cx, cy, _ = box["center"]
    sx, sy, _ = box["size"]
    return (cx - sx / 2, cx + sx / 2, cy - sy / 2, cy + sy / 2)


def xy_gap(a, b):
    ax0, ax1, ay0, ay1 = rect(a)
    bx0, bx1, by0, by1 = rect(b)
    return (max(0.0, max(ax0, bx0) - min(ax1, bx1)) ** 2 +
            max(0.0, max(ay0, by0) - min(ay1, by1)) ** 2) ** 0.5


def overlap(a, b):
    ax0, ax1, ay0, ay1 = rect(a)
    bx0, bx1, by0, by1 = rect(b)
    dx = min(ax1, bx1) - max(ax0, bx0)
    dy = min(ay1, by1) - max(ay0, by0)
    dz = min(a["top"], b["top"]) - max(a["bottom"], b["bottom"])
    return min(dx, dy, dz)


def main() -> int:
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    boxes = geom.derive(layout)
    issues: list[str] = []

    if layout.get("schema") != "map-layout/2":
        issues.append("schema 不是 map-layout/2")
    if len(set(layout.get("materials", []))) != 6:
        issues.append(f"材质应为 6 类，实际 {len(set(layout.get('materials', [])))}")
    lo, hi = layout["bounds"]["min"], layout["bounds"]["max"]
    for box in boxes:
        x0, x1, y0, y1 = rect(box)
        if x0 < lo[0] - 64 or x1 > hi[0] + 64 or y0 < lo[1] - 64 or y1 > hi[1] + 64:
            issues.append(f"盒越界 {box['id']} ({x0:.0f},{y0:.0f})-({x1:.0f},{y1:.0f})")
    solid = [b for b in boxes if b["collision"]]
    if len(solid) > CAPACITY:
        issues.append(f"碰撞盒 {len(solid)} 超过内核容量 {CAPACITY}")

    LEVEL_KINDS = {"platform", "stairs", "catwalk", "floor"}
    for i in range(len(solid)):
        for j in range(i + 1, len(solid)):
            depth = overlap(solid[i], solid[j])
            if depth <= 1.0:
                continue
            kinds = {solid[i]["kind"], solid[j]["kind"]}
            if kinds & LEVEL_KINDS:
                continue  # 台面/台阶相互搭接属层高设计
            limit = 40.0 if kinds == {"wall"} else 48.0
            if depth <= limit:
                continue  # 装配接触（墙角/道具贴墙）
            issues.append(f"穿插 {solid[i]['id']} <-> {solid[j]['id']} depth={depth:.0f}")

    for box in solid:
        if box["bottom"] <= 1.0 or not box["collision"]:
            continue
        supported = any(
            other is not box and other["top"] >= box["bottom"] - 1.0
            and other["bottom"] <= box["bottom"] + 1.0 and xy_gap(box, other) <= 2.0
            for other in solid
        )
        if not supported:
            issues.append(f"悬浮 {box['id']} bottom={box['bottom']:.0f}")

    for team, spawn in layout["spawns"].items():
        if len(spawn.get("slots", [])) < 5:
            issues.append(f"出生位不足 {team}")
        for index, slot in enumerate(spawn.get("slots", [])):
            probe = {"center": [slot[0], slot[1], 36.0], "size": [32, 32, 72],
                     "bottom": 0.0, "top": 72.0}
            for box in solid:
                if box["top"] > 1.0 and overlap(probe, box) > 0.0:
                    issues.append(f"出生点卡实体 {team}[{index}] by {box['id']}")
    for site in layout["bombsites"]:
        cx, cy = site["center"][0], site["center"][1]
        for box in solid:
            if box["top"] > 0 and abs(box["center"][0] - cx) < box["size"][0] / 2 and \
                    abs(box["center"][1] - cy) < box["size"][1] / 2 and box["top"] <= site["center"][2] + 32:
                if box["top"] <= site["center"][2]:
                    continue
                issues.append(f"包点中心被实体占据 {box['id']}")

    computed = {}
    for lane in layout["lanes"]:
        pts = lane["points"]
        if float(lane["width"]) < 96:
            issues.append(f"对线过窄 {lane['id']}")
        for x, y in pts:
            if not (lo[0] <= x <= hi[0] and lo[1] <= y <= hi[1]):
                issues.append(f"对线越界 {lane['id']} ({x},{y})")
        total = geom.lane_length(pts)
        computed[lane["id"]] = round(total / 250.0, 1)
        target = layout["rotation_seconds_target"].get(lane["id"])
        if target is None:
            issues.append(f"缺时长目标 {lane['id']}")
        elif abs(target - computed[lane["id"]]) > TOLERANCE:
            issues.append(f"时长漂移 {lane['id']} 目标 {target} 实测 {computed[lane['id']]}")
        travelled = 0.0
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            seg = ((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5
            steps = max(1, int(seg // 48))
            for k in range(steps + 1):
                x = x0 + (x1 - x0) * k / steps
                y = y0 + (y1 - y0) * k / steps
                travelled += seg * k / steps
                if travelled < 260.0 or travelled > total - 260.0:
                    continue
                for box in solid:
                    if box["top"] <= 57.0 or box["kind"] in LEVEL_KINDS:
                        continue
                    if abs(x - box["center"][0]) < box["size"][0] / 2 + 4 and \
                            abs(y - box["center"][1]) < box["size"][1] / 2 + 4:
                        issues.append(f"对线被高墙挡 {lane['id']} @({x:.0f},{y:.0f}) by {box['id']}")
                        break

    kinds: dict[str, int] = {}
    for box in boxes:
        kinds[box["kind"]] = kinds.get(box["kind"], 0) + 1
    print(f"boxes={len(boxes)} solid={len(solid)} kinds={kinds}")
    print("lanes=" + ", ".join(f"{k}:{v}s" for k, v in computed.items()))
    if issues:
        for issue in issues[:30]:
            print(" -", issue)
        print(f"check_dust2 FAIL issues={len(issues)}")
        return 1
    print(f"check_dust2 PASS name={layout['name']} lanes={len(layout['lanes'])} "
          f"spawn_pairs={len(layout['spawns'])} choke={len(layout['choke_points'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

