"""de_dust2 围合：按「设计可行走矩形并集」实体化墙体，封死非路径空地。

数据源是手工台账 `scene-elements.csv` 的可走行（floor/room/spawn/...），不是 lane 中线。
算法：64u 栅格标记「与任一可走矩形相交」的格子为通道，其余格子实心；再把实心格贪心合并成
轴对齐墙盒（行列合并 + 同 y 区间横向合并）。墙只会在通道边界外生成，因此：
- 玩家只能走设计路径，空地全部变成实体（不再出现大块空场）；
- 窄连接/门洞不会被自己的墙堵住（标记按“有交集”膨胀，只会让通道略宽）。
"""

from __future__ import annotations

import math

CELL = 64              # 栅格尺寸：墙厚/台阶步长
WALL_HEIGHT = 512      # 主体围合高度：高过 256 上限层，挡住远景空场
SHELL_THICKNESS = 64   # 外圈边界墙厚
WALKABLE_TYPES = {
    "floor", "room", "tunnel", "spawn", "buyzone", "choke", "step",
    "platform", "ramp", "catwalk", "stairs", "pit", "doorway", "window",
}


def _rect(row) -> tuple[int, int, int, int]:
    return (int(row["rect_x1"]), int(row["rect_y1"]),
            int(row["rect_x2"]), int(row["rect_y2"]))


def _walk_mask(rows, layout: dict, lo: int, hi: int) -> list[list[bool]]:
    """格子与任一可走矩形有交集，或中心落在 lane 走廊内，即视为通道。"""
    rects = [_rect(row) for row in rows if row["type"] in WALKABLE_TYPES]
    lanes = layout.get("lanes", [])
    cells = (hi - lo) // CELL
    grid = [[False] * cells for _ in range(cells)]
    for j in range(cells):
        y0, y1 = lo + j * CELL, lo + (j + 1) * CELL
        cy = (y0 + y1) / 2.0
        for i in range(cells):
            x0, x1 = lo + i * CELL, lo + (i + 1) * CELL
            cx = (x0 + x1) / 2.0
            if any(not (x1 <= r[0] or r[2] <= x0 or y1 <= r[1] or r[3] <= y0)
                   for r in rects):
                grid[j][i] = True
            elif any(_near_lane(cx, cy, lane) for lane in lanes):
                grid[j][i] = True
    return grid


def _near_lane(x: float, y: float, lane: dict) -> bool:
    half = lane["width"] / 2.0 + CELL / 2.0
    for (ax, ay), (bx, by) in zip(lane["points"], lane["points"][1:]):
        dx, dy = bx - ax, by - ay
        length_sq = dx * dx + dy * dy
        if length_sq <= 1e-9:
            if math.hypot(x - ax, y - ay) <= half:
                return True
            continue
        t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / length_sq))
        if math.hypot(x - (ax + t * dx), y - (ay + t * dy)) <= half:
            return True
    return False


def _run_end(grid, used, row: int, col: int, cells: int) -> int:
    """该行从 col 起连续未占用实心格的右端（不含）。"""
    while col < cells and not grid[row][col] and not used[row][col]:
        col += 1
    return col


def _rows_free(grid, used, top: int, left: int, right: int, cells: int) -> int:
    """整段 [left,right) 都未占用实心格的下端（不含）。"""
    bottom = top
    while bottom < cells and all(
            not grid[bottom][col] and not used[bottom][col] for col in range(left, right)):
        bottom += 1
    return bottom


def _solid_rects(grid: list[list[bool]]) -> list[tuple[int, int, int, int]]:
    """贪心不重叠矩形：从每个未占用实心格向右/向下扩展，整块标记占用。"""
    cells = len(grid)
    used = [[False] * cells for _ in range(cells)]
    rects: list[tuple[int, int, int, int]] = []
    for j in range(cells):
        i = 0
        while i < cells:
            if grid[j][i] or used[j][i]:
                i += 1
                continue
            right = _run_end(grid, used, j, i, cells)
            bottom = _rows_free(grid, used, j + 1, i, right, cells)
            for row in range(j, bottom):
                for col in range(i, right):
                    used[row][col] = True
            rects.append((i, j, right, bottom))
            i = right
    return rects


def route_enclosure(rows, layout: dict) -> list[dict]:
    lo, hi = int(layout["bounds"]["min"][0]), int(layout["bounds"]["max"][0])
    grid = _walk_mask(rows, layout, lo, hi)
    out: list[dict] = []
    for (x0, y0, x1, y1) in _solid_rects(grid):
        world = (lo + x0 * CELL, lo + y0 * CELL, lo + x1 * CELL, lo + y1 * CELL)
        out.append({
            "id": "shell_enclose_%d_%d" % (world[0], world[1]),
            "kind": "wall",
            "center": [(world[0] + world[2]) / 2.0, (world[1] + world[3]) / 2.0],
            "size": [world[2] - world[0], world[3] - world[1]],
            "height": WALL_HEIGHT,
            "base_z": 0.0,
            "material": "stone",
        })
        out.extend(_upper_storey(world))
    return out


def _upper_storey(world) -> list[dict]:
    """大体块加一层退台屋顶，形成高低错落的建筑天际线（不是一堵等高的长墙）。"""
    width, depth = world[2] - world[0], world[3] - world[1]
    if width < 192 or depth < 192:
        return []
    inset = 24.0
    level = 64 + 32 * ((world[0] // 128 + world[1] // 128) % 3)
    return [{
        "id": "shell_upper_%d_%d" % (world[0], world[1]),
        "kind": "wall",
        "center": [(world[0] + world[2]) / 2.0, (world[1] + world[3]) / 2.0, WALL_HEIGHT + level / 2.0],
        "size": [width - inset * 2, depth - inset * 2, level],
        "height": level,
        "base_z": WALL_HEIGHT,
        "material": "stone" if (world[0] + world[1]) % 256 else "concrete",
        "collision": False,  # 屋顶层在玩家头顶以上，只做视觉天际线，不进碰撞查询
    }]


def perimeter(layout: dict) -> list[dict]:
    """四条边界墙：内表面与 bounds 对齐，板厚 64、高 288。"""
    lo, hi = layout["bounds"]["min"], layout["bounds"]["max"]
    out: list[dict] = []
    spans = [
        ("shell_west", (lo[0], lo[1], lo[0] + SHELL_THICKNESS, hi[1])),
        ("shell_east", (hi[0] - SHELL_THICKNESS, lo[1], hi[0], hi[1])),
        ("shell_south", (lo[0], lo[1], hi[0], lo[1] + SHELL_THICKNESS)),
        ("shell_north", (lo[0], hi[1] - SHELL_THICKNESS, hi[0], hi[1])),
    ]
    for name, (x0, y0, x1, y1) in spans:
        out.append({
            "id": name,
            "kind": "wall",
            "center": [(x0 + x1) / 2.0, (y0 + y1) / 2.0],
            "size": [x1 - x0, y1 - y0],
            "height": WALL_HEIGHT,
            "base_z": 0.0,
            "material": "stone",
        })
    return out


def build(layout: dict, rows=None) -> list[dict]:
    """rows 必须传：按手工台账的可走矩形围合；不传时只保留外圈边界。"""
    items = perimeter(layout)
    if rows is not None:
        items += route_enclosure(rows, layout)
    return sorted(items, key=lambda item: item["id"])


def is_shell(item: dict) -> bool:
    return str(item.get("id", "")).startswith("shell_")
