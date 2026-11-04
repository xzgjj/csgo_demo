"""装饰件（结构类）：女儿墙压顶、木屋顶、门洞木门框、拱门拱石、边界墙扶壁。"""

from __future__ import annotations

import math

from mathutils import Matrix

from .data import box_bounds, door_openings, thin_axis
from .geometry import GeoBuilder


def build_wall_details(boxes, materials, collection):
    """四周边界墙的内外扶壁 + 腰线（打破大面积空墙，纯视觉）。"""
    created = []
    for wall in boxes:
        if wall["kind"] != "wall":
            continue
        if max(wall["size"][0], wall["size"][1]) < 4000.0:
            continue
        created.append(_wall_buttress(wall, materials, collection))
    return created


def _wall_buttress(wall, materials, collection):
    axis = thin_axis(wall)
    long_axis = 1 - axis
    cx, cy = wall["center"][0], wall["center"][1]
    thickness = wall["size"][axis]
    length = wall["size"][long_axis]
    height = wall["size"][2]
    count = max(3, int(length // 520))
    builder = GeoBuilder()
    for index in range(count):
        ratio = (index + 0.5) / count
        along = -length / 2.0 + ratio * length
        for sign in (-1.0, 1.0):
            center = [cx, cy, height / 2.0]
            center[axis] += sign * (thickness / 2.0 + 10.0)
            center[long_axis] += along
            size = [22.0, 44.0, height]
            size[axis] = 22.0
            size[long_axis] = 44.0
            builder.box(tuple(center), tuple(size), materials["concrete"])
    # 腰线：墙中段的通长挑檐
    center = [cx, cy, height * 0.5]
    size = [thickness + 14.0, 10.0, 12.0]
    size[axis] = thickness + 14.0
    size[long_axis] = length
    builder.box(tuple(center), tuple(size), materials["concrete"])
    return builder.finish(f"decor_wall_buttress_{wall['index']:03d}", collection)


def _trim_object(box, materials, collection, label, with_plinth):
    """一栋墙/建筑的压顶条（可选墙基），合并成单个装饰对象。"""
    x0, y0, _z0, x1, y1, z1 = box_bounds(box)
    builder = GeoBuilder()
    builder.box(((x0 + x1) / 2.0, (y0 + y1) / 2.0, z1 + 6.0),
                (x1 - x0 + 10.0, y1 - y0 + 10.0, 12.0), materials["concrete"])
    if with_plinth:
        builder.box(((x0 + x1) / 2.0, (y0 + y1) / 2.0, 9.0),
                    (x1 - x0 + 16.0, y1 - y0 + 16.0, 18.0), materials["concrete"])
    return builder.finish(f"decor_{label}_{box['index']:03d}", collection)


def _near_site(box, sites):
    for site in sites:
        if (abs(box["center"][0] - site["center"][0]) <= 340.0
                and abs(box["center"][1] - site["center"][1]) <= 340.0):
            return True
    return False


def build_coping(boxes, materials, collection):
    """女儿墙压顶 + 建筑墙基（只加视觉，不动任何碰撞盒）。"""
    created = []
    sites = [box for box in boxes if box["kind"] == "site"]
    for box in boxes:
        if box["kind"] == "solid":
            created.append(
                _trim_object(box, materials, collection, "coping_bldg", True))
        elif box["kind"] == "wall":
            boundary = max(box["size"][0], box["size"][1]) >= 4000.0
            label = "coping_wall" if boundary else "coping_yard"
            if boundary or _near_site(box, sites):
                created.append(_trim_object(box, materials, collection, label, boundary))
    return created


def build_roofs(boxes, materials, collection):
    """木屋顶：女儿墙压顶圈 + 出挑木梁 + 屋面小通风亭。"""
    created = []
    for roof in boxes:
        if roof["kind"] != "roof":
            continue
        created.append(_roof_parapet(roof, materials, collection))
        created.append(_roof_beams(roof, materials, collection))
        created.append(_roof_vent(roof, materials, collection))
    return created


def _roof_parapet(roof, materials, collection):
    x0, y0, _z0, x1, y1, z1 = box_bounds(roof)
    width = x1 - x0
    depth = y1 - y0
    cx = (x0 + x1) / 2.0
    cy = (y0 + y1) / 2.0
    rim = 13.0
    band = 18.0
    builder = GeoBuilder()
    builder.box((cx, y0, z1 + rim / 2.0), (width + 36.0, band, rim),
                materials["concrete"])
    builder.box((cx, y1, z1 + rim / 2.0), (width + 36.0, band, rim),
                materials["concrete"])
    builder.box((x0, cy, z1 + rim / 2.0), (band, depth + 36.0, rim),
                materials["concrete"])
    builder.box((x1, cy, z1 + rim / 2.0), (band, depth + 36.0, rim),
                materials["concrete"])
    return builder.finish(f"decor_roof_parapet_{roof['index']:03d}", collection)


def _roof_beams(roof, materials, collection):
    x0, y0, z0, x1, y1, _z1 = box_bounds(roof)
    width = x1 - x0
    depth = y1 - y0
    cx = (x0 + x1) / 2.0
    cy = (y0 + y1) / 2.0
    long_axis = 0 if width >= depth else 1
    builder = GeoBuilder()
    beams = 5
    for index in range(beams):
        ratio = (index + 0.5) / beams
        if long_axis == 0:
            center = (x0 + ratio * width, cy, z0 - 3.0)
            size = (15.0, depth + 56.0, 16.0)
        else:
            center = (cx, y0 + ratio * depth, z0 - 3.0)
            size = (width + 56.0, 15.0, 16.0)
        builder.box(center, size, materials["wood"])
    return builder.finish(f"decor_roof_beams_{roof['index']:03d}", collection)


def _roof_vent(roof, materials, collection):
    x0, y0, _z0, x1, y1, z1 = box_bounds(roof)
    width = x1 - x0
    depth = y1 - y0
    vent_x = x0 + width * 0.28
    vent_y = y0 + depth * 0.32
    builder = GeoBuilder()
    builder.box((vent_x, vent_y, z1 + 22.0), (34.0, 34.0, 44.0),
                materials["concrete"])
    builder.box((vent_x, vent_y, z1 + 48.0), (44.0, 44.0, 8.0),
                materials["concrete"])
    return builder.finish(f"decor_roof_vent_{roof['index']:03d}", collection)


def build_door_frames(boxes, materials, collection):
    """每个门洞（lintel 下沿 <=140u）加一圈木门框：门樘 + 门楣 + 门槛。"""
    created = []
    for door in door_openings(boxes):
        if door["bottom"] > 140.0:
            continue
        created.append(_door_frame(door, materials, collection))
    return created


def _door_frame(door, materials, collection):
    lintel = door["box"]
    axis = door["axis"]
    span = door["span"]
    bottom = door["bottom"]
    depth = min(lintel["size"][1 - axis], 36.0) - 8.0
    cx = lintel["center"][0]
    cy = lintel["center"][1]
    builder = GeoBuilder()
    if axis == 0:
        for sign in (-1.0, 1.0):
            builder.box((cx + sign * (span / 2.0 - 7.0), cy, (bottom - 14.0) / 2.0),
                        (14.0, depth, bottom - 14.0), materials["wood"])
        builder.box((cx, cy, bottom - 7.0), (span, depth, 14.0), materials["wood"])
        builder.box((cx, cy, 3.0), (span, depth + 8.0, 6.0), materials["wood"])
    else:
        for sign in (-1.0, 1.0):
            builder.box((cx, cy + sign * (span / 2.0 - 7.0), (bottom - 14.0) / 2.0),
                        (depth, 14.0, bottom - 14.0), materials["wood"])
        builder.box((cx, cy, bottom - 7.0), (depth, span, 14.0), materials["wood"])
        builder.box((cx, cy, 3.0), (depth + 8.0, span, 6.0), materials["wood"])
    return builder.finish(f"decor_door_frame_{lintel['index']:03d}", collection)


def build_arches(boxes, materials, collection):
    """拱洞（lintel 下沿 >=176u）：环状拱石 + 拱心石。"""
    created = []
    for door in door_openings(boxes):
        if door["bottom"] >= 176.0:
            created.append(_arch(door, materials, collection))
    return created


def _arch_block(builder, door, theta, tangential, radial, depth, materials):
    lintel = door["box"]
    axis = door["axis"]
    span = door["span"]
    radius = span / 2.0
    arc_center_z = door["bottom"] - radius
    mid_radius = radius - radial / 2.0
    offset_span = math.cos(theta) * mid_radius
    offset_up = math.sin(theta) * mid_radius
    cx = lintel["center"][0]
    cy = lintel["center"][1]
    if axis == 0:
        center = (cx + offset_span, cy, arc_center_z + offset_up)
        rotation = Matrix.Rotation(-(theta + math.pi / 2.0), 3, "Y")
        size = (tangential, depth, radial)
    else:
        center = (cx, cy + offset_span, arc_center_z + offset_up)
        rotation = Matrix.Rotation(theta + math.pi / 2.0, 3, "X")
        size = (depth, tangential, radial)
    builder.box(center, size, materials["concrete"], rotation)


def _arch(door, materials, collection):
    lintel = door["box"]
    depth = lintel["size"][1 - door["axis"]]
    radius = door["span"] / 2.0
    arc_center_z = door["bottom"] - radius
    blocks = 7
    radial = 20.0
    mid_radius = radius - radial / 2.0
    tangential = math.pi * mid_radius / blocks * 1.16
    builder = GeoBuilder()
    for index in range(blocks):
        _arch_block(builder, door, math.pi * (index + 0.5) / blocks, tangential,
                    radial, depth, materials)
    keystone_z = arc_center_z + radius - 10.0
    if door["axis"] == 1:
        keystone_size = (34.0, depth + 10.0, 40.0)
    else:
        keystone_size = (depth + 10.0, 34.0, 40.0)
    builder.box((lintel["center"][0], lintel["center"][1], keystone_z),
                keystone_size, materials["concrete"])
    return builder.finish(f"decor_arch_{lintel['index']:03d}", collection)
