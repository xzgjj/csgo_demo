"""坡道行的解析与落位：从 CSV 行算出斜面方向，产出一条 ramp prop。

坡道是独立图元（不进墙体碰撞），内核用 Ramp 结构接管支撑，Blender 出楔形网格。
"""


def rect_of(row):
    return (int(row["rect_x1"]), int(row["rect_y1"]),
            int(row["rect_x2"]), int(row["rect_y2"]))


def slope_of(row):
    """上升方向：CSV 的 slope 列（x+/x-/y+/y-）；缺省按长边上升，便于旧行兼容。"""
    value = str(row.get("slope") or "").strip().lower()
    if value in {"x+", "x-", "y+", "y-"}:
        return value[0], value[1] == "+"
    x0, y0, x1, y1 = rect_of(row)
    axis = "x" if (x1 - x0) >= (y1 - y0) else "y"
    return axis, True


def emit_ramp(row, props, material_of):
    """坡道：一节斜面盒（不切台阶）。高度取 height_u，底面贴 0。"""
    x0, y0, x1, y1 = rect_of(row)
    axis, ascending = slope_of(row)
    props.append({
        "id": row["id"],
        "kind": "ramp",
        "center": [(x0 + x1) / 2.0, (y0 + y1) / 2.0, 0.0],
        "size": [x1 - x0, y1 - y0, float(row["height_u"])],
        "material": material_of(row["surface"]),
        "walkable": True,
        "slope": {"axis": axis, "ascending": ascending},
    })
