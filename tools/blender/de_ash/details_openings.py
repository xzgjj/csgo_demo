"""装饰件（开口类）：窗洞/窗框/窗板、包点布篷、木箱木纹边框。"""

from __future__ import annotations

import math

from mathutils import Vector

from .data import (box_bounds, door_openings, nearest_site_direction, thin_axis)
from .geometry import GeoBuilder, rot_axes

# 立体建筑（solid）的定向开窗：索引 -> ((法线符号, 沿长轴坐标), ...)
# B 双层楼 -X 面、CT 回防屋 +X 面（另一面紧贴院墙，不开窗）
SOLID_WINDOW_FACES = {
    4: ((-1.0, 850.0),),
    5: ((1.0, -1450.0),),
}
SOLID_WINDOW_FULL_Z = (148.0, 210.0)
SOLID_WINDOW_LOW_Z = (44.0, 94.0)


def add_window(builder, face_x, face_y, normal, long_axis, width, z0, z1,
               materials, shutters=True):
    """在墙面外侧加窗：玻璃 + 木窗框 + 石窗台 + 木窗板。"""
    up = Vector((0.0, 0.0, 1.0))
    u = Vector((1.0, 0.0, 0.0)) if long_axis == 0 else Vector((0.0, 1.0, 0.0))
    n = Vector(normal)
    height = z1 - z0
    zc = (z0 + z1) / 2.0
    base = Vector((face_x, face_y, 0.0))
    rotation = rot_axes(u, n, up)
    builder.box(base + n * 2.0 + up * zc, (width - 6.0, 5.0, height - 6.0),
                materials["glass"], rotation)
    for sign in (-1.0, 1.0):
        builder.box(base + u * (sign * width / 2.0) + n * 4.0 + up * zc,
                    (9.0, 9.0, height + 8.0), materials["wood"], rotation)
    builder.box(base + up * (z1 + 5.0) + n * 4.0, (width + 18.0, 9.0, 10.0),
                materials["wood"], rotation)
    builder.box(base + up * (z0 - 6.0) + n * 6.0, (width + 24.0, 12.0, 12.0),
                materials["concrete"], rotation)
    if shutters:
        shutter_width = width * 0.5
        for sign in (-1.0, 1.0):
            builder.box(base + u * (sign * (width / 2.0 + 5.0 + shutter_width / 2.0))
                        + n * 2.5 + up * zc,
                        (shutter_width, 3.0, height), materials["wood"], rotation)


def _glass_windows(boxes, materials, collection):
    created = []
    for glass in boxes:
        if glass["kind"] != "prop" or glass["material"] != "glass":
            continue
        x0, y0, z0, x1, y1, z1 = box_bounds(glass)
        builder = GeoBuilder()
        add_window(builder, (x0 + x1) / 2.0, y0, (0.0, -1.0, 0.0), 0, x1 - x0,
                   z0, z1, materials)
        created.append(builder.finish(
            f"decor_window_glass_{glass['index']:03d}", collection))
    return created


def _is_house_wall(wall, roof):
    """是否是某栋木屋顶建筑的长向外墙（用来开窗）。"""
    if wall["kind"] != "wall" or min(wall["size"][0], wall["size"][1]) > 40.0:
        return False
    if wall["size"][2] < 200.0:
        return False
    rx0, ry0, _rz0, rx1, ry1, _rz1 = box_bounds(roof)
    if not (rx0 - 40.0 <= wall["center"][0] <= rx1 + 40.0):
        return False
    if not (ry0 - 40.0 <= wall["center"][1] <= ry1 + 40.0):
        return False
    return wall["size"][1 - thin_axis(wall)] >= 160.0


def _house_window(wall, roof, materials, collection):
    axis = thin_axis(wall)
    long_axis = 1 - axis
    outward = 1.0 if wall["center"][axis] > roof["center"][axis] else -1.0
    normal = [0.0, 0.0, 0.0]
    normal[axis] = outward
    offset = Vector(normal) * (wall["size"][axis] / 2.0)
    face = Vector((wall["center"][0], wall["center"][1], 0.0)) + offset
    builder = GeoBuilder()
    add_window(builder, face.x, face.y, normal, long_axis, 50.0, 140.0, 206.0,
               materials)
    return builder.finish(f"decor_window_house_{wall['index']:03d}", collection)


def _house_windows(boxes, materials, collection):
    created = []
    for roof in boxes:
        if roof["kind"] != "roof":
            continue
        for wall in boxes:
            if _is_house_wall(wall, roof):
                created.append(_house_window(wall, roof, materials, collection))
    return created


def _solid_window_z(solid):
    if solid["size"][2] >= 200.0:
        return SOLID_WINDOW_FULL_Z
    if solid["size"][2] >= 120.0:
        return SOLID_WINDOW_LOW_Z
    return None


def _solid_window_faces(solid):
    """返回 (faces, width, normal_axis, long_axis)。"""
    faces = SOLID_WINDOW_FACES.get(solid["index"])
    if faces is not None:
        return faces, 48.0, 0, 1
    axis = thin_axis(solid)
    long_axis = 1 - axis
    width = min(max(solid["size"][long_axis] * 0.18, 42.0), 58.0)
    center = solid["center"][long_axis]
    return ((-1.0, center), (1.0, center)), width, axis, long_axis


def _solid_window(solid, face_spec, width, axis, long_axis, window_z,
                  materials, collection):
    sign, _along = face_spec
    normal = [0.0, 0.0, 0.0]
    normal[axis] = sign
    offset = Vector(normal) * (solid["size"][axis] / 2.0)
    face = Vector((solid["center"][0], solid["center"][1], 0.0)) + offset
    builder = GeoBuilder()
    add_window(builder, face.x, face.y, normal, long_axis, width, window_z[0],
               window_z[1], materials)
    suffix = "wm" if sign < 0.0 else "wp"
    return builder.finish(
        f"decor_window_solid_{solid['index']:03d}_{suffix}", collection)


def _solid_windows(boxes, materials, collection):
    created = []
    for solid in boxes:
        if solid["kind"] != "solid":
            continue
        window_z = _solid_window_z(solid)
        if window_z is None:
            continue
        faces, width, axis, long_axis = _solid_window_faces(solid)
        for face_spec in faces:
            created.append(_solid_window(solid, face_spec, width, axis, long_axis,
                                         window_z, materials, collection))
    return created


def build_windows(boxes, materials, collection):
    """窗户：玻璃 prop 处做窗框；木屋顶建筑长墙各开一扇；立体建筑大面开窗。"""
    created = []
    created += _glass_windows(boxes, materials, collection)
    created += _house_windows(boxes, materials, collection)
    created += _solid_windows(boxes, materials, collection)
    return created


def build_awnings(boxes, materials, collection):
    """包点院墙门洞上方的布篷（斜挂布 + 两根木撑杆 + 墙面托木）。"""
    sites = [box for box in boxes if box["kind"] == "site"]
    candidates = []
    for door in door_openings(boxes):
        if door["bottom"] > 140.0:
            continue
        distance = min(abs(door["box"]["center"][0] - site["center"][0])
                       + abs(door["box"]["center"][1] - site["center"][1])
                       for site in sites)
        if distance <= 360.0:
            candidates.append((door, distance))
    candidates.sort(key=lambda item: (-item[0]["span"], item[0]["box"]["index"]))
    return [_awning(door, sites, materials, collection)
            for door, _distance in candidates[:4]]


def _awning(door, sites, materials, collection):
    lintel = door["box"]
    axis = door["axis"]
    span = door["span"]
    bottom = door["bottom"]
    normal = nearest_site_direction(lintel, sites)
    n = Vector(normal)
    u = Vector((1.0, 0.0, 0.0)) if axis == 0 else Vector((0.0, 1.0, 0.0))
    up = Vector((0.0, 0.0, 1.0))
    offset = n * (lintel["size"][1 - axis] / 2.0)
    face = Vector((lintel["center"][0], lintel["center"][1], 0.0)) + offset
    attach_z = bottom + 26.0
    slope = math.radians(22.0)
    direction = (n * math.cos(slope) - up * math.sin(slope)).normalized()
    side = u.cross(direction).normalized()
    rotation = rot_axes(u, direction, side)
    builder = GeoBuilder()
    builder.box(face + n * 30.0 + up * (attach_z - 10.0),
                (span + 70.0, 62.0, 5.0), materials["fabric"], rotation)
    outer = face + n * 56.0
    for sign in (-1.0, 1.0):
        builder.box(outer + u * (sign * (span / 2.0 + 26.0))
                    + up * ((attach_z - 24.0) / 2.0),
                    (9.0, 9.0, attach_z - 24.0), materials["wood"])
        builder.box(face + n * 7.0 + u * (sign * (span / 2.0 + 18.0))
                    + up * attach_z, (12.0, 16.0, 10.0), materials["wood"])
    return builder.finish(f"decor_awning_{lintel['index']:03d}", collection)


def _is_crate(box):
    if box["material"] != "wood":
        return False
    return min(box["size"][0], box["size"][1]) >= 44.0 and box["size"][2] >= 40.0


def _is_barrel(box):
    if box["material"] != "metal":
        return False
    return box["size"][2] >= 40.0 and max(box["size"][0], box["size"][1]) <= 60.0


def _crate_frame(box, materials, collection):
    x0, y0, z0, x1, y1, z1 = box_bounds(box)
    builder = GeoBuilder()
    strip = 11.0
    for cx in (x0 + strip / 2.0, x1 - strip / 2.0):
        for cy in (y0 + strip / 2.0, y1 - strip / 2.0):
            builder.box((cx, cy, (z0 + z1) / 2.0), (strip, strip, z1 - z0),
                        materials["wood"])
    for zc in (z0 + 7.0, z1 - 7.0):
        builder.box(((x0 + x1) / 2.0, y0 + strip / 2.0, zc),
                    (x1 - x0 - 2.0, strip, 14.0), materials["wood"])
        builder.box(((x0 + x1) / 2.0, y1 - strip / 2.0, zc),
                    (x1 - x0 - 2.0, strip, 14.0), materials["wood"])
        builder.box((x0 + strip / 2.0, (y0 + y1) / 2.0, zc),
                    (strip, y1 - y0 - 2.0, 14.0), materials["wood"])
        builder.box((x1 - strip / 2.0, (y0 + y1) / 2.0, zc),
                    (strip, y1 - y0 - 2.0, 14.0), materials["wood"])
    return builder.finish(f"decor_crate_frame_{box['index']:03d}", collection)


def _barrel_hoops(box, materials, collection):
    x0, y0, z0, x1, y1, z1 = box_bounds(box)
    builder = GeoBuilder()
    center_x = (x0 + x1) / 2.0
    center_y = (y0 + y1) / 2.0
    for ratio in (0.32, 0.68):
        builder.box((center_x, center_y, z0 + (z1 - z0) * ratio),
                    (x1 - x0 + 6.0, y1 - y0 + 6.0, 5.0), materials["metal"])
    return builder.finish(f"decor_barrel_hoops_{box['index']:03d}", collection)


def build_crates(boxes, materials, collection):
    """木箱：木纹边框（角柱 + 上下框条）；油桶加两道箍。"""
    created = []
    for box in boxes:
        if box["kind"] != "prop":
            continue
        if _is_crate(box):
            created.append(_crate_frame(box, materials, collection))
        elif _is_barrel(box):
            created.append(_barrel_hoops(box, materials, collection))
    return created
