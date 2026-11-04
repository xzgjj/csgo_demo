"""装饰件（道具类）：壁灯/路灯、招牌、木箱篷布、外围沙地裙板。"""

from __future__ import annotations

from mathutils import Vector

from .data import (box_bounds, door_openings, nearest_site_direction,
                   point_blocked)
from .geometry import GeoBuilder


def _sample_doors(boxes, sites, span_min=0.0):
    """包点院墙附近的门洞（用于挂灯/招牌）：按距离 <=360u 筛选。"""
    doors = []
    for door in door_openings(boxes):
        if door["bottom"] > 140.0 or door["span"] < span_min:
            continue
        lintel = door["box"]
        distance = min(abs(lintel["center"][0] - site["center"][0])
                       + abs(lintel["center"][1] - site["center"][1])
                       for site in sites)
        if distance <= 360.0:
            doors.append(door)
    return doors


def _wall_lamp(door, sites, materials, collection):
    lintel = door["box"]
    axis = door["axis"]
    span = door["span"]
    normal = nearest_site_direction(lintel, sites)
    n = Vector(normal)
    u = Vector((1.0, 0.0, 0.0)) if axis == 0 else Vector((0.0, 1.0, 0.0))
    up = Vector((0.0, 0.0, 1.0))
    face = Vector((lintel["center"][0], lintel["center"][1], 0.0)) \
        + n * (lintel["size"][1 - axis] / 2.0)
    anchor = face + u * (span / 2.0 - 24.0) + up * (door["bottom"] + 56.0)
    arm_size = (30.0, 8.0, 8.0) if normal[0] != 0.0 else (8.0, 30.0, 8.0)
    builder = GeoBuilder()
    builder.box(anchor + n * 4.0, (10.0, 10.0, 8.0), materials["metal"])
    builder.box(anchor + n * 18.0, arm_size, materials["metal"])
    head = anchor + n * 32.0 - up * 12.0
    builder.box(head, (16.0, 16.0, 24.0), materials["metal"])
    builder.box(head - up * 15.0, (21.0, 21.0, 6.0), materials["metal"])
    return builder.finish(f"decor_wall_lamp_{lintel['index']:03d}", collection)


def _wall_lamps(boxes, materials, collection):
    sites = [box for box in boxes if box["kind"] == "site"]
    created = []
    for door in _sample_doors(boxes, sites)[:6]:
        created.append(_wall_lamp(door, sites, materials, collection))
    return created


def _street_lamp_anchors(sites):
    anchors = []
    for site in sites:
        for sign in (-1.0, 1.0):
            anchors.append((site["center"][0] + sign * 430.0, site["center"][1]))
    anchors.append((150.0, -270.0))
    anchors.append((-150.0, 270.0))
    return anchors


def _street_lamp(x, y, order, materials, collection):
    toward = -1.0 if x > 0.0 else 1.0
    builder = GeoBuilder()
    builder.box((x, y, 8.0), (32.0, 32.0, 16.0), materials["metal"])
    builder.box((x, y, 92.0), (11.0, 11.0, 168.0), materials["metal"])
    builder.box((x + toward * 26.0, y, 176.0), (56.0, 10.0, 8.0),
                materials["metal"])
    builder.box((x + toward * 50.0, y, 176.0), (18.0, 18.0, 8.0),
                materials["metal"])
    builder.box((x + toward * 50.0, y, 158.0), (24.0, 24.0, 30.0),
                materials["metal"])
    return builder.finish(f"decor_street_lamp_{order:02d}", collection)


def _street_lamps(boxes, materials, collection):
    sites = [box for box in boxes if box["kind"] == "site"]
    created = []
    for order, (x, y) in enumerate(_street_lamp_anchors(sites)):
        if point_blocked(boxes, x, y, radius=18.0):
            continue
        created.append(_street_lamp(x, y, order, materials, collection))
    return created


def build_lamps(boxes, materials, collection):
    """壁灯（包点门洞旁）+ 街灯（包点外围 / 中路天桥两侧，未占用地面才放）。"""
    created = []
    created += _wall_lamps(boxes, materials, collection)
    created += _street_lamps(boxes, materials, collection)
    return created


def _door_sign(door, sites, materials, collection):
    lintel = door["box"]
    axis = door["axis"]
    normal = nearest_site_direction(lintel, sites)
    n = Vector(normal)
    up = Vector((0.0, 0.0, 1.0))
    face = Vector((lintel["center"][0], lintel["center"][1], 0.0)) \
        + n * (lintel["size"][1 - axis] / 2.0)
    plate_center = face + n * 6.0 + up * (door["bottom"] + 84.0)
    builder = GeoBuilder()
    builder.box(plate_center, (78.0, 6.0, 30.0), materials["wood"])
    builder.box(plate_center + up * 20.0, (40.0, 14.0, 7.0), materials["wood"])
    return builder.finish(f"decor_sign_{lintel['index']:03d}", collection)


def _door_signs(boxes, materials, collection):
    sites = [box for box in boxes if box["kind"] == "site"]
    created = []
    for door in _sample_doors(boxes, sites, span_min=150.0)[:4]:
        created.append(_door_sign(door, sites, materials, collection))
    return created


def _crate_tarp(box, materials, collection):
    x0, y0, _z0, x1, y1, z1 = box_bounds(box)
    builder = GeoBuilder()
    builder.box(((x0 + x1) / 2.0, (y0 + y1) / 2.0, z1 + 4.0),
                (x1 - x0 + 10.0, y1 - y0 + 10.0, 8.0), materials["fabric"])
    return builder.finish(f"decor_tarp_{box['index']:03d}", collection)


def _crate_tarps(boxes, materials, collection):
    created = []
    for box in boxes:
        if len(created) >= 2:
            break
        if box["kind"] != "prop" or box["material"] != "wood":
            continue
        if box["size"][2] < 60.0 or min(box["size"][0], box["size"][1]) < 60.0:
            continue
        created.append(_crate_tarp(box, materials, collection))
    return created


def build_signs(boxes, materials, collection):
    """招牌（包点门洞上方的木牌 + 木托架）+ 两处木箱篷布。"""
    created = []
    created += _door_signs(boxes, materials, collection)
    created += _crate_tarps(boxes, materials, collection)
    return created


def build_ground_skirt(boxes, materials, collection):
    """地图外围沙地裙板（避免俯视机位看到楼板悬空）；纯视觉，不参与碰撞。"""
    xs = [box["center"][0] for box in boxes]
    ys = [box["center"][1] for box in boxes]
    extent = max(max(abs(value) for value in xs), max(abs(value) for value in ys))
    half = extent + 2600.0
    builder = GeoBuilder()
    builder.box((0.0, 0.0, -9.0), (half * 2.0, half * 2.0, 16.0),
                materials["dirt"])
    return [builder.finish("decor_ground_skirt", collection)]
