"""de_ash 几何派生引擎入口：layout 参数 → AABB 盒子（sim 坐标，唯一几何事实源）。

所有消费端（Godot 渲染 / Blender GLB / 内核 MapDef / 检查器）都只消费这里的输出，
几何算法只写一次，避免三端漂移。

盒子契约：
  {"id", "kind", "center": [x, y, z]（盒子中心）,
   "size": [sx, sy, sz], "bottom", "top",
   "material": 6 类材质 id, "collision": bool, "walkable": bool}

坐标系：sim x 东 / y 北 / z 上；Godot/Blender 坐标由 gen_blockout.py 统一转换。

本文件保留派生入口（derive / walkable_surfaces / lane_length）与对外的符号出口；
实现拆分在 geom_common.py（公共件与墙体切分）、geom_blocks.py（建筑主体）、
geom_attach.py（平台/坡道/楼梯/天桥/柱/道具/标记），拆分不改变任何几何语义。
"""

from __future__ import annotations

from geom_attach import marker_boxes, prop_boxes, structure_boxes
from geom_blocks import building_boxes
from geom_common import (BRIDGE_THICKNESS, JUMP_MAX, MATERIAL_COLORS,
                         STEP_RISE_MAX, make_box, rect_walls)

__all__ = [
    "BRIDGE_THICKNESS", "JUMP_MAX", "MATERIAL_COLORS", "STEP_RISE_MAX",
    "building_boxes", "derive", "lane_length", "make_box", "marker_boxes",
    "prop_boxes", "rect_walls", "structure_boxes", "walkable_surfaces",
]


def derive(layout):
    """layout → 全部盒子（顺序确定：地板→外墙→建筑→构筑→道具→标记）。"""
    lo, hi = layout["bounds"]["min"], layout["bounds"]["max"]
    wall_t = float(layout.get("perimeter_thickness", 32.0))
    height = float(layout["wall_height"])
    boxes = [
        make_box(
            "floor",
            "floor",
            "dirt",
            lo[0] - wall_t,
            hi[0] + wall_t,
            lo[1] - wall_t,
            hi[1] + wall_t,
            -16.0,
            0.0,
        ),
        make_box(
            "perimeter_n",
            "wall",
            "concrete",
            lo[0] - wall_t,
            hi[0] + wall_t,
            hi[1],
            hi[1] + wall_t,
            0.0,
            height,
        ),
        make_box(
            "perimeter_s",
            "wall",
            "concrete",
            lo[0] - wall_t,
            hi[0] + wall_t,
            lo[1] - wall_t,
            lo[1],
            0.0,
            height,
        ),
        make_box(
            "perimeter_w",
            "wall",
            "concrete",
            lo[0] - wall_t,
            lo[0],
            lo[1],
            hi[1],
            0.0,
            height,
        ),
        make_box(
            "perimeter_e",
            "wall",
            "concrete",
            hi[0],
            hi[0] + wall_t,
            lo[1],
            hi[1],
            0.0,
            height,
        ),
    ]
    for building in layout["buildings"]:
        boxes += building_boxes(building)
    for structure in layout.get("structures", []):
        boxes += structure_boxes(structure)
    for prop in layout.get("props", []):
        boxes += prop_boxes(prop)
    boxes += marker_boxes(layout)
    return boxes


def walkable_surfaces(boxes):
    """可站立面：地板 + walkable 盒子顶面。返回 [(top_z, x0, x1, y0, y1)]。"""
    surfaces = [(0.0, -4096.0, 4096.0, -4096.0, 4096.0)]
    for box in boxes:
        if box["walkable"] and box["collision"]:
            cx, cy, _ = box["center"]
            sx, sy, _ = box["size"]
            surfaces.append(
                (box["top"], cx - sx / 2, cx + sx / 2, cy - sy / 2, cy + sy / 2)
            )
    return surfaces


def lane_length(points):
    """折线长度（sim 单位）。"""
    total = 0.0
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        total += ((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5
    return total
