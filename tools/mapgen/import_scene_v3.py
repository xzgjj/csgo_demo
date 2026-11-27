"""把 scene-design/scene-elements.csv 转成 de_dust2 布局与盒子清单（测试用，不覆盖 de_ash）。

用法：python -X utf8 tools/mapgen/import_scene_v3.py
输入：docs/scene-design/scene-elements.csv（本地文档，不入库）
输出：content/maps/de_dust2.layout.json、host/scenes/de_dust2_boxes.json
说明：墙按 doorway/window 行做 1D 挖洞；+64/+128 地面生成实体台；坡道/台阶按 32u 分级；
      围合外壳与「漏进虚空」修复由 geom_shell / geom_reach 派生，不进手工台账。
"""

from __future__ import annotations

import csv
import json
from pathlib import Path

import gen_blockout
import geom
import geom_reach
import geom_shell
import prop_emit
import scene_contract

ROOT = Path(__file__).resolve().parents[2]
CSV = ROOT / "docs/scene-design/scene-elements.csv"
LAYOUT_OUT = ROOT / "content/maps/de_dust2.layout.json"
BOXES_OUT = ROOT / "host/scenes/de_dust2_boxes.json"

MATERIAL = {
    "concrete": "concrete",
    "metal": "metal",
    "wood": "wood",
    "glass": "glass",
    "fabric": "fabric",
    "stone": "stone",
    "dirt": "dirt",
    "sandbag": "fabric",
    "tarp": "wood",
    "railing": "metal",
}
SOLID_TYPES = {"wall"}
PROP_TYPES = {"prop", "tarp", "sandbag", "railing", "door"}
SURFACE_TYPES = {"floor", "room", "tunnel", "spawn", "buyzone", "choke", "step",
                 "platform", "ramp", "catwalk", "stairs", "pit", "doorway", "window"}

RUN_SPEED = 250.0
STEP = 32.0
SHELL_ROUNDS = 3
# 墙顶统一上限：与 geom_shell.WALL_HEIGHT 一致。原 384 的“两层楼”墙在灰盒里读作 13m 深墙，
# 直接压到 256（人眼 64 / 跳高 72 的三倍半），天际线交给屋顶退台，不再靠加高墙体撑场面。
WALL_TOP = 256.0

def load_rows():
    with open(CSV, encoding="utf-8") as handle:
        return [{key: row[key] for key in row} for row in csv.DictReader(handle)]

def rect_of(row):
    return (int(row["rect_x1"]), int(row["rect_y1"]), int(row["rect_x2"]), int(row["rect_y2"]))

def overlaps(a, b, tol=2):
    return not (a[2] <= b[0] + tol or b[2] <= a[0] + tol or a[3] <= b[1] + tol or b[3] <= a[1] + tol)

def wall_pieces(wall, openings):
    """墙沿长轴挖洞，返回 [(x0,x1,y0,y1,z0,z1)]；窗保留窗台与过梁。"""
    x0, y0, x1, y1 = rect_of(wall)
    height = float(wall["height_u"])
    axis = "x" if (x1 - x0) >= (y1 - y0) else "y"
    lo, hi = (x0, x1) if axis == "x" else (y0, y1)
    holes = _wall_holes(wall, openings, axis, lo, hi)
    pieces = _hole_pieces(lo, hi, height, holes)
    out = []
    for a0, a1, z0, z1 in pieces:
        if a1 - a0 <= 0.5 or z1 - z0 <= 0.5:
            continue
        if axis == "x":
            out.append((a0, a1, y0, y1, z0, z1))
        else:
            out.append((x0, x1, a0, a1, z0, z1))
    return out

def _wall_holes(wall, openings, axis, lo, hi):
    holes = []
    for opening in openings:
        if not overlaps(rect_of(wall), rect_of(opening)):
            continue
        ox0, oy0, ox1, oy1 = rect_of(opening)
        a0, a1 = (ox0, ox1) if axis == "x" else (oy0, oy1)
        a0, a1 = max(lo, a0), min(hi, a1)
        if a1 <= a0:
            continue
        window = opening["type"] == "window"
        holes.append((a0, a1, window))
    return sorted(holes)

def _hole_pieces(lo, hi, height, holes):
    pieces = []
    cursor = lo
    for a0, a1, window in holes:
        if a0 > cursor:
            pieces.append((cursor, a0, 0.0, height))
        if window:
            pieces.append((a0, a1, 0.0, 96.0))
            pieces.append((a0, a1, 192.0, height))
        cursor = max(cursor, a1)
    if cursor < hi:
        pieces.append((cursor, hi, 0.0, height))
    return pieces

def surface_top(platforms, x, y):
    """点位上已有平台的最高顶面（用于把道具/墙压到台面上）。"""
    top = 0.0
    for (px0, py0, px1, py1), height in platforms:
        if px0 <= x <= px1 and py0 <= y <= py1:
            top = max(top, height)
    return top

def _emit_walls(row, openings, platforms, structures, building_groups):
    x0, y0, x1, y1 = rect_of(row)
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    material = MATERIAL.get(row["surface"], "concrete")
    height = float(row["height_u"])
    base = surface_top(platforms, cx, cy)
    top = min(max(height, base + 96.0), WALL_TOP)
    building_groups.append({
        "id": row["id"],
        "kind": "wall",
        "center": [cx, cy, (base + top) / 2.0],
        "size": [abs(x1 - x0), abs(y1 - y0), top - base],
        "height": top - base,
    })
    for index, (bx0, bx1, by0, by1, bz0, bz1) in enumerate(wall_pieces(row, openings)):
        z0 = base + bz0
        z1 = min(base + bz1, top)
        if z1 - z0 <= 0.5:
            continue
        structures.append({
            "id": f"{row['id']}_{index}",
            "kind": "wall",
            "center": [(bx0 + bx1) / 2.0, (by0 + by1) / 2.0],
            "size": [bx1 - bx0, by1 - by0],
            "height": z1 - z0,
            "base_z": z0,
            "material": material,
        })

def _emit_prop(row, platforms, props):
    bounds = rect_of(row)
    cx, cy = (bounds[0] + bounds[2]) / 2.0, (bounds[1] + bounds[3]) / 2.0
    height, hp, dent_only = prop_emit.stats(row)
    base = surface_top(platforms, cx, cy)
    material = MATERIAL.get(row["surface"], "concrete")
    panels = prop_emit.split_railing(row, bounds, base, material, hp)
    if panels is not None:
        props.extend(panels)
        return
    props.append(prop_emit.prop_box(row, bounds, base, material, height, hp, dent_only))

def _emit_room(row, platforms, props, building_groups):
    x0, y0, x1, y1 = rect_of(row)
    height = float(row["height_u"])
    building_groups.append({
        "id": row["id"],
        "kind": "room",
        "center": [(x0 + x1) / 2.0, (y0 + y1) / 2.0, max(height, 0.0) / 2.0],
        "size": [abs(x1 - x0), abs(y1 - y0), max(height, 0.0)],
        "height": max(height, 0.0),
    })
    if height > 0:
        _emit_platform(row, platforms, props)

def _emit_level_platform(row, platforms, props):
    if float(row["height_u"]) > 0:
        _emit_platform(row, platforms, props)

def _emit_level_stairs(row, props):
    if float(row["height_u"]) > 0:
        _emit_stairs(row, props)

def _emit_roof(row, structures):
    """屋面板：height_u 为板底标高，固定 32 厚；不参与可行走并集。

    板底钳到 WALL_TOP：墙压到 256 后，原来 384 的屋面板必须跟着落下，否则又变成一块悬空大板。
    """
    x0, y0, x1, y1 = rect_of(row)
    base = min(float(row["height_u"]), WALL_TOP - 32.0)
    if base <= 0.0:
        return
    structures.append({
        "id": row["id"],
        "kind": "wall",
        "center": [(x0 + x1) / 2.0, (y0 + y1) / 2.0, base + 16.0],
        "size": [abs(x1 - x0), abs(y1 - y0), 32.0],
        "height": 32.0,
        "base_z": base,
        "material": MATERIAL.get(row["surface"], "concrete"),
        "collision": False,  # 屋面板只封顶，不参与玩家碰撞
    })

def _collect_elements(rows):
    openings = [r for r in rows if r["type"] in {"doorway", "window"}]
    groups, structures, props, platforms = [], [], [], []
    for row in rows:
        if row["type"] in SOLID_TYPES:
            _emit_walls(row, openings, platforms, structures, groups)
    for row in rows:
        handler = TYPE_HANDLERS.get(row["type"])
        if handler is not None:
            handler(row, platforms, structures, props, groups)
    return groups, structures, props, platforms

TYPE_HANDLERS = {
    "window": lambda row, pl, st, props, groups: scene_contract.emit_window(
        row, rect_of, surface_top, pl, props),
    "room": lambda row, pl, st, props, groups: _emit_room(row, pl, props, groups),
    "platform": lambda row, pl, st, props, groups: _emit_level_platform(row, pl, props),
    "catwalk": lambda row, pl, st, props, groups: _emit_level_platform(row, pl, props),
    "floor": lambda row, pl, st, props, groups: _emit_level_platform(row, pl, props),
    "tunnel": lambda row, pl, st, props, groups: _emit_level_platform(row, pl, props),
    "ramp": lambda row, pl, st, props, groups: _emit_level_stairs(row, props),
    "stairs": lambda row, pl, st, props, groups: _emit_level_stairs(row, props),
    "roof": lambda row, pl, st, props, groups: _emit_roof(row, st),
}
TYPE_HANDLERS.update({
    kind: (lambda row, pl, st, props, groups: _emit_prop(row, pl, props))
    for kind in PROP_TYPES
})

def _emit_platform(row, platforms, props):
    x0, y0, x1, y1 = rect_of(row)
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    sx, sy = x1 - x0, y1 - y0
    height = float(row["height_u"])
    if any(p[1] == height and rect_of(row) == p[0] for p in platforms):
        return
    platforms.append((rect_of(row), height))
    props.append({
        "id": row["id"],
        "kind": "platform",
        "center": [cx, cy, 0.0],
        "size": [sx, sy, height],
        "material": MATERIAL.get(row["surface"], "concrete"),
        "walkable": True,
    })

def _emit_stairs(row, props):
    x0, y0, x1, y1 = rect_of(row)
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    sx, sy = x1 - x0, y1 - y0
    height = float(row["height_u"])
    material = MATERIAL.get(row["surface"], "concrete")
    steps = max(1, int(round(height / STEP)))
    rise = height / steps
    for i in range(steps):
        if sy >= sx:
            depth = sy / steps
            center, size = [cx, y0 + depth * (i + 1) - depth / 2.0, 0.0], [sx, depth, rise * (i + 1)]
        else:
            depth = sx / steps
            center, size = [x0 + depth * (i + 1) - depth / 2.0, cy, 0.0], [depth, sy, rise * (i + 1)]
        props.append({
            "id": f"{row['id']}_step{i}",
            "kind": "stairs",
            "center": center,
            "size": size,
            "material": material,
            "walkable": True,
        })

def _apply_shell(layout, rows):
    layout["structures"].extend(geom_shell.build(layout, rows))
    for round_index in range(SHELL_ROUNDS):
        patches = geom_reach.repair_blocks(layout, geom.derive(layout))
        if not patches:
            break
        layout["structures"].extend(patches)
        print(f"shell repair round {round_index + 1}: +{len(patches)} 块")

def _fill_rotation(layout):
    for lane in layout["lanes"]:
        total = geom.lane_length(lane["points"])
        layout["rotation_seconds_target"][lane["id"]] = round(total / RUN_SPEED, 1)

def _build_layout(structures, props, building_groups, doorways):
    return {
        "schema": "map-layout/3",
        "name": "de_dust2",
        "reference": "de_dust2 雷达（仅拓扑/节奏参考；几何、命名、贴图全部自建，见 SPEC-01）",
        "units": "source_units",
        "bounds": {"min": [-2300, -2300, 0], "max": [2300, 2300, 288]},
        "floor_z": 0,
        "wall_height": 256,
        "perimeter_thickness": 32,
        # 地面材质：de_dust2 全部通道地面用 road（压实土），de_ash 仍走默认 dirt。
        "ground_material": "road",
        "layers": [
            {"id": "L0", "z": 0, "walkable": True, "note": "地面：通道/包点/出生"},
            {"id": "L1", "z": 64, "walkable": True, "note": "一级台/猫道/侧屋"},
            {"id": "L2", "z": 128, "walkable": True, "note": "A 台/A 短"},
        ],
        "reach": {
            "jump_max": 72,
            "step_max": 32,
            "door_width": 96,
            "door_height": 128,
            "roof_walkable": False,
            "max_building_height": 256,
            "note": "v3 折角/支路；单级 32u，门洞 96/128",
        },
        "materials": ["concrete", "metal", "wood", "glass", "fabric", "road"],
        "spawns": {
            "T": {
                "center": [-565, -1835, 0],
                "size": [770, 520],
                "facing": [0, 1],
                "slots": [[-780, -1900], [-600, -1900], [-420, -1900], [-240, -1900], [-560, -1760]],
            },
            "CT": {
                "center": [380, 1420, 0],
                "size": [800, 440],
                "facing": [0, -1],
                "slots": [[60, 1410], [240, 1410], [420, 1410], [600, 1410], [380, 1540]],
            },
        },
        "buy_zones": [
            {"team": "T", "center": [-565, -1925, 0], "size": [730, 470]},
            {"team": "CT", "center": [380, 1425, 0], "size": [780, 390]},
        ],
        "bombsites": [
            {"id": "A", "center": [1500, 1566, 128], "size": [740, 650], "plant_radius": 64},
            {"id": "B", "center": [-1506, 1771, 64], "size": [740, 580], "plant_radius": 64},
        ],
        # sites 是给 schema v3 消费端的稳定别名（bombsites 保留兼容雷达/旧工具）。
        "sites": {
            "A": {"center": [1500, 1566, 128], "size": [740, 650], "plant_radius": 64},
            "B": {"center": [-1506, 1771, 64], "size": [740, 580], "plant_radius": 64},
        },
        # buildings 保留给 geom.derive 的建筑体（本项目由墙盒直接派生，故为空）；
        # building_groups 是设计台账的 14 个墙/房间分组，供 checks.min_buildings 校验。
        "buildings": [],
        "building_groups": building_groups,
        "doorways": doorways,
        "structures": structures,
        "lanes": [
            {"id": "A_long", "width": 240, "points": [[-420, -1900], [660, -1800], [990, -1560], [1160, -1300], [1160, -400], [1290, -210], [1460, -100], [1460, 520], [1360, 860], [1300, 1100], [1470, 1450]]},
            {"id": "A_short", "width": 220, "points": [[-230, -1700], [-20, -1440], [-20, -760], [220, -580], [300, -300], [300, 140], [600, 100], [830, 280], [830, 660], [1000, 840], [1070, 1120], [1080, 1250]]},
            {"id": "B_tunnel", "width": 240, "points": [[-780, -1760], [-1000, -1300], [-1040, -400], [-980, -60], [-700, -40], [-1000, 600], [-1010, 1300], [-1100, 1450], [-1350, 1700]]},
            {"id": "CT_A", "width": 220, "points": [[380, 1320], [860, 1480], [1100, 1400], [1300, 1290]]},
            {"id": "CT_B", "width": 180, "points": [[300, 1240], [-150, 1480], [-640, 1590], [-1000, 1640], [-1240, 1700]]},
            {"id": "CT_mid", "width": 400, "points": [[150, 1200], [300, 1090], [320, 900], [320, 720]]},
            {"id": "T_mid", "width": 400, "points": [[-420, -1640], [-100, -1420], [-60, -900], [120, -600], [260, -300], [260, 100], [320, 500], [320, 700]]},
            {"id": "B_loop", "width": 200, "points": [[-1010, 1200], [-950, 700], [-780, -100], [-560, -520], [-300, -620], [-60, -620]]},
        ],
        "choke_points": [
            {"id": "long_door", "center": [960, -1560], "width": 170},
            {"id": "mid_door", "center": [320, 700], "width": 200},
            {"id": "b_door", "center": [-700, 1600], "width": 110},
            {"id": "a_short", "center": [1080, 1210], "width": 200},
            {"id": "b_tunnel", "center": [-1020, 1240], "width": 128},
        ],
        "sightlines": [],
        "props": props,
        "rotation_seconds_target": {},
        "checks": scene_contract.checker_spec(),
    }

def _write_outputs(layout):
    LAYOUT_OUT.write_text(json.dumps(layout, ensure_ascii=False, indent=1), encoding="utf-8")
    data = gen_blockout.build(layout)
    data["schema"] = "de-ash-boxes/2"
    data["source"] = "tools/mapgen/import_scene_v3.py"
    BOXES_OUT.parent.mkdir(parents=True, exist_ok=True)
    BOXES_OUT.write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")
    kinds = {}
    for box in data["boxes"]:
        kinds[box["kind"]] = kinds.get(box["kind"], 0) + 1
    print(f"wrote {LAYOUT_OUT.relative_to(ROOT)} structures={len(layout['structures'])} "
          f"props={len(layout['props'])}")
    print(f"wrote {BOXES_OUT.relative_to(ROOT)} boxes={data['count']} kinds={kinds}")

def main() -> int:
    rows = load_rows()
    building_groups, structures, props, _ = _collect_elements(rows)
    layout = _build_layout(structures, props, building_groups,
                           scene_contract.doorway_rows(rows, rect_of))
    _apply_shell(layout, rows)
    _fill_rotation(layout)
    _write_outputs(layout)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
