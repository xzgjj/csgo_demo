"""可玩区连通性与围合判据（检查脚本与围合修复共用同一套栅格算法）。

约定：
- 栅格 GRID=48u（约玩家直径 1.5 倍），从 T 出生做 4 邻域洪水填充；
- 阻挡 = 碰撞盒且顶面 > STEP_LIMIT，且不属于台面/台阶/包点/出生；
- 可玩区 = 走廊（lane 折线按 width/2 外扩）+ 包点/出生/买区矩形；
- 越界 = 可达格到可玩区边界内缩超过 PLAY_TOLERANCE（说明从缝隙漏进虚空）；
- 修复：把越界格按 REPAIR_CELL=192 对齐成块（184 宽，留 8 缝 < 玩家直径），
  再跳过与已有实体重叠的块，由 import_scene_v3 迭代到收敛。
"""

from __future__ import annotations

GRID = 48.0
STEP_LIMIT = 96.0
PLAY_TOLERANCE = 240.0
REPAIR_CELL = 192.0
REPAIR_GAP = 8.0
WALKABLE_KINDS = {"platform", "stairs", "catwalk", "floor", "site", "spawn"}


def rect_of(box) -> tuple[float, float, float, float]:
    cx, cy, _ = box["center"]
    sx, sy, _ = box["size"]
    return cx - sx / 2, cx + sx / 2, cy - sy / 2, cy + sy / 2


def blocks_player(box) -> bool:
    if not box.get("collision", True):
        return False
    if box["kind"] in WALKABLE_KINDS:
        return False
    return box["top"] > STEP_LIMIT


def _seg_distance(px: float, py: float, ax: float, ay: float, bx: float, by: float) -> float:
    dx, dy = bx - ax, by - ay
    length_sq = dx * dx + dy * dy
    if length_sq <= 1e-9:
        return ((px - ax) ** 2 + (py - ay) ** 2) ** 0.5
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length_sq))
    return ((px - (ax + t * dx)) ** 2 + (py - (ay + t * dy)) ** 2) ** 0.5


def corridor_clearance(x: float, y: float, lanes) -> float:
    """点到最近走廊边界的距离；正数表示在走廊内。"""
    best = float("inf")
    for lane in lanes:
        half = lane["width"] / 2.0
        for (ax, ay), (bx, by) in zip(lane["points"], lane["points"][1:]):
            best = min(best, _seg_distance(x, y, ax, ay, bx, by) - half)
    return best


def zone_rects(layout) -> list[tuple[float, float, float, float]]:
    out = []
    for site in layout.get("bombsites", []):
        cx, cy, _ = site["center"]
        sx, sy = site["size"]
        out.append((cx - sx / 2, cx + sx / 2, cy - sy / 2, cy + sy / 2))
    for spawn in layout.get("spawns", {}).values():
        cx, cy, _ = spawn["center"]
        sx, sy = spawn["size"]
        out.append((cx - sx / 2, cx + sx / 2, cy - sy / 2, cy + sy / 2))
    for zone in layout.get("buy_zones", []):
        cx, cy, _ = zone["center"]
        sx, sy = zone["size"]
        out.append((cx - sx / 2, cx + sx / 2, cy - sy / 2, cy + sy / 2))
    return out


def _rect_clearance(x: float, y: float, rect) -> float:
    x0, x1, y0, y1 = rect
    dx = max(x0 - x, 0.0, x - x1)
    dy = max(y0 - y, 0.0, y - y1)
    if x0 <= x <= x1 and y0 <= y <= y1:
        return min(x - x0, x1 - x, y - y0, y1 - y)
    return -((dx * dx + dy * dy) ** 0.5)


def playable_clearance(x: float, y: float, layout) -> float:
    best = corridor_clearance(x, y, layout["lanes"])
    for rect in zone_rects(layout):
        best = max(best, _rect_clearance(x, y, rect))
    return best


def _free_grid(layout, blockers, grid: float):
    lo, hi = layout["bounds"]["min"], layout["bounds"]["max"]
    nx = int((hi[0] - lo[0]) // grid) + 1
    ny = int((hi[1] - lo[1]) // grid) + 1
    free = [[True] * ny for _ in range(nx)]
    for i in range(nx):
        cx = lo[0] + grid * i
        for j in range(ny):
            cy = lo[1] + grid * j
            for box in blockers:
                x0, x1, y0, y1 = rect_of(box)
                if x0 - 12 <= cx <= x1 + 12 and y0 - 12 <= cy <= y1 + 12:
                    free[i][j] = False
                    break
    return free, nx, ny, lo


def _spawn_start(layout, free, grid: float, lo, nx: int, ny: int):
    spawn = layout["spawns"]["T"]["center"]
    for radius in range(10):
        for i in range(nx):
            for j in range(ny):
                if not free[i][j]:
                    continue
                cx = lo[0] + grid * i
                cy = lo[1] + grid * j
                if abs(cx - spawn[0]) <= grid * radius and abs(cy - spawn[1]) <= grid * radius:
                    return i, j
    return None


def _flood(free, start, nx: int, ny: int) -> set:
    seen = {start}
    queue = [start]
    while queue:
        i, j = queue.pop()
        for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ni, nj = i + di, j + dj
            if 0 <= ni < nx and 0 <= nj < ny and free[ni][nj] and (ni, nj) not in seen:
                seen.add((ni, nj))
                queue.append((ni, nj))
    return seen


def reachable(layout, boxes, grid: float = GRID):
    """从 T 出生做 BFS；返回 (cells, nx, ny, lo)。"""
    blockers = [b for b in boxes if blocks_player(b)]
    free, nx, ny, lo = _free_grid(layout, blockers, grid)
    start = _spawn_start(layout, free, grid, lo, nx, ny)
    if start is None:
        return set(), nx, ny, lo
    return _flood(free, start, nx, ny), nx, ny, lo



def cell_near(cells, lo, target, radius_cells: int = 8) -> bool:
    for i, j in cells:
        if abs(lo[0] + GRID * i - target[0]) <= GRID * radius_cells and \
                abs(lo[1] + GRID * j - target[1]) <= GRID * radius_cells:
            return True
    return False


def leak_cells(layout, boxes) -> list[tuple[int, int]]:
    """可达但已离开可玩区（clearance < -PLAY_TOLERANCE）的格子。"""
    cells, _, _, lo = reachable(layout, boxes)
    out = []
    for i, j in cells:
        if playable_clearance(lo[0] + GRID * i, lo[1] + GRID * j, layout) < -PLAY_TOLERANCE:
            out.append((i, j))
    return out


def _overlaps_existing(x: float, y: float, size: float, boxes) -> bool:
    rect = (x - size / 2, x + size / 2, y - size / 2, y + size / 2)
    for box in boxes:
        if not box.get("collision", True):
            continue
        other = rect_of(box)
        if not (rect[1] <= other[0] or other[1] <= rect[0]
                or rect[3] <= other[2] or other[3] <= rect[2]):
            return True
    return False


def repair_blocks(layout, boxes) -> list[dict]:
    """把越界可达格按 192 对齐成块；跳过与已有实体重叠的位置。"""
    cells, _, _, lo = reachable(layout, boxes)
    seen_cells: set[tuple[int, int]] = set()
    blocks: list[dict] = []
    size = REPAIR_CELL - REPAIR_GAP
    for i, j in cells:
        cx = lo[0] + GRID * i
        cy = lo[1] + GRID * j
        if playable_clearance(cx, cy, layout) >= -PLAY_TOLERANCE:
            continue
        key = (int(cx // REPAIR_CELL), int(cy // REPAIR_CELL))
        if key in seen_cells:
            continue
        seen_cells.add(key)
        bx = (key[0] + 0.5) * REPAIR_CELL
        by = (key[1] + 0.5) * REPAIR_CELL
        if _overlaps_existing(bx, by, size, boxes):
            block_size = 96.0
        else:
            block_size = size
        blocks.append({
            "id": "shell_repair_%d_%d" % (int(bx), int(by)),
            "kind": "wall",
            "center": [bx, by],
            "size": [block_size, block_size],
            "height": 288,
            "base_z": 0.0,
            "material": "concrete",
        })
    return blocks
