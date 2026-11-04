"""de_ash 布局校验器（mapcheck）：结构/高度层/可达/净空/对线/派生一致性。

用法：python -X utf8 tools/mapgen/check_layout.py
退出码：0 全过；1 有问题（问题列表打印在最后一行之前）。
"""

from __future__ import annotations

import json
import sys

import check_sightlines
import gen_blockout
import geom
from paths import BOXES, LAYOUT

RUN_SPEED = 250.0
ROT_TOLERANCE = 0.35
EPS = 1.0


def rect(box):
    cx, cy, _ = box["center"]
    sx, sy, _ = box["size"]
    return (cx - sx / 2, cx + sx / 2, cy - sy / 2, cy + sy / 2)


def xy_gap(a, b):
    ax0, ax1, ay0, ay1 = rect(a)
    bx0, bx1, by0, by1 = rect(b)
    gx = max(0.0, max(ax0, bx0) - min(ax1, bx1))
    gy = max(0.0, max(ay0, by0) - min(ay1, by1))
    return (gx * gx + gy * gy) ** 0.5


def covers(box, x, y, margin=0.0):
    x0, x1, y0, y1 = rect(box)
    return x0 - margin <= x <= x1 + margin and y0 - margin <= y <= y1 + margin


def volume_overlap(a, b):
    ax0, ax1, ay0, ay1 = rect(a)
    bx0, bx1, by0, by1 = rect(b)
    dx = min(ax1, bx1) - max(ax0, bx0)
    dy = min(ay1, by1) - max(ay0, by0)
    dz = min(a["top"], b["top"]) - max(a["bottom"], b["bottom"])
    return min(dx, dy, dz)


def check_meta(layout):
    issues = []
    if layout.get("schema") != "map-layout/2":
        issues.append(f"schema 应为 map-layout/2，实际 {layout.get('schema')}")
    if len(set(layout.get("materials", []))) != 6:
        issues.append("材质数量应为 6")
    if layout.get("floor_z") != 0:
        issues.append("floor_z 应为 0")
    return issues


def _opening_issues(building, issues):
    for opening in building.get("openings", []):
        width = float(opening.get("width", 0))
        height = float(opening.get("height", 0))
        if width < 96:
            issues.append(f"门洞过窄 {building['id']}/{opening['id']}: {width}")
        if opening.get("kind") == "door" and height < 128:
            issues.append(f"门洞过低 {building['id']}/{opening['id']}: {height}")
        if opening.get("kind") == "window" and float(opening.get("sill", 0)) != 96:
            issues.append(f"窗台高度应为 96: {building['id']}/{opening['id']}")
    return issues


def _building_shape_issues(building, boxes, max_h, issues):
    if building["kind"] not in {"solid", "ring", "shell", "arch", "bridge"}:
        issues.append(f"未知建筑类型 {building['kind']}: {building['id']}")
    height = float(building.get("height", building.get("top_z", 0.0)))
    if height > max_h:
        issues.append(f"建筑超高 {building['id']}: {height}")
    if building.get("roof_walkable", False):
        issues.append(f"屋顶不应可走: {building['id']}")
    prefix = building["id"]
    if not any(
        box["id"] == prefix or box["id"].startswith(prefix + "_") for box in boxes
    ):
        issues.append(f"建筑未派生盒子: {building['id']}")
    return issues


def check_buildings(layout, boxes):
    issues = []
    buildings = layout.get("buildings", [])
    if len(buildings) != 14:
        issues.append(f"建筑数量应为 14，实际 {len(buildings)}")
    ids = [building["id"] for building in buildings]
    if len(set(ids)) != len(ids):
        issues.append("建筑 id 重复")
    max_h = layout["reach"]["max_building_height"]
    for building in buildings:
        issues = _building_shape_issues(building, boxes, max_h, issues)
        issues = _opening_issues(building, issues)
    return issues


def check_walkable(boxes, issues):
    surfaces = geom.walkable_surfaces(boxes)
    tops = sorted({round(s[0], 3) for s in surfaces})
    for top in tops:
        if top >= 256:
            issues.append(f"出现 >=256 的可走面: {top}")
    if 0.0 not in tops:
        issues.append("缺少 L0 地面可走面")
    if 128.0 not in tops:
        issues.append("缺少 L2(+128) 可走面")
    return surfaces, issues


def check_reachability(surfaces, issues):
    reachable = [True] + [False] * (len(surfaces) - 1)
    changed = True
    while changed:
        changed = False
        for i, (zi, xi0, xi1, yi0, yi1) in enumerate(surfaces):
            if not reachable[i]:
                continue
            for j, (zj, xj0, xj1, yj0, yj1) in enumerate(surfaces):
                if reachable[j] or zj <= zi or zj - zi > geom.JUMP_MAX:
                    continue
                gx = max(0.0, max(xi0, xj0) - min(xi1, xj1))
                gy = max(0.0, max(yi0, yj0) - min(yi1, yj1))
                if (gx * gx + gy * gy) ** 0.5 <= 64.0:
                    reachable[j] = True
                    changed = True
    for i, (z, x0, x1, y0, y1) in enumerate(surfaces):
        if not reachable[i]:
            issues.append(
                f"可走面不可达 z={z:.0f} 中心=({(x0 + x1) / 2:.0f},{(y0 + y1) / 2:.0f})"
            )
    return reachable, issues


def check_floating(boxes, issues):
    for box in boxes:
        if not box["collision"] or box["bottom"] <= EPS:
            continue
        supported = False
        for other in boxes:
            if other is box or other["top"] < box["bottom"] - EPS:
                continue
            if other["bottom"] <= box["bottom"] + EPS and xy_gap(box, other) <= 2.0:
                supported = True
                break
        if not supported:
            issues.append(f"悬浮盒子 {box['id']}: bottom={box['bottom']:.0f}")
    return issues


def check_overlap(boxes, issues):
    solid = [box for box in boxes if box["collision"]]
    for i in range(len(solid)):
        for j in range(i + 1, len(solid)):
            if volume_overlap(solid[i], solid[j]) > 1.0:
                issues.append(f"盒体穿插 {solid[i]['id']} <-> {solid[j]['id']}")
    return issues


def _lane_samples(lane, trim=120.0):
    """沿折线采样（去掉距离端点 trim 的样本，避免门洞/墙体误报）。"""
    points = lane["points"]
    total = geom.lane_length(points)
    samples = []
    travelled = 0.0
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        seg = ((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5
        if seg <= 0.0:
            continue
        steps = max(1, int(seg // 48))
        for k in range(steps + 1):
            dist = travelled + seg * k / steps
            if trim <= dist <= total - trim:
                samples.append((x0 + (x1 - x0) * k / steps, y0 + (y1 - y0) * k / steps))
        travelled += seg
    return samples


def _lane_meta_issues(lane, lo, hi, issues):
    if float(lane["width"]) < 96:
        issues.append(f"对线过窄 {lane['id']}: {lane['width']}")
    if len(lane["points"]) < 2:
        issues.append(f"对线点不足 {lane['id']}")
    for x, y in lane["points"]:
        if not (lo[0] <= x <= hi[0] and lo[1] <= y <= hi[1]):
            issues.append(f"对线越界 {lane['id']}: ({x},{y})")
    return issues


def _lane_corridor_issues(lane, boxes, issues):
    for x, y in _lane_samples(lane):
        ceiling = 1e9
        for box in boxes:
            if box["collision"] and box["bottom"] > EPS and covers(box, x, y):
                ceiling = min(ceiling, box["bottom"])
        if ceiling < 96.0:
            issues.append(f"净空不足 {lane['id']} @({x:.0f},{y:.0f}) = {ceiling:.0f}")
        for box in boxes:
            if (
                box["collision"]
                and box["bottom"] <= EPS
                and box["top"] > geom.JUMP_MAX
                and covers(box, x, y, margin=8.0)
            ):
                issues.append(
                    f"对线被挡 {lane['id']} @({x:.0f},{y:.0f}) by {box['id']}"
                )
    return issues


def check_lanes(layout, boxes, issues):
    lanes = layout.get("lanes", [])
    if len(lanes) < 8:
        issues.append(f"对线数量应 >= 8，实际 {len(lanes)}")
    computed = {}
    lo, hi = layout["bounds"]["min"], layout["bounds"]["max"]
    for lane in lanes:
        issues = _lane_meta_issues(lane, lo, hi, issues)
        computed[lane["id"]] = round(geom.lane_length(lane["points"]) / RUN_SPEED, 1)
        issues = _lane_corridor_issues(lane, boxes, issues)
    return computed, issues


def check_rotation(layout, computed, issues):
    targets = layout.get("rotation_seconds_target", {})
    for lane_id, seconds in computed.items():
        target = targets.get(lane_id)
        if target is None:
            issues.append(f"缺少时长目标 {lane_id}（实测 {seconds}s）")
        elif abs(float(target) - seconds) > ROT_TOLERANCE:
            issues.append(f"时长漂移 {lane_id}: 目标 {target}s 实测 {seconds}s")
    return computed, issues


def _spawn_clear_issues(layout, boxes, issues):
    for team, spawn in layout["spawns"].items():
        cx, cy = spawn["center"][0], spawn["center"][1]
        probe = {
            "center": [cx, cy, 36.0],
            "size": [32, 32, 72],
            "bottom": 0.0,
            "top": 72.0,
        }
        for box in boxes:
            if (
                box["collision"]
                and box["top"] > EPS
                and volume_overlap(probe, box) > 0.0
            ):
                issues.append(f"出生点卡进实体: {team} by {box['id']}")
    return issues


def _site_clear_issues(layout, boxes, issues):
    for site in layout["bombsites"]:
        cx, cy = site["center"][0], site["center"][1]
        if any(
            box["collision"] and box["top"] > EPS and covers(box, cx, cy)
            for box in boxes
        ):
            issues.append(f"包点中心被实体占据: {site['id']}")
    return issues


def _zone_bounds_issues(layout, issues):
    lo, hi = layout["bounds"]["min"], layout["bounds"]["max"]
    for zone in layout.get("buy_zones", []):
        x, y = zone["center"][0], zone["center"][1]
        if not (lo[0] <= x <= hi[0] and lo[1] <= y <= hi[1]):
            issues.append(f"购买区越界: {zone['team']}")
    return issues


def check_spawns(layout, boxes, issues):
    issues = _spawn_clear_issues(layout, boxes, issues)
    issues = _site_clear_issues(layout, boxes, issues)
    return _zone_bounds_issues(layout, issues)


def check_boxes_sync(layout, issues):
    if not BOXES.exists():
        issues.append(f"缺少 {BOXES.name}，先跑 tools/mapgen/gen_blockout.py")
        return issues
    data = json.loads(BOXES.read_text(encoding="utf-8"))
    derived = gen_blockout.build(layout)["boxes"]
    actual = data.get("boxes", [])
    if len(actual) != len(derived):
        issues.append(f"boxes 不同步：文件 {len(actual)} 个，派生 {len(derived)} 个")
        return issues
    for index, (got, want) in enumerate(zip(actual, derived)):
        for key in ("center", "size"):
            if any(
                abs(float(a) - float(b)) > 0.01
                for a, b in zip(got.get(key, []), want[key])
            ):
                issues.append(
                    f"boxes 不同步 #{index} {key}: {got.get(key)} != {want[key]}"
                )
                return issues
        if got.get("kind") != want["kind"] or got.get("material") != want["material"]:
            issues.append(f"boxes 不同步 #{index} kind/material")
            return issues
    return issues


def main() -> int:
    try:
        layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"mapcheck FAIL 读取布局失败: {exc}")
        return 1
    boxes = geom.derive(layout)
    issues = []
    issues += check_meta(layout)
    issues += check_buildings(layout, boxes)
    surfaces, issues = check_walkable(boxes, issues)
    _, issues = check_reachability(surfaces, issues)
    issues = check_floating(boxes, issues)
    issues = check_overlap(boxes, issues)
    computed, issues = check_lanes(layout, boxes, issues)
    computed, issues = check_rotation(layout, computed, issues)
    issues = check_spawns(layout, boxes, issues)
    issues += check_sightlines.check(layout, boxes)
    issues = check_boxes_sync(layout, issues)
    if issues:
        for issue in issues[:40]:
            print(" -", issue)
        print(f"mapcheck FAIL issues={len(issues)}")
        return 1
    print(
        f"mapcheck PASS name={layout['name']} buildings={len(layout['buildings'])} "
        f"lanes={len(layout['lanes'])} boxes={len(boxes)} "
        f"walkable={len(surfaces) - 1} rot={computed.get('CT_rotate_A', 0)}s"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
