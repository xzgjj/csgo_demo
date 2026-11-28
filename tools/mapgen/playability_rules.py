"""包点可玩性规则：铺地、台阶、车道遮挡、掩体尺度、出生槽、坡道。

纯几何审计，只读 layout/boxes，不写任何游戏数据；阈值与内核契约对齐：
可迈高差 32u（kStepHeight）、坡道坡度 <=45°（MapError::SlopeTooSteep）。
"""

import math

CELL = 32.0               # 包点采样格边长
STEP_MAX = 32.0           # 可直接迈上的高度
WALK_BLOCK_TOP = 40.0     # 高于这个顶面才挡人（低于它可踩上去）
LANE_BLOCK_RATIO = 0.5    # 盒横宽超过车道宽度的比例才算「拦路」
SITE_RADIUS = 320.0       # 「包点附近」搜索半径
SLAB_MIN_LEN = 96.0       # 细长板长边下限（>=96 且短边 <=24 视为无意义长条）
SLAB_MAX_THICK = 24.0
LONG_STAIR_STEPS = 3      # >=3 级且跨度 >=256u 视为「长台阶」
LONG_STAIR_RUN = 256.0
MAX_SLOPE_DEGREES = 45.0  # 与内核坡道校验同一门槛
LANDING_GAP = 24.0        # 坡顶与台面允许的水平缝


def footprint(box):
    cx, _, cz = (float(v) for v in box["center"])
    sx, _, sz = (abs(float(v)) for v in box["size"])
    return (cx - sx / 2.0, cx + sx / 2.0, cz - sz / 2.0, cz + sz / 2.0)


def top_of(box):
    return float(box["center"][1]) + abs(float(box["size"][1])) / 2.0


def bottom_of(box):
    return float(box["center"][1]) - abs(float(box["size"][1])) / 2.0


def contains(fp, x, z):
    return fp[0] <= x <= fp[1] and fp[2] <= z <= fp[3]


def site_rect(layout, site_id):
    site = layout["sites"][site_id]
    cx, cz, level = (float(v) for v in site["center"])
    sx, sz = (float(v) for v in site["size"])
    return (cx - sx / 2.0, cx + sx / 2.0, cz - sz / 2.0, cz + sz / 2.0, level)


def check_paving(layout, boxes):
    """包点地面在自身标高上是否连续：有坑就会掉下去被卡住。"""
    issues = []
    for site_id in ("A", "B"):
        x0, x1, z0, z1, level = site_rect(layout, site_id)
        solid = [b for b in boxes if b.get("collision") and top_of(b) >= level - 2.0]
        holes = []
        steps_x = int(math.floor((x1 - x0) / CELL))
        steps_z = int(math.floor((z1 - z0) / CELL))
        for i in range(steps_x):
            x = x0 + (i + 0.5) * CELL
            for j in range(steps_z):
                z = z0 + (j + 0.5) * CELL
                if not any(contains(footprint(b), x, z) for b in solid):
                    holes.append([round(x, 1), round(z, 1)])
        cells = max(1, steps_x * steps_z)
        issues.append({"site": site_id, "level": level, "cells": cells,
                       "holes": len(holes), "coverage": round(1.0 - len(holes) / cells, 4),
                       "sample_holes": holes[:8]})
    return issues


def _stair_links(stairs):
    """相邻（缝 <=24）且落差 <=32 的台阶对：并查集把它们聚成一条坡级链。"""
    links = []
    for i, a in enumerate(stairs):
        fa = footprint(a)
        for j in range(i + 1, len(stairs)):
            b = stairs[j]
            fb = footprint(b)
            gap_x = max(0.0, max(fa[0], fb[0]) - min(fa[1], fb[1]))
            gap_z = max(0.0, max(fa[2], fb[2]) - min(fa[3], fb[3]))
            rise = top_of(b) - top_of(a)
            if math.hypot(gap_x, gap_z) <= LANDING_GAP and 0.0 < abs(rise) <= STEP_MAX:
                links.append((i, j))
    return links


def _stair_chains(boxes):
    stairs = [b for b in boxes if b["kind"] == "stairs"]
    parent = list(range(len(stairs)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for i, j in _stair_links(stairs):
        parent[find(i)] = find(j)
    chains = {}
    for i, box in enumerate(stairs):
        chains.setdefault(find(i), []).append(box)
    return {f"chain_{key}": chain for key, chain in chains.items()}


def _chain_run(chain):
    spans = [footprint(b) for b in chain]
    x0 = min(s[0] for s in spans)
    x1 = max(s[1] for s in spans)
    z0 = min(s[2] for s in spans)
    z1 = max(s[3] for s in spans)
    return max(x1 - x0, z1 - z0)


def check_stairs(layout, boxes):
    """台阶链：>=3 级且跨度 >=256u 就是用户说的「长台阶」，应改成平滑斜坡。"""
    issues = []
    for key, chain in sorted(_stair_chains(boxes).items()):
        run = _chain_run(chain)
        site = None
        center = chain[0]["center"]
        for site_id in ("A", "B"):
            x0, x1, z0, z1, _ = site_rect(layout, site_id)
            if contains((x0 - SITE_RADIUS, x1 + SITE_RADIUS,
                         z0 - SITE_RADIUS, z1 + SITE_RADIUS), center[0], center[2]):
                site = site_id
        long_run = len(chain) >= LONG_STAIR_STEPS and run >= LONG_STAIR_RUN
        issues.append({"chain": key, "steps": len(chain), "run": round(run, 1),
                       "site": site, "verdict": "long_stairs" if long_run else "steps"})
    return issues


def _narrow_widths(box, lane_dir):
    """盒子在车道法向上的投影宽度（判断它横了多宽）。"""
    fp = footprint(box)
    nx, nz = -lane_dir[1], lane_dir[0]
    corners = [(fp[0], fp[2]), (fp[1], fp[2]), (fp[0], fp[3]), (fp[1], fp[3])]
    proj = [c[0] * nx + c[1] * nz for c in corners]
    return max(proj) - min(proj)


def _severity_of(box, cross, width):
    """拦路盒分级：细长板 = 要撤/换；比车道宽太多 = 先确认是墙；其余是可读掩体。"""
    sx = abs(float(box["size"][0]))
    sz = abs(float(box["size"][2]))
    thin = min(sx, sz) <= SLAB_MAX_THICK
    long = max(sx, sz) >= SLAB_MIN_LEN and top_of(box) - bottom_of(box) >= 64.0
    if thin and long and box.get("material") != "glass":
        return "slab"
    if cross > width * 1.5:
        return "solid"
    return "cover"


def _blocker_record(box, cross, width):
    sx = abs(float(box["size"][0]))
    sz = abs(float(box["size"][2]))
    return {"id": str(box["id"]), "kind": box["kind"],
            "severity": _severity_of(box, cross, width),
            "size": [round(sx, 1), round(sz, 1), round(top_of(box) - bottom_of(box), 1)],
            "cross": round(cross, 1), "lane_width": width,
            "top": round(top_of(box), 1), "samples": 0}


def _scan_lane_segment(ax, az, bx, bz, width, boxes, hits):
    length = math.hypot(bx - ax, bz - az)
    if length <= 1e-6:
        return
    ux, uz = (bx - ax) / length, (bz - az) / length
    steps = max(1, int(length // CELL))
    for k in range(steps + 1):
        x, z = ax + ux * k * CELL, az + uz * k * CELL
        for box in boxes:
            if not box.get("collision") or box["kind"] in ("floor", "site", "spawn"):
                continue
            if bottom_of(box) > STEP_MAX or top_of(box) <= WALK_BLOCK_TOP:
                continue
            if not contains(footprint(box), x, z):
                continue
            cross = _narrow_widths(box, (ux, uz))
            if cross < width * LANE_BLOCK_RATIO:
                continue
            rec = hits.setdefault(str(box["id"]), _blocker_record(box, cross, width))
            rec["samples"] += 1


def check_lane_blockers(layout, boxes):
    """车道中线上的挡人盒：要么撤掉，要么换成可读掩体（灭火箱/石墩）。"""
    issues = []
    for lane in layout["lanes"]:
        pts = [(float(p[0]), float(p[1])) for p in lane["points"]]
        width = float(lane["width"])
        hits = {}
        for (ax, az), (bx, bz) in zip(pts, pts[1:]):
            _scan_lane_segment(ax, az, bx, bz, width, boxes, hits)
        issues.append({"lane": lane["id"], "width": width, "blockers": list(hits.values())})
    return issues


def _cover_entry(box, level):
    sx, _, sz = (abs(float(v)) for v in box["size"])
    h = top_of(box) - level
    return {"id": str(box["id"]), "kind": box["kind"],
            "size": [round(sx, 1), round(sz, 1)], "height": round(h, 1)}


def check_cover(layout, boxes):
    """交战空间里的可读掩体：高度 24~160、底面积 48~400；同时点名「无意义长条」。"""
    issues = []
    for site_id in ("A", "B"):
        x0, x1, z0, z1, level = site_rect(layout, site_id)
        area = (x0 - SITE_RADIUS, x1 + SITE_RADIUS, z0 - SITE_RADIUS, z1 + SITE_RADIUS)
        covers, slabs = [], []
        for box in boxes:
            if not box.get("collision") or box["kind"] in ("floor", "wall", "site", "spawn"):
                continue
            if not contains(area, box["center"][0], box["center"][2]):
                continue
            sx, _, sz = (abs(float(v)) for v in box["size"])
            h = top_of(box) - level
            if 24.0 <= h <= 160.0 and 48.0 <= min(sx, sz) and max(sx, sz) <= 400.0:
                covers.append(_cover_entry(box, level))
            if (max(sx, sz) >= SLAB_MIN_LEN and min(sx, sz) <= SLAB_MAX_THICK
                    and h >= 64.0 and box.get("material") != "glass"):
                slabs.append(_cover_entry(box, level))
        issues.append({"site": site_id, "covers": covers, "slabs": slabs,
                       "cover_count": len(covers), "slab_count": len(slabs)})
    return issues


def _slot_problems(index, x, z, rect, slots):
    """单个出生槽：出框 / 与后槽重叠 / 嵌实体，都算问题。"""
    problems = []
    if not contains(rect, x, z):
        problems.append(f"slot{index} 出框")
    for x2, z2 in slots[index + 1:]:
        if abs(x2 - x) < 1e-6 and abs(z2 - z) < 1e-6:
            problems.append(f"slot{index} 与后槽重叠")
    return problems


def check_spawns(layout, boxes):
    """两侧出生点：5 槽、落在出生矩形内、不嵌实体。"""
    issues = []
    for team in ("T", "CT"):
        spawn = layout["spawns"][team]
        cx, cz, _ = (float(v) for v in spawn["center"])
        sx, sz = (float(v) for v in spawn["size"])
        rect = (cx - sx / 2.0, cx + sx / 2.0, cz - sz / 2.0, cz + sz / 2.0)
        slots = [(float(p[0]), float(p[1])) for p in spawn["slots"]]
        problems = []
        for index, (x, z) in enumerate(slots):
            problems += _slot_problems(index, x, z, rect, slots)
            blocked = [str(b["id"]) for b in boxes
                       if b.get("collision") and top_of(b) > STEP_MAX
                       and contains(footprint(b), x, z)]
            if blocked:
                problems.append(f"slot{index} 嵌 {blocked[0]}")
        issues.append({"team": team, "slots": len(slots), "center": [cx, cz],
                       "rect": [round(v, 1) for v in rect], "problems": problems})
    return issues


def _ramp_slope(box):
    """坡度 = 升高 / 顺坡方向跑长。坡向来自 slope.axis（x/z），缺省取短边（宁可报陡）。"""
    fp = footprint(box)
    run_x, run_z = fp[1] - fp[0], fp[3] - fp[2]
    slope = box.get("slope") or {}
    if slope.get("axis") == "x":
        run = run_x
    elif slope.get("axis") == "z":
        run = run_z
    else:
        run = min(run_x, run_z)
    run = max(1e-6, run)
    return math.degrees(math.atan2(top_of(box) - bottom_of(box), run))


def _ramp_landing(box, boxes):
    """坡顶接得住的台面：水平缝 <=24 且顶面与坡顶等高（±32）。"""
    fp = footprint(box)
    top = top_of(box)
    landing = []
    for other in boxes:
        if other["kind"] in ("ramp", "site", "spawn") or not other.get("collision"):
            continue
        ofp = footprint(other)
        gap_x = max(0.0, max(fp[0], ofp[0]) - min(fp[1], ofp[1]))
        gap_z = max(0.0, max(fp[2], ofp[2]) - min(fp[3], ofp[3]))
        if math.hypot(gap_x, gap_z) <= LANDING_GAP and abs(top_of(other) - top) <= STEP_MAX:
            landing.append(str(other["id"]))
    return landing


def check_ramps(boxes):
    """坡道体检：坡度 <=45°，顶端必须有等高台面接住，否则等于把人推下悬空边。"""
    issues = []
    for box in boxes:
        if box["kind"] != "ramp":
            continue
        slope = _ramp_slope(box)
        landing = _ramp_landing(box, boxes)
        problems = []
        if slope > MAX_SLOPE_DEGREES:
            problems.append(f"坡度过陡 {slope:.1f}° > {MAX_SLOPE_DEGREES:.0f}°")
        if not landing:
            problems.append("顶端无台面接住")
        fp = footprint(box)
        issues.append({"id": str(box["id"]), "center": [round(v, 1) for v in box["center"]],
                       "run": [round(fp[1] - fp[0], 1), round(fp[3] - fp[2], 1)],
                       "rise": round(top_of(box) - bottom_of(box), 1),
                       "slope": round(slope, 1), "landing": landing, "problems": problems})
    return issues
