"""地图设计指标（v4 重设计前基线）：用代码量化“一眼看穿/出生街道太空/交战点无掩体”。

指标（阈值可调，输出 PASS/FAIL 与实测值）：
- 最长直视 ≤ 2500u
- 主路 3 条（A / Mid / B）
- 出生点正面直街 ≤ 1200u，出生 700u 内可掩体 ≥ 3
- 出生→包点路径必经 ≥1 choke
- 每个主交战点 256u 内掩体 ≥ 2
- 双方到每个交战点/包点都有时间（tick→秒，速度 250u/s），并给出首接触时间差
运行：python -X utf8 tools/mapgen/check_design_metrics.py
"""

from __future__ import annotations

import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAYOUT = ROOT / "content/maps/de_ash.layout.json"
BOXES = ROOT / "host/scenes/de_ash_boxes.json"
OUT = ROOT / "log/map"
SPEED = 250.0
EYE = 64.0
BLOCK_KINDS = {"floor", "roof", "site", "spawn"}


def box_aabb(box):
    cx, cy, cz = box["center"]
    sx, sy, sz = box["size"]
    return (cx - sx / 2, cy - sy / 2, cz - sz / 2, cx + sx / 2, cy + sy / 2, cz + sz / 2)


def los_blocked(boxes, a, b):
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    for box in boxes:
        if box["kind"] in BLOCK_KINDS and box["kind"] != "roof":
            pass
        x0, y0, z0, x1, y1, z1 = box_aabb(box)
        if z1 <= EYE + 2 or z0 >= EYE + 60 or (x1 < min(ax, bx) or x0 > max(ax, bx)) or (
            y1 < min(ay, by) or y0 > max(ay, by)
        ):
            continue
        if box["kind"] in ("floor", "site", "spawn"):
            continue
        # 线段-AABB（2D）
        tmin, tmax = 0.0, 1.0
        ok = True
        for p, d, lo, hi in ((ax, dx, x0, x1), (ay, dy, y0, y1)):
            if abs(d) < 1e-9:
                ok = ok and lo <= p <= hi
                continue
            t1, t2 = (lo - p) / d, (hi - p) / d
            if t1 > t2:
                t1, t2 = t2, t1
            tmin, tmax = max(tmin, t1), min(tmax, t2)
            if tmin > tmax:
                ok = False
                break
        if ok and tmax >= 0 and tmin <= 1:
            return True
    return False


def load():
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    boxes = json.loads(BOXES.read_text(encoding="utf-8"))["boxes"]
    # boxes 存的是 Godot 坐标 (-y, z, -x)，换回 sim 坐标 (x, y, z)。
    for box in boxes:
        cx, cy, cz = box["center"]
        sx, sy, sz = box["size"]
        box["center"] = [-cz, -cx, cy]
        box["size"] = [sz, sx, sy]
    return layout, boxes


def lane_points(lane):
    return [tuple(p) for p in lane["points"]]


def dist_along(points, target):
    best = 1e9
    for p in points:
        best = min(best, math.dist(p, target))
    return best


def main() -> int:
    layout, boxes = load()
    spawns = layout["spawns"]
    chokes = layout["choke_points"]
    sites = layout["bombsites"]
    lanes = {lane["id"]: lane_points(lane) for lane in layout["lanes"]}
    sightlines = layout["sightlines"]
    routes = {"A": ["A_long", "A_short"], "Mid": ["mid"], "B": ["B_long", "B_short"]}
    metrics = {"lanes": {}, "spawn": {}, "chokes": [], "sites": [], "sightlines": []}

    # 主路
    for route, ids in routes.items():
        present = [i for i in ids if i in lanes]
        metrics["lanes"][route] = {"lanes": present, "chokes": 0,
                                   "length": round(sum(
                                       math.dist(p, q) for p, q in zip(lanes[present[0]],
                                                                      lanes[present[0]][1:])), 1)
                                   if present else 0}
    for choke in chokes:
        c = (choke["center"][0], choke["center"][1])
        for route, ids in routes.items():
            if any(dist_along(lanes[i], c) <= 320 for i in ids if i in lanes):
                metrics["lanes"][route]["chokes"] += 1

    # 出生点：正面直街长度 / 掩体数 / 出口
    for team, spawn in spawns.items():
        cx, cy = spawn["center"][0], spawn["center"][1]
        fx, fy = spawn["facing"]
        exit_point = (cx + fx * 150, cy + fy * 150)
        straight = 0.0
        while straight < 3000:
            a = (exit_point[0], exit_point[1] + fy * straight)
            b = (a[0], a[1] + fy * 64)
            if a[0] != exit_point[0]:
                a = (exit_point[0] + fx * straight, exit_point[1])
                b = (a[0] + fx * 64, a[1])
            if los_blocked(boxes, a, b):
                break
            straight += 64
        covers = 0
        for box in boxes:
            x0, y0, z0, x1, y1, z1 = box_aabb(box)
            if box["kind"] in ("floor", "spawn", "site", "roof"):
                continue
            if z1 > 96 or z0 >= 32:
                continue
            if math.dist(((x0 + x1) / 2, (y0 + y1) / 2), exit_point) <= 700:
                covers += 1
        metrics["spawn"][team] = {"exit": [round(v) for v in exit_point],
                                  "straight": round(straight), "covers_700": covers,
                                  "chokes_1500": sum(
                                      1 for ch in chokes
                                      if math.dist((ch["center"][0], ch["center"][1]),
                                                   exit_point) <= 1500)}

    # 交战点：时间（T/CT 欧氏距离 / 速度）、掩体数
    def time_from(spawn_center, point):
        return math.dist((spawn_center[0], spawn_center[1]), point) / SPEED

    for choke in chokes:
        c = (choke["center"][0], choke["center"][1])
        covers = 0
        for box in boxes:
            x0, y0, z0, x1, y1, z1 = box_aabb(box)
            if box["kind"] in ("floor", "spawn", "site", "roof", "lintel"):
                continue
            if z1 > 96 or z0 >= 32:
                continue
            if math.dist(((x0 + x1) / 2, (y0 + y1) / 2), c) <= 256:
                covers += 1
        metrics["chokes"].append({
            "id": choke["id"], "width": choke["passage_width"], "covers_256": covers,
            "t_time": round(time_from(spawns["T"]["center"], c), 2),
            "ct_time": round(time_from(spawns["CT"]["center"], c), 2),
        })
    for site in sites:
        c = (site["center"][0], site["center"][1])
        metrics["sites"].append({
            "id": site["id"],
            "t_time": round(time_from(spawns["T"]["center"], c), 2),
            "ct_time": round(time_from(spawns["CT"]["center"], c), 2),
        })

    # 视线：已声明 + 主路内采样
    for sl in sightlines:
        a, b = tuple(sl["from"]), tuple(sl["to"])
        blocked = los_blocked(boxes, a, b)
        metrics["sightlines"].append({"id": sl["id"], "length": round(math.dist(a, b)),
                                      "expect": sl["expect"],
                                      "blocked": blocked, "ok": (blocked == (sl["expect"] == "blocked"))})
    samples = []
    for lane_id, pts in lanes.items():
        for i in range(0, len(pts), 2):
            for j in range(i + 2, len(pts), 2):
                if not los_blocked(boxes, pts[i], pts[j]):
                    samples.append((round(math.dist(pts[i], pts[j])), lane_id, pts[i], pts[j]))
    samples.sort(reverse=True)
    longest = samples[0] if samples else (0, "", (), ())

    # 阈值判定
    checks = {
        "longest_sightline<=2500": longest[0] <= 2500,
        "three_main_routes": all(metrics["lanes"][r]["lanes"] for r in ("A", "Mid", "B")),
        "spawn_straight<=1200(T)": metrics["spawn"]["T"]["straight"] <= 1200,
        "spawn_straight<=1200(CT)": metrics["spawn"]["CT"]["straight"] <= 1200,
        "spawn_cover>=3": all(metrics["spawn"][t]["covers_700"] >= 3 for t in ("T", "CT")),
        "spawn->site_choke": all(m["chokes"] >= 1 for m in metrics["lanes"].values()),
        "engagement_cover>=2": all(c["covers_256"] >= 2 for c in metrics["chokes"]),
        "engagement_times_both": all(c["t_time"] > 0 and c["ct_time"] > 0 for c in metrics["chokes"])
        and all(s["t_time"] > 0 and s["ct_time"] > 0 for s in metrics["sites"]),
        "sightlines_match": all(s["ok"] for s in metrics["sightlines"]),
    }

    report = {"longest": {"length": longest[0], "lane": longest[1], "from": longest[2],
                          "to": longest[3]},
              "spawn": metrics["spawn"], "routes": metrics["lanes"],
              "chokes": metrics["chokes"], "sites": metrics["sites"],
              "checks": checks}
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "design_metrics.json").write_text(json.dumps(report, ensure_ascii=False, indent=2),
                                             encoding="utf-8")
    lines = ["# de_ash 设计指标（当前基线）", "",
             f"- 最长直视：**{longest[0]}u**（{longest[1]}）",
             f"- 出生街直段：T {metrics['spawn']['T']['straight']}u / CT {metrics['spawn']['CT']['straight']}u",
             f"- 出生 700u 掩体：T {metrics['spawn']['T']['covers_700']} / CT {metrics['spawn']['CT']['covers_700']}",
             "", "## 主路", "| 路线 | 子路 | choke 数 |", "|---|---|---|"]
    for route, item in metrics["lanes"].items():
        lines.append(f"| {route} | {', '.join(item['lanes'])} | {item['chokes']} |")
    lines += ["", "## 交战点（T/CT 到达时间 + 256u 内掩体）", "| 交战点 | 宽度 | T 秒 | CT 秒 | 掩体 |",
              "|---|---|---|---|---|"]
    for choke in metrics["chokes"]:
        lines.append(f"| {choke['id']} | {choke['width']} | {choke['t_time']} | "
                     f"{choke['ct_time']} | {choke['covers_256']} |")
    lines += ["", "| 包点 | T 秒 | CT 秒 |", "|---|---|---|"]
    for site in metrics["sites"]:
        lines.append(f"| {site['id']} | {site['t_time']} | {site['ct_time']} |")
    lines += ["", "## 检查", "| 指标 | 结果 |", "|---|---|"]
    for name, ok in checks.items():
        lines.append(f"| {name} | {'PASS' if ok else 'FAIL'} |")
    (OUT / "design_metrics.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
