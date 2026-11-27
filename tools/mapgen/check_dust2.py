"""de_dust2 灰盒自检（不套用 de_ash 的 18 建筑 / ±128 层约束）。

用法：python -X utf8 tools/mapgen/check_dust2.py
检查：schema / 边界 / 材质 / 盒数容量 / 穿插 / 悬浮 / 出生与包点净空 /
      对线越界与阻挡 / 围合外壳 / 栅格连通性 / 时长。
退出码 0 = 全过。
"""

from __future__ import annotations

import json
import sys

import geom
import geom_reach
from paths import ROOT

LAYOUT = ROOT / "content/maps/de_dust2.layout.json"
BOXES = ROOT / "host/scripts/../scenes/de_dust2_boxes.json"
CAPACITY = 512
TOLERANCE = 0.35
LEVEL_KINDS = {"platform", "stairs", "catwalk", "floor"}
SHELL_NAMES = ("shell_west", "shell_east", "shell_south", "shell_north")


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


def _check_schema(layout, boxes, issues: list[str]) -> None:
    if layout.get("schema") != "map-layout/2":
        issues.append("schema 不是 map-layout/2")
    if len(set(layout.get("materials", []))) != 6:
        issues.append(f"材质应为 6 类，实际 {len(set(layout.get('materials', [])))}")
    lo, hi = layout["bounds"]["min"], layout["bounds"]["max"]
    for box in boxes:
        x0, x1, y0, y1 = rect(box)
        if x0 < lo[0] - 64 or x1 > hi[0] + 64 or y0 < lo[1] - 64 or y1 > hi[1] + 64:
            issues.append(f"盒越界 {box['id']} ({x0:.0f},{y0:.0f})-({x1:.0f},{y1:.0f})")


def _check_overlaps(solid, issues: list[str]) -> None:
    for i in range(len(solid)):
        for j in range(i + 1, len(solid)):
            depth = overlap(solid[i], solid[j])
            if depth <= 1.0:
                continue
            kinds = {solid[i]["kind"], solid[j]["kind"]}
            if kinds & LEVEL_KINDS:
                continue  # 台面/台阶相互搭接属层高设计
            limit = 40.0 if kinds == {"wall"} else 48.0
            if solid[i]["id"].startswith("shell_") or solid[j]["id"].startswith("shell_"):
                limit = 96.0  # 边界墙/填充块与建筑对接
            if depth <= limit:
                continue  # 装配接触（墙角/道具贴墙）
            issues.append(f"穿插 {solid[i]['id']} <-> {solid[j]['id']} depth={depth:.0f}")


def _check_support(solid, issues: list[str]) -> None:
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


def _check_spawn_slots(team, spawn, solid, issues: list[str]) -> None:
    if len(spawn.get("slots", [])) < 5:
        issues.append(f"出生位不足 {team}")
    for index, slot in enumerate(spawn.get("slots", [])):
        probe = {"center": [slot[0], slot[1], 36.0], "size": [32, 32, 72],
                 "bottom": 0.0, "top": 72.0}
        for box in solid:
            if box["top"] > 1.0 and overlap(probe, box) > 0.0:
                issues.append(f"出生点卡实体 {team}[{index}] by {box['id']}")


def _check_site_center(site, solid, issues: list[str]) -> None:
    cx, cy, site_z = site["center"][0], site["center"][1], site["center"][2]
    for box in solid:
        near_center = abs(box["center"][0] - cx) < box["size"][0] / 2 and \
            abs(box["center"][1] - cy) < box["size"][1] / 2
        if not near_center or box["top"] <= 0:
            continue
        if box["top"] > site_z + 32 or box["top"] <= site_z:
            continue  # 高过包点或与包点齐平都允许，只有半高遮挡才算占位
        issues.append(f"包点中心被实体占据 {box['id']}")


def _check_spawns_sites(layout, solid, issues: list[str]) -> None:
    for team, spawn in layout["spawns"].items():
        _check_spawn_slots(team, spawn, solid, issues)
    for site in layout["bombsites"]:
        _check_site_center(site, solid, issues)


def _lane_blocker(lane, solid, total: float, issues: list[str]) -> None:
    pts = lane["points"]
    travelled = 0.0
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        seg = ((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5
        steps = max(1, int(seg // 48))
        for k in range(steps + 1):
            travelled += seg * k / steps
            if travelled < 260.0 or travelled > total - 260.0:
                continue
            x = x0 + (x1 - x0) * k / steps
            y = y0 + (y1 - y0) * k / steps
            for box in solid:
                if box["top"] <= 57.0 or box["kind"] in LEVEL_KINDS:
                    continue
                if abs(x - box["center"][0]) < box["size"][0] / 2 + 4 and \
                        abs(y - box["center"][1]) < box["size"][1] / 2 + 4:
                    issues.append(f"对线被高墙挡 {lane['id']} @({x:.0f},{y:.0f}) by {box['id']}")
                    return


def _check_lanes(layout, solid, lo, hi, issues: list[str]) -> dict[str, float]:
    computed: dict[str, float] = {}
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
        _lane_blocker(lane, solid, total, issues)
    return computed


def _check_shell(layout, solid, issues: list[str]) -> tuple[set, int, int, list]:
    shell_ids = [b["id"] for b in solid if b["id"].startswith("shell_")]
    if len(shell_ids) < 20:
        issues.append(f"围合外壳不足（{len(shell_ids)} 块，至少 20）")
    for name in SHELL_NAMES:
        if name not in shell_ids:
            issues.append(f"缺边界墙 {name}")
    cells, nx, ny, grid_lo = geom_reach.reachable(layout, solid)
    if not cells:
        issues.append("T 出生点被围死：栅格 BFS 无起点")
        return cells, nx, ny, grid_lo
    goals = {
        "A 包点": layout["bombsites"][0]["center"],
        "B 包点": layout["bombsites"][1]["center"],
        "CT 出生": layout["spawns"]["CT"]["center"],
        "T 买区": layout["buy_zones"][0]["center"],
    }
    for label, point in goals.items():
        if not geom_reach.cell_near(cells, grid_lo, point):
            issues.append(f"连通性缺口：T 出生走不到 {label}")
    for i, j in cells:
        cx = grid_lo[0] + geom_reach.GRID * i
        cy = grid_lo[1] + geom_reach.GRID * j
        if geom_reach.playable_clearance(cx, cy, layout) < -geom_reach.PLAY_TOLERANCE:
            issues.append(f"可达区越出可玩区 @({cx:.0f},{cy:.0f})")
            break
    return cells, nx, ny, grid_lo


def main() -> int:
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    boxes = geom.derive(layout)
    solid = [b for b in boxes if b["collision"]]
    issues: list[str] = []

    _check_schema(layout, boxes, issues)
    if len(solid) > CAPACITY:
        issues.append(f"碰撞盒 {len(solid)} 超过内核容量 {CAPACITY}")
    _check_overlaps(solid, issues)
    _check_support(solid, issues)
    _check_spawns_sites(layout, solid, issues)
    lo, hi = layout["bounds"]["min"], layout["bounds"]["max"]
    computed = _check_lanes(layout, solid, lo, hi, issues)
    cells, nx, ny, _ = _check_shell(layout, solid, issues)

    kinds: dict[str, int] = {}
    for box in boxes:
        kinds[box["kind"]] = kinds.get(box["kind"], 0) + 1
    shell_count = sum(1 for b in solid if b["id"].startswith("shell_"))
    print(f"boxes={len(boxes)} solid={len(solid)} kinds={kinds}")
    print("lanes=" + ", ".join(f"{k}:{v}s" for k, v in computed.items()))
    print(f"reach_cells={len(cells)}/{nx * ny} shell={shell_count}")
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
