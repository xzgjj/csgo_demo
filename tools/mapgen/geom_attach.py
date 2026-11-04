"""geom 附属结构：天桥（桥面 + 柱）、平台、坡道/楼梯、建筑 attachments 分派，
以及构筑物 / 道具 / 包点出生点标记的盒子生成。

公共件（make_box / 拱门）见 geom_common.py；建筑主体见 geom_blocks.py。
"""

from __future__ import annotations

from geom_common import BRIDGE_THICKNESS, STEP_RISE_MAX, _arch, make_box


def _bridge(building, material):
    """天桥/猫道：桥面板 + 若干立柱。"""
    cx, cy = building["center"]
    sx, sy = building["size"]
    top = float(building["top_z"])
    boxes = [
        make_box(
            f"{building['id']}_deck",
            "catwalk",
            building.get("deck_material", material),
            cx - sx / 2,
            cx + sx / 2,
            cy - sy / 2,
            cy + sy / 2,
            top - BRIDGE_THICKNESS,
            top,
            walkable=True,
        )
    ]
    col = float(building.get("column_size", 32.0))
    for index, (px, py) in enumerate(building.get("columns", [])):
        boxes.append(
            make_box(
                f"{building['id']}_col{index}",
                "column",
                "concrete",
                px - col / 2,
                px + col / 2,
                py - col / 2,
                py + col / 2,
                0.0,
                top - BRIDGE_THICKNESS,
            )
        )
    return boxes


def _platform(attach, material):
    aid = attach["id"]
    cx, cy = attach["center"]
    sx, sy = attach["size"]
    top = float(attach["top_z"])
    base = float(attach.get("base_z", 0.0))
    return [
        make_box(
            aid,
            "platform",
            attach.get("material", material),
            cx - sx / 2,
            cx + sx / 2,
            cy - sy / 2,
            cy + sy / 2,
            base,
            top,
            walkable=True,
        )
    ]


def _stepped(attach, rise):
    """坡道/楼梯：沿 axis+direction 生成 N 级实体台阶（每级 rise 高），顶面可走。"""
    aid = attach["id"]
    sx, sy = attach["start"]
    length = float(attach["length"])
    width = float(attach["width"])
    steps = max(1, int(round((float(attach["z1"]) - float(attach["z0"])) / rise)))
    dz = (float(attach["z1"]) - float(attach["z0"])) / steps
    step_len = length / steps
    axis = attach["axis"]
    sign = 1.0 if float(attach.get("dir", 1)) >= 0 else -1.0
    material = attach.get("material", "concrete")
    boxes = []
    for i in range(steps):
        a0 = i * step_len
        a1 = (i + 1) * step_len
        top = float(attach["z0"]) + dz * (i + 1)
        if axis == "y":
            y_a, y_b = sy + sign * a0, sy + sign * a1
            boxes.append(
                make_box(
                    f"{aid}_step{i}",
                    attach["kind"],
                    material,
                    sx - width / 2,
                    sx + width / 2,
                    y_a,
                    y_b,
                    0.0,
                    top,
                    walkable=True,
                )
            )
        else:
            x_a, x_b = sx + sign * a0, sx + sign * a1
            boxes.append(
                make_box(
                    f"{aid}_step{i}",
                    attach["kind"],
                    material,
                    x_a,
                    x_b,
                    sy - width / 2,
                    sy + width / 2,
                    0.0,
                    top,
                    walkable=True,
                )
            )
    return boxes


def _attachments(building, material):
    boxes = []
    for attach in building.get("attachments", []):
        kind = attach["kind"]
        if kind == "platform":
            boxes += _platform(attach, material)
        elif kind == "ramp":
            boxes += _stepped(attach, 16.0)
        elif kind == "stairs":
            boxes += _stepped(attach, STEP_RISE_MAX)
        elif kind == "catwalk":
            boxes += _bridge(attach, attach.get("material", material))
        elif kind == "arch":
            boxes += _arch(attach, attach.get("material", material))
        else:
            raise ValueError(f"未知 attachment.kind: {kind} ({building['id']})")
    return boxes


def structure_boxes(structure):
    cx, cy = structure["center"]
    sx, sy = structure["size"]
    h = float(structure["height"])
    return [
        make_box(
            structure["id"],
            structure.get("kind", "wall"),
            structure.get("material", "concrete"),
            cx - sx / 2,
            cx + sx / 2,
            cy - sy / 2,
            cy + sy / 2,
            0.0,
            h,
        )
    ]


def prop_boxes(prop):
    cx, cy = prop["center"][0], prop["center"][1]
    sx, sy, sz = prop["size"]
    z0 = float(prop["center"][2]) if len(prop["center"]) > 2 else 0.0
    z0 += float(prop.get("base_z", 0.0))
    return [
        make_box(
            prop["id"],
            prop.get("kind", "prop"),
            prop.get("material", "wood"),
            cx - sx / 2,
            cx + sx / 2,
            cy - sy / 2,
            cy + sy / 2,
            z0,
            z0 + sz,
            walkable=bool(prop.get("walkable", False)),
            collision=bool(prop.get("collision", True)),
        )
    ]


def marker_boxes(layout):
    """包点/出生点标记（仅表现/锚点，不参与碰撞）。"""
    boxes = []
    for site in layout["bombsites"]:
        cx, cy = site["center"][0], site["center"][1]
        half = 128.0
        boxes.append(
            make_box(
                f"marker_site_{site['id']}",
                "site",
                "site",
                cx - half,
                cx + half,
                cy - half,
                cy + half,
                0.0,
                3.0,
                collision=False,
            )
        )
    for team, spawn in layout["spawns"].items():
        cx, cy = spawn["center"][0], spawn["center"][1]
        half = 64.0
        boxes.append(
            make_box(
                f"marker_spawn_{team}",
                "spawn",
                "spawn",
                cx - half,
                cx + half,
                cy - half,
                cy + half,
                0.0,
                3.0,
                collision=False,
            )
        )
    return boxes
