"""装饰件（通行类）：楼梯/坡道扶手、天桥与猫道的栏杆和支撑梁。"""

from __future__ import annotations

import math

from mathutils import Vector

from .data import box_bounds, stair_groups
from .geometry import GeoBuilder, rot_axes


def _run_direction(group):
    """楼梯沿哪个水平轴爬升、向哪边升：返回 (axis, sign)。"""
    for axis in (0, 1):
        ordered = sorted(group, key=lambda box: box["center"][axis])
        tops = [box["center"][2] + box["size"][2] / 2.0 for box in ordered]
        rising = all(high > low for low, high in zip(tops, tops[1:]))
        falling = all(low > high for low, high in zip(tops, tops[1:]))
        if rising:
            return axis, 1.0
        if falling:
            return axis, -1.0
    return 0, 1.0


def _surface_z(group, run_axis, coord):
    nearest = min(group, key=lambda box: abs(box["center"][run_axis] - coord))
    return nearest["center"][2] + nearest["size"][2] / 2.0


def _side_bounds(group, side_axis):
    low = min(box["center"][side_axis] - box["size"][side_axis] / 2.0
              for box in group)
    high = max(box["center"][side_axis] + box["size"][side_axis] / 2.0
               for box in group)
    return low, high


def _stair_side_rail(builder, context, side_position, materials):
    group, run_axis, side_axis = context["group"], context["run_axis"], context["side_axis"]
    run_low, run_high = context["run_low"], context["run_high"]
    rail_height = context["rail_height"]
    for ratio in (0.0, 0.5, 1.0):
        coord = run_low + (run_high - run_low) * ratio
        base_z = _surface_z(group, run_axis, coord)
        world = Vector((0.0, 0.0, 0.0))
        world[run_axis] = coord
        world[side_axis] = side_position
        world.z = base_z + rail_height / 2.0
        builder.box(world, (10.0, 10.0, rail_height), materials["wood"])
    middle = Vector((0.0, 0.0, 0.0))
    middle[run_axis] = (run_low + run_high) / 2.0
    middle[side_axis] = side_position
    middle.z = (context["z_start"] + context["z_end"]) / 2.0 + rail_height - 4.0
    builder.box(middle, (context["rail_length"], 9.0, 9.0), materials["wood"],
                context["rotation"])
    middle.z = (context["z_start"] + context["z_end"]) / 2.0 + rail_height * 0.5
    builder.box(middle, (context["rail_length"], 7.0, 7.0), materials["wood"],
                context["rotation"])


def _stair_context(group):
    """一段楼梯的扶手几何上下文（爬升轴、坡向、长度、两侧边界）。"""
    run_axis, run_sign = _run_direction(group)
    side_axis = 1 - run_axis
    coords = [box["center"][run_axis] for box in group]
    run_low, run_high = min(coords), max(coords)
    side_low, side_high = _side_bounds(group, side_axis)
    start_coord = run_low if run_sign > 0.0 else run_high
    end_coord = run_high if run_sign > 0.0 else run_low
    z_start = _surface_z(group, run_axis, start_coord)
    z_end = _surface_z(group, run_axis, end_coord)
    rail_height = 46.0
    run_dir = Vector((0.0, 0.0, 0.0))
    run_dir[run_axis] = run_sign
    up = Vector((0.0, 0.0, 1.0))
    horizontal = run_dir * ((end_coord - start_coord) * run_sign)
    slope_dir = (horizontal + up * (z_end - z_start)).normalized()
    side_dir = Vector((0.0, 0.0, 0.0))
    side_dir[side_axis] = 1.0
    perp = slope_dir.cross(side_dir).normalized()
    return {
        "group": group,
        "run_axis": run_axis,
        "side_axis": side_axis,
        "run_low": run_low,
        "run_high": run_high,
        "z_start": z_start,
        "z_end": z_end,
        "rail_height": rail_height,
        "rail_length": math.hypot(run_high - run_low, z_end - z_start) + 26.0,
        "rotation": rot_axes(slope_dir, side_dir, perp),
        "side_low": side_low,
        "side_high": side_high,
    }


def build_stair_rails(boxes, materials, collection):
    """楼梯/坡道扶手：两侧立柱 + 斜扶手 + 中横杆。"""
    created = []
    for group in stair_groups(boxes):
        created.append(_stair_rail_group(group, materials, collection))
    return created


def _stair_rail_group(group, materials, collection):
    context = _stair_context(group)
    builder = GeoBuilder()
    for side_position in (context["side_low"] + 10.0, context["side_high"] - 10.0):
        _stair_side_rail(builder, context, side_position, materials)
    index = min(box["index"] for box in group)
    return builder.finish(f"decor_stair_rail_{index:03d}", collection)


def _catwalk_point(long_axis, ratio, bounds, side_coord):
    x0, y0, _z0, x1, y1, _z1 = bounds
    world = Vector((0.0, 0.0, 0.0))
    if long_axis == 0:
        world.x = x0 + ratio * (x1 - x0)
        world.y = side_coord
    else:
        world.x = side_coord
        world.y = y0 + ratio * (y1 - y0)
    return world


def _catwalk_rail_bar(builder, long_axis, bounds, side_coord, height, thickness,
                      materials):
    x0, y0, _z0, x1, y1, _z1 = bounds
    if long_axis == 0:
        builder.box(((x0 + x1) / 2.0, side_coord, height),
                    ((x1 - x0) + 20.0, thickness, thickness), materials["wood"])
    else:
        builder.box((side_coord, (y0 + y1) / 2.0, height),
                    (thickness, (y1 - y0) + 20.0, thickness), materials["wood"])


def _catwalk_railing(builder, bounds, long_axis, short_axis, materials):
    x0, y0, _z0, x1, y1, z1 = bounds
    for side in (0, 1):
        side_coord = (y0, y1)[side] if short_axis == 1 else (x0, x1)[side]
        posts = 6
        for index in range(posts):
            ratio = (index + 0.5) / posts
            world = _catwalk_point(long_axis, ratio, bounds, side_coord)
            world.z = z1 + 24.0
            builder.box(world, (11.0, 11.0, 48.0), materials["wood"])
        _catwalk_rail_bar(builder, long_axis, bounds, side_coord, z1 + 44.0, 8.0,
                          materials)
        _catwalk_rail_bar(builder, long_axis, bounds, side_coord, z1 + 22.0, 7.0,
                          materials)


def _catwalk_beams(builder, bounds, long_axis, materials):
    x0, y0, z0, x1, y1, _z1 = bounds
    width = x1 - x0
    depth = y1 - y0
    short_length = min(width, depth)
    beams = 4
    for index in range(beams):
        ratio = (index + 0.5) / beams
        if long_axis == 0:
            center = (x0 + ratio * width, y0 + depth / 2.0, z0 - 9.0)
            size = (16.0, short_length + 44.0, 18.0)
        else:
            center = (x0 + width / 2.0, y0 + ratio * depth, z0 - 9.0)
            size = (short_length + 44.0, 16.0, 18.0)
        builder.box(center, size, materials["wood"])
    long_length = max(width, depth) + 24.0
    for side in (0, 1):
        if long_axis == 0:
            builder.box(((x0 + x1) / 2.0, (y0, y1)[side], z0 - 5.0),
                        (long_length, 12.0, 14.0), materials["wood"])
        else:
            builder.box(((x0, x1)[side], (y0 + y1) / 2.0, z0 - 5.0),
                        (12.0, long_length, 14.0), materials["wood"])


def build_catwalk_details(boxes, materials, collection):
    """天桥/猫道：两侧栏杆（立柱 + 两道横杆）+ 桥下横梁 + 侧边梁。"""
    created = []
    for catwalk in boxes:
        if catwalk["kind"] != "catwalk":
            continue
        created.append(_catwalk_object(catwalk, materials, collection))
    return created


def _catwalk_object(catwalk, materials, collection):
    bounds = box_bounds(catwalk)
    x0, y0, _z0, x1, y1, _z1 = bounds
    width = x1 - x0
    depth = y1 - y0
    long_axis = 0 if width >= depth else 1
    short_axis = 1 - long_axis
    builder = GeoBuilder()
    _catwalk_railing(builder, bounds, long_axis, short_axis, materials)
    _catwalk_beams(builder, bounds, long_axis, materials)
    return builder.finish(f"decor_catwalk_{catwalk['index']:03d}", collection)
