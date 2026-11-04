"""de_ash 构建数据层：路径/常量 + 盒子清单读取与结构分析（纯数据，不依赖 bpy）。

几何事实源是 ``host/scenes/de_ash_boxes.json``（Godot 坐标），本模块只做
Godot → Blender 的坐标换算与结构分类，不写任何文件。
"""

from __future__ import annotations

import json
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[3]
BOXES_PATH = PROJECT_ROOT / "host" / "scenes" / "de_ash_boxes.json"
VERSION_DIR = PROJECT_ROOT / "assets" / "source" / "de_ash" / "versions"
GLB_PATH = PROJECT_ROOT / "content" / "maps" / "de_ash.glb"
RENDER_DIR = PROJECT_ROOT / "docs" / "design" / "de_ash_renders"

RENDER_W = 1280
RENDER_H = 720
RENDER_SAMPLES = 64
PREVIEW_W = 1280
PREVIEW_H = 720
PREVIEW_SAMPLES = 16
FOV_DEG = 75.0  # 与 host/tests/capture_de_ash.gd 的 Camera3D.fov 一致（垂直 FOV）

# 6 类 PBR + 2 个高亮派生材质；参数权威表 = assets/materials/README.md
MATERIAL_SPECS = {
    "concrete": {"color": (0.58, 0.48, 0.36), "roughness": 0.95, "metallic": 0.0},
    "metal": {"color": (0.52, 0.54, 0.58), "roughness": 0.35, "metallic": 0.90},
    "wood": {"color": (0.42, 0.29, 0.17), "roughness": 0.80, "metallic": 0.0},
    "glass": {
        "color": (0.70, 0.82, 0.88), "roughness": 0.06, "metallic": 0.10,
        "alpha": 0.34,
    },
    "fabric": {"color": (0.45, 0.40, 0.32), "roughness": 1.00, "metallic": 0.0},
    "dirt": {"color": (0.42, 0.34, 0.24), "roughness": 1.00, "metallic": 0.0},
    "site": {
        "color": (0.62, 0.22, 0.18), "roughness": 0.88, "metallic": 0.0,
        "emission": (0.85, 0.18, 0.12), "emission_strength": 0.90,
    },
    "spawn": {
        "color": (0.20, 0.38, 0.62), "roughness": 0.88, "metallic": 0.0,
        "emission": (0.15, 0.40, 0.85), "emission_strength": 0.90,
    },
}

# 机位：与 host/tests/capture_de_ash.gd 的 views 一致（Godot 坐标）
#   key: (相机位置, 注视点, 是否顶点俯视)
VIEWS = {
    "overview_top": ((0.0, 3200.0, 0.0), (0.0, 0.0, 0.0), True),
    "A_site": ((1400.0, 900.0, 1900.0), (800.0, 0.0, 1150.0), False),
    "B_site": ((-1500.0, 900.0, -1900.0), (-800.0, 0.0, -1150.0), False),
    "mid": ((0.0, 300.0, 1400.0), (0.0, 20.0, -600.0), False),
    "T_spawn": ((1700.0, 220.0, 500.0), (1700.0, 20.0, 0.0), False),
    "CT_spawn": ((-1700.0, 220.0, -500.0), (-1700.0, 20.0, 0.0), False),
}
VIEW_ORDER = ("overview_top", "A_site", "B_site", "mid", "T_spawn", "CT_spawn")


def godot_to_blender_point(point):
    """Godot (x, y_up, z) → Blender (x, -z, y)。"""
    return (float(point[0]), -float(point[2]), float(point[1]))


def godot_to_blender_size(size):
    """Godot (sx, sy_up, sz) → Blender (sx, sz, sy)。"""
    return (float(size[0]), float(size[2]), float(size[1]))


def load_boxes():
    """读取唯一几何事实源，转成 Blender 空间的盒子字典列表。"""
    data = json.loads(BOXES_PATH.read_text(encoding="utf-8"))
    boxes = []
    for index, raw in enumerate(data["boxes"]):
        center = godot_to_blender_point(raw["center"])
        size = godot_to_blender_size(raw["size"])
        boxes.append({
            "index": index,
            "name": f"box_{index:03d}_{raw['kind']}",
            "center": center,
            "size": size,
            "kind": raw["kind"],
            "material": raw["material"],
            "color": tuple(raw["color"]),
            "collision": bool(raw["collision"]),
        })
    return boxes


def box_bounds(box):
    """返回 Blender 空间 (x0, y0, z0, x1, y1, z1)。"""
    cx, cy, cz = box["center"]
    sx, sy, sz = box["size"]
    return (cx - sx / 2.0, cy - sy / 2.0, cz - sz / 2.0,
            cx + sx / 2.0, cy + sy / 2.0, cz + sz / 2.0)


def thin_axis(box):
    """水平面上较薄（墙体厚度方向）的轴：0=X，1=Y。"""
    return 0 if box["size"][0] <= box["size"][1] else 1


def point_blocked(boxes, x, y, radius=8.0, z_max=10.0):
    """地面落点是否被已有着地盒子占用（给路灯/立杆选点用）。"""
    for box in boxes:
        if box["kind"] == "floor":
            continue
        x0, y0, z0, x1, y1, _z1 = box_bounds(box)
        if z0 > z_max:
            continue
        if x0 - radius <= x <= x1 + radius and y0 - radius <= y <= y1 + radius:
            return True
    return False


def _neighbour_ends(box, boxes, axis):
    """洞口沿 axis 两端是否被墙体夹持：返回 {-1: 负端数量, +1: 正端数量}。"""
    x0, y0, z0, x1, y1, z1 = box_bounds(box)
    low = (x0, y0)[axis]
    high = (x1, y1)[axis]
    low_other = (x0, y0)[1 - axis]
    high_other = (x1, y1)[1 - axis]
    ends = {-1: 0, 1: 0}
    for other in boxes:
        if other is box or other["kind"] not in ("wall", "solid", "column"):
            continue
        ox0, oy0, oz0, ox1, oy1, oz1 = box_bounds(other)
        if not (oz0 < z1 - 1.0 and oz1 > z0 + 1.0):
            continue
        other_low_other = (ox0, oy0)[1 - axis]
        other_high_other = (ox1, oy1)[1 - axis]
        if not (other_low_other < high_other - 1.0
                and other_high_other > low_other + 1.0):
            continue
        if abs((ox1, oy1)[axis] - low) <= 6.0:
            ends[-1] += 1
        if abs((ox0, oy0)[axis] - high) <= 6.0:
            ends[1] += 1
    return ends


def span_axis(box, boxes):
    """门洞/窗洞的跨向轴（洞口宽度方向）：取两头有墙体夹持的水平轴。"""
    ends_x = _neighbour_ends(box, boxes, 0)
    ends_y = _neighbour_ends(box, boxes, 1)
    support_x = min(ends_x[-1], ends_x[1])
    support_y = min(ends_y[-1], ends_y[1])
    if support_x != support_y:
        return 0 if support_x > support_y else 1
    # 退路：洞口通常比结构进深窄 → 取较短的水平轴
    return 0 if box["size"][0] <= box["size"][1] else 1


def door_openings(boxes):
    """把 lintel（门/窗/拱的过梁）分类成门洞 / 窗洞 / 拱洞。"""
    doors = []
    for box in boxes:
        if box["kind"] != "lintel":
            continue
        bottom = box["center"][2] - box["size"][2] / 2.0
        axis = span_axis(box, boxes)
        doors.append({
            "box": box,
            "axis": axis,
            "span": box["size"][axis],
            "bottom": bottom,
        })
    return doors


def nearest_site_direction(box, sites):
    """门口外侧法线：从最近包点中心指向门洞的方向（用于遮阳篷/壁灯朝向）。"""
    axis = thin_axis(box)
    distances = [(abs(box["center"][0] - site["center"][0])
                  + abs(box["center"][1] - site["center"][1]), site) for site in sites]
    _, site = min(distances, key=lambda item: item[0])
    sign = 1.0 if box["center"][axis] >= site["center"][axis] else -1.0
    normal = [0.0, 0.0, 0.0]
    normal[axis] = sign
    return normal


def stair_groups(boxes):
    """把楼梯 / 坡道盒按相邻关系聚成组（每组建一段扶手）。"""
    candidates = [box for box in boxes if box["kind"] in ("stairs", "ramp")]
    groups = []
    for box in candidates:
        placed = False
        for group in groups:
            if _near_any(box, group):
                group.append(box)
                placed = True
                break
        if not placed:
            groups.append([box])
    return [group for group in groups if len(group) >= 2]


def _near_any(box, group):
    for other in group:
        if (abs(box["center"][0] - other["center"][0]) <= 90.0
                and abs(box["center"][1] - other["center"][1]) <= 90.0):
            return True
    return False
