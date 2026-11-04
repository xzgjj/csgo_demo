"""geom 公共件：盒子契约、材质色表、墙体切分与拱门基础盒。

被 geom_blocks（建筑主体）与 geom_attach（附属结构）共用；本身不产生依赖环。
坐标与契约说明见 geom.py 顶部文档。
"""

from __future__ import annotations

STEP_RISE_MAX = 32.0
JUMP_MAX = 54.0
BRIDGE_THICKNESS = 16.0

MATERIAL_COLORS = {
    "concrete": [0.58, 0.48, 0.36],
    "metal": [0.52, 0.54, 0.58],
    "wood": [0.42, 0.29, 0.17],
    "glass": [0.70, 0.82, 0.88],
    "fabric": [0.45, 0.40, 0.32],
    "dirt": [0.42, 0.34, 0.24],
    "site": [0.62, 0.22, 0.18],
    "spawn": [0.20, 0.38, 0.62],
}


def make_box(
    bid, kind, material, x0, x1, y0, y1, z0, z1, *, walkable=False, collision=True
):
    """按 min/max 建盒子（自动排序），返回派生引擎的标准盒子字典。"""
    x0, x1 = sorted((x0, x1))
    y0, y1 = sorted((y0, y1))
    z0, z1 = sorted((z0, z1))
    return {
        "id": bid,
        "kind": kind,
        "center": [(x0 + x1) / 2.0, (y0 + y1) / 2.0, (z0 + z1) / 2.0],
        "size": [x1 - x0, y1 - y0, z1 - z0],
        "bottom": z0,
        "top": z1,
        "material": material,
        "color": MATERIAL_COLORS.get(material, MATERIAL_COLORS["concrete"]),
        "collision": collision,
        "walkable": walkable,
    }


def _wall_seg(prefix, name, wall_rect, axis, a0, a1, z0, z1, material, kind="wall"):
    """单段墙盒（可空）：长轴 a0..a1、竖直 z0..z1。"""
    if a1 - a0 <= 0.0 or z1 - z0 <= 0.0:
        return None
    x0, x1, y0, y1 = wall_rect
    if axis == "x":
        return make_box(f"{prefix}_{name}", kind, material, a0, a1, y0, y1, z0, z1)
    return make_box(f"{prefix}_{name}", kind, material, x0, x1, a0, a1, z0, z1)


def _opening_boxes(prefix, wall_rect, axis, index, opening, height, material):
    """一个洞口的切分：窗下槛 + 过梁（门洞只有过梁）。"""
    x0, x1, y0, y1 = wall_rect
    lo, hi = (x0, x1) if axis == "x" else (y0, y1)
    mid = (lo + hi) / 2.0
    a0 = max(lo, mid + opening["offset"] - opening["width"] / 2.0)
    a1 = min(hi, mid + opening["offset"] + opening["width"] / 2.0)
    if a1 <= a0:
        return []
    sill = float(opening.get("sill", 0.0))
    opening_h = float(opening.get("height", height))
    top = min(
        sill + opening_h if opening.get("kind") == "window" else opening_h, height
    )
    boxes = []
    if sill > 0.0:
        boxes.append(
            _wall_seg(
                prefix, f"sill{index}", wall_rect, axis, a0, a1, 0.0, sill, material
            )
        )
    boxes.append(
        _wall_seg(
            prefix,
            f"lintel{index}",
            wall_rect,
            axis,
            a0,
            a1,
            top,
            height,
            material,
            kind="lintel",
        )
    )
    return [box for box in boxes if box is not None], a0, a1


def _wall_segments(prefix, wall_rect, axis, height, material, openings):
    """沿墙长轴切分：实体段 + 门/窗洞（下槛/过梁），返回盒子列表。"""
    x0, x1, y0, y1 = wall_rect
    lo, hi = (x0, x1) if axis == "x" else (y0, y1)
    cuts = []
    for index, opening in enumerate(openings):
        part = _opening_boxes(prefix, wall_rect, axis, index, opening, height, material)
        if not part:
            continue
        cut, a0, a1 = part
        cuts.append((a0, a1, cut))
    cuts.sort(key=lambda item: item[0])
    boxes = []
    cursor = lo
    for a0, a1, cut in cuts:
        if a0 > cursor:
            boxes.append(
                _wall_seg(
                    prefix, "solid", wall_rect, axis, cursor, a0, 0.0, height, material
                )
            )
        boxes.extend(cut)
        cursor = max(cursor, a1)
    if cursor < hi:
        boxes.append(
            _wall_seg(
                prefix, "solid_end", wall_rect, axis, cursor, hi, 0.0, height, material
            )
        )
    return [box for box in boxes if box is not None]


def rect_walls(prefix, center, size, thickness, height, material, openings, sides):
    """矩形墙（院墙/房间壳）：N/S 墙占满角，E/W 墙内缩避免角部重叠。"""
    cx, cy = center
    sx, sy = size
    x0, x1 = cx - sx / 2.0, cx + sx / 2.0
    y0, y1 = cy - sy / 2.0, cy + sy / 2.0
    walls = {
        "N": ((x0, x1, y1 - thickness, y1), "x"),
        "S": ((x0, x1, y0, y0 + thickness), "x"),
        "E": ((x1 - thickness, x1, y0 + thickness, y1 - thickness), "y"),
        "W": ((x0, x0 + thickness, y0 + thickness, y1 - thickness), "y"),
    }
    boxes = []
    for side in sides:
        if side not in walls:
            continue
        rect, axis = walls[side]
        side_ops = [op for op in openings if op.get("wall") == side]
        boxes += _wall_segments(
            f"{prefix}_{side}", rect, axis, height, material, side_ops
        )
    return boxes


def _arch(building, material):
    """拱门砌体：两侧墩 + 顶部过梁（门洞高度 passage_height）。"""
    cx, cy = building["center"]
    sx, sy = building["size"]
    height = float(building["height"])
    pw = float(building["passage_width"])
    ph = float(building["passage_height"])
    y0, y1 = cy - sy / 2, cy + sy / 2
    boxes = [
        make_box(
            f"{building['id']}_pier_w",
            "wall",
            material,
            cx - sx / 2,
            cx - pw / 2,
            y0,
            y1,
            0.0,
            height,
        ),
        make_box(
            f"{building['id']}_pier_e",
            "wall",
            material,
            cx + pw / 2,
            cx + sx / 2,
            y0,
            y1,
            0.0,
            height,
        ),
        make_box(
            f"{building['id']}_lintel",
            "lintel",
            material,
            cx - pw / 2,
            cx + pw / 2,
            y0,
            y1,
            ph,
            height,
        ),
    ]
    return boxes
