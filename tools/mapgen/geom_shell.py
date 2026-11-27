"""de_dust2 围合外壳：由 layout 派生「边界墙 + 空白象限填充块」。

目的：不改手工台账（scene-elements.csv 仍 75 行），用确定性算法把可玩区封起来——
外圈四条边界墙挡住出界，空白象限用大块墙体填掉，避免玩家从建筑缝隙走进虚空。

约定（与 import_scene_v3 / check_dust2 共用）：
- 只用轴对齐盒：格子填充，不做斜墙；
- 可玩走廊 = lane 折线按 width/2 + 96 外扩；包点/出生/买区再外扩 128；
- 已有结构 64 内不填，保证不压建筑；
- 墙高 288（高于 256 的上限层，保证不可跳越）。

用法：import_scene_v3.main() 里 `layout["structures"] += shell.build(layout)`。
"""

from __future__ import annotations

import math

CELL = 384              # 填充格边长（世界单位）
GAP = 8                 # 相邻填充块留缝：小于玩家直径 32，走不过去
CORRIDOR_MARGIN = 48    # 走廊外扩：玩家半径 + 贴墙走位
ZONE_MARGIN = 96       # 包点/出生/买区外扩
BUILDING_MARGIN = 32    # 已有结构外扩，不压建筑
SHELL_THICKNESS = 64    # 边界墙厚度
SHELL_HEIGHT = 288      # 边界/填充墙高


def _rect_of_structure(item: dict) -> tuple[float, float, float, float]:
    cx, cy = item["center"][0], item["center"][1]
    sx, sy = item["size"][0], item["size"][1]
    return cx - sx / 2, cy - sy / 2, cx + sx / 2, cy + sy / 2


def _rects_overlap(a, b, margin: float) -> bool:
    return not (
        a[2] + margin <= b[0] or b[2] + margin <= a[0]
        or a[3] + margin <= b[1] or b[3] + margin <= a[1]
    )


def _point_segment_distance(px: float, py: float, ax: float, ay: float,
                            bx: float, by: float) -> float:
    dx, dy = bx - ax, by - ay
    length_sq = dx * dx + dy * dy
    if length_sq <= 1e-9:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length_sq))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def _rect_corridor_distance(rect, lane) -> float:
    """矩形到 lane 折线的最近距离（用角点 + 中心近似，格子里够用）。"""
    x0, y0, x1, y1 = rect
    samples = [(x0, y0), (x1, y1), (x0, y1), (x1, y0),
               ((x0 + x1) / 2, (y0 + y1) / 2)]
    best = float("inf")
    for px, py in samples:
        for (ax, ay), (bx, by) in zip(lane["points"], lane["points"][1:]):
            best = min(best, _point_segment_distance(px, py, ax, ay, bx, by))
    return best


RIB_STEP = 128.0        # 走廊墙取样间距
RIB_SIZE = 128.0        # 单块走廊墙边长（阶梯化逼近斜向边）
RIB_HALF = 24.0         # 墙体半厚，中心在走廊边界外 24
RIB_END_GAP = 160.0     # 段两端留口，作为路口/门洞


def _inside_other_corridor(px: float, py: float, lanes, skip_lane: str) -> bool:
    for lane in lanes:
        if lane["id"] == skip_lane:
            continue
        half = lane["width"] / 2.0
        for (ax, ay), (bx, by) in zip(lane["points"], lane["points"][1:]):
            if _point_segment_distance(px, py, ax, ay, bx, by) < half + RIB_HALF:
                return True
    return False


def _inside_zone(px: float, py: float, layout: dict) -> bool:
    rects = []
    for site in layout.get("bombsites", []):
        cx, cy, _ = site["center"]
        sx, sy = site["size"]
        rects.append((cx - sx / 2, cy - sy / 2, cx + sx / 2, cy + sy / 2))
    for spawn in layout.get("spawns", {}).values():
        cx, cy, _ = spawn["center"]
        sx, sy = spawn["size"]
        rects.append((cx - sx / 2, cy - sy / 2, cx + sx / 2, cy + sy / 2))
    for zone in layout.get("buy_zones", []):
        cx, cy, _ = zone["center"]
        sx, sy = zone["size"]
        rects.append((cx - sx / 2, cy - sy / 2, cx + sx / 2, cy + sy / 2))
    for rect in rects:
        if rect[0] <= px <= rect[2] and rect[1] <= py <= rect[3]:
            return True
    return False


def corridor_ribbon(layout: dict) -> list[dict]:
    """沿每条 lane 的两侧铺阶梯化墙块，围出走廊；段端留口当路口。"""
    chokes = [(c["center"][0], c["center"][1], c.get("width", 96) / 2 + 64)
              for c in layout.get("choke_points", [])]
    out: list[dict] = []
    for lane in layout.get("lanes", []):
        half = lane["width"] / 2.0
        for seg_index, ((ax, ay), (bx, by)) in enumerate(
                zip(lane["points"], lane["points"][1:])):
            seg_len = math.hypot(bx - ax, by - ay)
            if seg_len < RIB_END_GAP * 2 + RIB_STEP:
                continue
            ux, uy = (bx - ax) / seg_len, (by - ay) / seg_len
            nx, ny = -uy, ux
            for side in (-1.0, 1.0):
                offset = half + RIB_HALF
                travelled = RIB_END_GAP
                while travelled <= seg_len - RIB_END_GAP:
                    px = ax + ux * travelled + nx * offset * side
                    py = ay + uy * travelled + ny * offset * side
                    travelled += RIB_STEP
                    if any((px - cx) ** 2 + (py - cy) ** 2 < limit ** 2
                           for cx, cy, limit in chokes):
                        continue
                    if _inside_other_corridor(px, py, layout.get("lanes", []), lane["id"]):
                        continue
                    if _inside_zone(px, py, layout):
                        continue
                    out.append({
                        "id": "shell_rib_%s_%d_%d_%d" % (
                            lane["id"], seg_index, int(side), int(travelled)),
                        "kind": "wall",
                        "center": [px, py],
                        "size": [RIB_SIZE, RIB_SIZE],
                        "height": SHELL_HEIGHT,
                        "base_z": 0.0,
                        "material": "concrete",
                    })
    return out


def perimeter(layout: dict) -> list[dict]:
    """四条边界墙：内表面与 bounds 对齐，板厚 64、高 288。"""
    lo, hi = layout["bounds"]["min"], layout["bounds"]["max"]
    thickness = SHELL_THICKNESS
    out: list[dict] = []
    spans = [
        ("shell_west", (lo[0], lo[1], lo[0] + thickness, hi[1])),
        ("shell_east", (hi[0] - thickness, lo[1], hi[0], hi[1])),
        ("shell_south", (lo[0], lo[1], hi[0], lo[1] + thickness)),
        ("shell_north", (lo[0], hi[1] - thickness, hi[0], hi[1])),
    ]
    for name, (x0, y0, x1, y1) in spans:
        out.append({
            "id": name,
            "kind": "wall",
            "center": [(x0 + x1) / 2.0, (y0 + y1) / 2.0],
            "size": [x1 - x0, y1 - y0],
            "height": SHELL_HEIGHT,
            "base_z": 0.0,
            "material": "concrete",
        })
    return out


def _expand(rect, margin: float) -> tuple[float, float, float, float]:
    return (rect[0] - margin, rect[1] - margin, rect[2] + margin, rect[3] + margin)


def fill_blocks(layout: dict) -> list[dict]:
    """把不被走廊/包点/出生/建筑覆盖的空格填成实心块。"""
    lo, hi = layout["bounds"]["min"], layout["bounds"]["max"]
    zone_rects: list[tuple[float, float, float, float]] = []
    for site in layout.get("bombsites", []):
        cx, cy, _ = site["center"]
        sx, sy = site["size"]
        zone_rects.append(_expand((cx - sx / 2, cy - sy / 2, cx + sx / 2, cy + sy / 2),
                                  ZONE_MARGIN))
    for spawn in layout.get("spawns", {}).values():
        cx, cy, _ = spawn["center"]
        sx, sy = spawn["size"]
        zone_rects.append(_expand((cx - sx / 2, cy - sy / 2, cx + sx / 2, cy + sy / 2),
                                  ZONE_MARGIN))
    for zone in layout.get("buy_zones", []):
        cx, cy, _ = zone["center"]
        sx, sy = zone["size"]
        zone_rects.append(_expand((cx - sx / 2, cy - sy / 2, cx + sx / 2, cy + sy / 2),
                                  ZONE_MARGIN))
    building_rects: list[tuple[float, float, float, float]] = []
    for item in layout.get("structures", []):
        building_rects.append(_expand(_rect_of_structure(item), BUILDING_MARGIN))
    for item in layout.get("props", []):
        building_rects.append(_expand(_rect_of_structure(item), BUILDING_MARGIN))

    lanes = layout.get("lanes", [])
    inset = SHELL_THICKNESS + GAP
    blocks: list[dict] = []
    x = lo[0] + inset
    while x + CELL <= hi[0] - inset:
        y = lo[1] + inset
        while y + CELL <= hi[1] - inset:
            rect = (x, y, x + CELL, y + CELL)
            shrink = (x + GAP / 2, y + GAP / 2, x + CELL - GAP / 2, y + CELL - GAP / 2)
            corridor = min(
                (_rect_corridor_distance(rect, lane) - lane["width"] / 2.0
                 for lane in lanes),
                default=float("inf"),
            )
            blocked = corridor < CORRIDOR_MARGIN
            if not blocked:
                blocked = any(_rects_overlap(rect, keep, 0.0)
                              for keep in zone_rects + building_rects)
            if not blocked:
                blocks.append({
                    "id": "shell_fill_%d_%d" % (int(x), int(y)),
                    "kind": "wall",
                    "center": [(shrink[0] + shrink[2]) / 2.0, (shrink[1] + shrink[3]) / 2.0],
                    "size": [shrink[2] - shrink[0], shrink[3] - shrink[1]],
                    "height": SHELL_HEIGHT,
                    "base_z": 0.0,
                    "material": "concrete",
                })
            y += CELL
        x += CELL
    return blocks


def build(layout: dict) -> list[dict]:
    """返回按 id 排序的外壳结构，供 import_scene_v3 追加到 layout["structures"]。"""
    items = perimeter(layout) + corridor_ribbon(layout) + fill_blocks(layout)
    return sorted(items, key=lambda item: item["id"])


def is_shell(item: dict) -> bool:
    """判断结构是否由本模块派生（命名前缀 shell_）。"""
    return str(item.get("id", "")).startswith("shell_")

