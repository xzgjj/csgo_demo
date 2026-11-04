"""de_ash 几何派生引擎：layout 参数 → AABB 盒子（sim 坐标，唯一几何事实源）。

所有消费端（Godot 渲染 / Blender GLB / 内核 MapDef / 检查器）都只消费这里的输出，
几何算法只写一次，避免三端漂移。

盒子契约：
  {"id", "kind", "center": [x, y, z]（盒子中心）,
   "size": [sx, sy, sz], "bottom", "top",
   "material": 6 类材质 id, "collision": bool, "walkable": bool}

坐标系：sim x 东 / y 北 / z 上；Godot/Blender 坐标由 gen_blockout.py 统一转换。
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


def _arch(building, material):
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


def _bridge(building, material):
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
