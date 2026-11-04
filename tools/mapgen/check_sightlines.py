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


## 布局未给 slots 时的回退：横排 5 位（与 core/src/round.cpp kSpawnOffsets 同步）。
SPAWN_OFFSETS = [(-128, 0), (-64, 0), (0, 0), (64, 0), (128, 0)]
SPAWN_SLOTS_PER_TEAM = 5


def spawn_slots(layout) -> dict[str, list[tuple[float, float]]]:
    """每队 5 个出生位（slot 0-4）。布局给了 slots 就用它，否则按 center+固定偏移回退。"""
    teams = layout.get("spawns", {})
    slots: dict[str, list[tuple[float, float]]] = {}
    for team in ("T", "CT"):
        spawn = teams.get(team, {})
        points = spawn.get("slots")
        if points:
            slots[team] = [(float(p[0]), float(p[1])) for p in points if len(p) >= 2]
        else:
            cx, cy = spawn.get("center", [0, 0])[:2]
            slots[team] = [(cx + ox, cy + oy) for ox, oy in SPAWN_OFFSETS]
    return slots


def spawn_pair_count(layout) -> int:
    """出生位配对总数（应为 5 × 5 = 25）。"""
    slots = spawn_slots(layout)
    return len(slots.get("T", [])) * len(slots.get("CT", []))


def _pair_clear(boxes, start, end) -> bool:
    direction = (end[0] - start[0], end[1] - start[1], end[2] - start[2])
    for box in boxes:
        if box["collision"] and ray_hits_box(start, direction, box):
            return False
    return True


def spawn_safety(layout, boxes) -> list[str]:
    """5 出生位 × 5 出生位 = 25 组配对，视线高度上必须互相遮挡（禁止出生对穿）。

    这是"出生位可见性配对"，不是玩家/队伍结构：队伍仍是 5v5。
    """
    teams = layout.get("spawns", {})
    if "T" not in teams or "CT" not in teams:
        return ["缺少 T/CT 出生点"]
    slots = spawn_slots(layout)
    issues = []
    for team in ("T", "CT"):
        if len(slots[team]) != SPAWN_SLOTS_PER_TEAM:
            issues.append(f"{team} 出生位应为 {SPAWN_SLOTS_PER_TEAM} 个，实际 {len(slots[team])}")
    for t_slot, (tx, ty) in enumerate(slots["T"]):
        for ct_slot, (cx, cy) in enumerate(slots["CT"]):
            start = (tx, ty, EYE_Z)
            end = (cx, cy, EYE_Z)
            if _pair_clear(boxes, start, end):
                issues.append(f"出生位配对对穿 T#slot{t_slot}({tx},{ty}) -> "
                              f"CT#slot{ct_slot}({cx},{cy})")
    return issues


def spawn_pairs_blocked(layout, boxes) -> int:
    """被遮挡的出生位配对数（全遮挡时应等于 spawn_pair_count）。"""
    slots = spawn_slots(layout)
    blocked = 0
    for tx, ty in slots["T"]:
        for cx, cy in slots["CT"]:
            if not _pair_clear(boxes, (tx, ty, EYE_Z), (cx, cy, EYE_Z)):
                blocked += 1
    return blocked



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
    issues += spawn_safety(layout, boxes)
    return issues


def _selftest(layout, boxes) -> int:
    """负向自检：拿掉所有遮挡物后 25 组出生位配对必须全部变成对穿（证明检查不是空转）。"""
    total = spawn_pair_count(layout)
    issues = spawn_safety(layout, [])
    ok = total == 25 and len(issues) == total
    print(f"selftest spawn_pairs={total} no_cover_issues={len(issues)} "
          f"{'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


def main(argv=None) -> int:
    args = sys.argv[1:] if argv is None else argv
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    boxes = geom.derive(layout)
    if "--selftest" in args:
        return _selftest(layout, boxes)
    issues = check(layout, boxes)
    if issues:
        for issue in issues:
            print(" -", issue)
        print(f"sightlines FAIL issues={len(issues)}")
        return 1
    print(f"sightlines PASS count={len(layout['sightlines'])} "
          f"spawn_pairs={spawn_pair_count(layout)} "
          f"blocked={spawn_pairs_blocked(layout, boxes)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
