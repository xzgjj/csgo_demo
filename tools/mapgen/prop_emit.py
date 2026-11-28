"""道具落盒细节：长栏杆分节与单盒记录构造（供 import_scene_v3 调用）。

长条金属栏杆不能整块输出，否则画面上是一面“无细节的黑色长方形”；
按 ~120u 分节、留 4u 缝：缝小于身宽 32u，碰撞仍连续，视觉读得出栏杆段。
"""

from __future__ import annotations

RAILING_MIN_LONG = 320.0
RAILING_MAX_THICK = 24.0
RAILING_PANEL = 120.0
RAILING_GAP = 4.0


def stats(row):
    """返回 (height, hp, dent_only)：门洞抬到 220u；金属薄件只凹不碎（保留碰撞）。"""
    height = float(row["height_u"])
    if row["type"] == "door":
        height = max(height, 220.0)
    hp = int(row["hp"]) if row["breakable"] == "yes" and row["hp"] else 0
    # A-11 薄金属按 docs/design/03 §3.4「只凹不碎，碰撞保留」：有耐久但不移除碰撞。
    dent_only = row["surface"] == "metal" and row["type"] == "prop" and hp > 0
    return height, hp, dent_only


def _panel_record(row, center, size, material, hp):
    return {
        "id": row["id"],
        "kind": "prop",
        "center": list(center),
        "size": list(size),
        "material": material,
        "walkable": False,
        "collision": True,
        "breakable": row["breakable"] == "yes",
        "hp": hp,
        "kit": row.get("reuse", ""),
    }


def split_railing(row, bounds, base, material, hp):
    """长栏杆返回分节盒列表；不是长栏杆时返回 None，交给普通单盒路径。"""
    x0, y0, x1, y1 = bounds
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    sx, sy = x1 - x0, y1 - y0
    long_side = max(sx, sy)
    if row["type"] != "railing" or long_side < RAILING_MIN_LONG:
        return None
    if min(sx, sy) > RAILING_MAX_THICK:
        return None
    count = max(3, int(round(long_side / RAILING_PANEL)))
    panel = (long_side - RAILING_GAP * (count - 1)) / count
    horizontal = sx >= sy
    height = float(row["height_u"])
    records = []
    for index in range(count):
        offset = index * (panel + RAILING_GAP) + panel * 0.5
        px, py = (x0 + offset, cy) if horizontal else (cx, y0 + offset)
        size = (panel, sy, height) if horizontal else (sx, panel, height)
        record = _panel_record(row, (px, py, base), size, material, hp)
        record["id"] = f"{row['id']}_{index + 1}"
        records.append(record)
    return records


def prop_box(row, bounds, base, material, height, hp, dent_only):
    x0, y0, x1, y1 = bounds
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    record = _panel_record(row, (cx, cy, base), (x1 - x0, y1 - y0, height), material,
                           hp)
    record["walkable"] = bool(row["enterable"] == "yes" and height <= 96)
    record["breakable"] = row["breakable"] == "yes" and not dent_only
    record["hp"] = 0 if dent_only else hp
    return record
