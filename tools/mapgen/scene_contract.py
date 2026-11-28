"""de_dust2 场景契约小件：窗玻璃片、门洞清单。

单独成文件：import_scene_v3 保持 <400 行（CI 复杂度预算），且这些字段是
schema v3 / check_dust2 共同消费的契约，不依赖布局主体。
"""

from __future__ import annotations


def emit_window(row, rect_of, surface_top, platforms, props) -> None:
    """窗 = 墙洞里的玻璃片（96..min(192, 墙高)）；破前挡视线/挡路，破后碰撞盒移除。"""
    x0, y0, x1, y1 = rect_of(row)
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    along_x = (x1 - x0) >= (y1 - y0)
    pane_x, pane_y = (x1 - x0, 8.0) if along_x else (8.0, y1 - y0)
    base = surface_top(platforms, cx, cy)
    sill = base + 96.0
    top = min(base + 192.0, base + max(float(row["height_u"]), 128.0))
    props.append({
        "id": row["id"],
        "kind": "prop",
        "center": [cx, cy, sill],
        "size": [pane_x, pane_y, max(32.0, top - sill)],
        "material": "glass",
        "walkable": False,
        "collision": True,
        "breakable": row["breakable"] == "yes",
        "hp": int(row["hp"]) if row["hp"] else 20,
    })


def checker_spec() -> dict:
    """schema v3 checks 阈值（check_dust2 / check_layout 共同读取）。"""
    return {
        "min_buildings": 14,
        "min_lanes": 6,
        "min_solid": 120,
        "max_boxes": 512,
        "door_width": 96,
        "max_door_width": 128,
        "min_site_width": 600,
        "min_site_depth": 580,
        "min_breakables": 13,
        "breakable_hp": [20, 30, 60, 100, 200, 300, 400],
        "layoutcheck": ["bounds", "openings", "clearance", "lanes", "rotation_times",
                        "boxes_sync", "breakables"],
    }


def doorway_rows(rows, rect_of) -> list[dict]:
    out = []
    for row in rows:
        if row["type"] != "doorway":
            continue
        x0, y0, x1, y1 = rect_of(row)
        out.append({
            "id": row["id"],
            "center": [(x0 + x1) / 2.0, (y0 + y1) / 2.0],
            "width": max(abs(x1 - x0), abs(y1 - y0)),
            "note": row["note"],
        })
    return out
