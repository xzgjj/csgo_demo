"""geom 建筑主体：实心楼（solid）/ 院墙（ring）/ 房间壳（shell）/ 建筑分派。

附属结构（平台/坡道/楼梯/天桥/柱）与道具、标记见 geom_attach.py；
公共件（make_box / 墙体切分 / 拱门）见 geom_common.py。
"""

from __future__ import annotations

from geom_attach import _attachments, _bridge
from geom_common import _arch, make_box, rect_walls


def _solid(building, material):
    cx, cy = building["center"]
    sx, sy = building["size"]
    height = float(building["height"])
    base = float(building.get("base_z", 0.0))
    return [
        make_box(
            building["id"],
            building["kind"],
            material,
            cx - sx / 2,
            cx + sx / 2,
            cy - sy / 2,
            cy + sy / 2,
            base,
            base + height,
        )
    ]


def _ring(building, material):
    thickness = float(building.get("wall_thickness", 32.0))
    height = float(building["height"])
    return rect_walls(
        building["id"],
        building["center"],
        building["size"],
        thickness,
        height,
        material,
        building.get("openings", []),
        ("N", "S", "E", "W"),
    )


def _shell(building, material):
    thickness = float(building.get("wall_thickness", 32.0))
    height = float(building["height"])
    roof_t = 32.0
    sides = tuple(building.get("sides", ("N", "S", "E", "W")))
    boxes = rect_walls(
        building["id"],
        building["center"],
        building["size"],
        thickness,
        height - roof_t,
        material,
        building.get("openings", []),
        sides,
    )
    cx, cy = building["center"]
    sx, sy = building["size"]
    boxes.append(
        make_box(
            f"{building['id']}_roof",
            "roof",
            building.get("roof_material", material),
            cx - sx / 2,
            cx + sx / 2,
            cy - sy / 2,
            cy + sy / 2,
            height - roof_t,
            height,
        )
    )
    return boxes


def building_boxes(building):
    material = building.get("material", "concrete")
    kind = building["kind"]
    if kind == "solid":
        boxes = _solid(building, material)
    elif kind == "ring":
        boxes = _ring(building, material)
    elif kind == "shell":
        boxes = _shell(building, material)
    elif kind == "arch":
        boxes = _arch(building, material)
    elif kind == "bridge":
        boxes = _bridge(building, material)
    else:
        raise ValueError(f"未知建筑类型: {kind} ({building['id']})")
    return boxes + _attachments(building, material)
