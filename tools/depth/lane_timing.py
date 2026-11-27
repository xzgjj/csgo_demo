#!/usr/bin/env python3
"""DEP-04：地图节奏报告（三路单程时间 / 差值 / 入口宽度）。

用法：
    python -X utf8 tools/depth/lane_timing.py [--map content/maps/de_ash.layout.json]
                                              [--csv log/verify/depth/lane_timing.csv]
只读 layout，不修改地图；三路默认取 A_long / mid / B_long，任一路缺失或时间差 <3s 视为 FAIL。
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RUN_SPEED = 220.0  # units/s，与 check_layout.py 的 RUN_SPEED 保持一致
ROUTES = ("A_long", "mid", "B_long")
MIN_SPREAD_S = 3.0
MIN_WIDTH = 96.0


def lane_seconds(lane: dict) -> float:
    points = lane.get("points", [])
    total = sum(math.dist(points[i], points[i + 1]) for i in range(len(points) - 1))
    return total / RUN_SPEED


def collect(layout: dict) -> tuple[list[dict], list[str]]:
    issues: list[str] = []
    lanes = {str(lane.get("id")): lane for lane in layout.get("lanes", [])}
    rows: list[dict] = []
    for lane_id, lane in lanes.items():
        width = float(lane.get("width", 0))
        if width < MIN_WIDTH:
            issues.append(f"{lane_id}:入口宽 {width:.0f} < {MIN_WIDTH:.0f}")
        rows.append({"id": lane_id, "width": width, "seconds": round(lane_seconds(lane), 2)})
    for site in ("A", "B"):
        entries = [r for r in rows if r["id"].startswith(f"{site}_")]
        if len(entries) < 2:
            issues.append(f"{site} 点入口 {len(entries)} < 2")
    missing = [route for route in ROUTES if route not in lanes]
    if missing:
        issues.append(f"缺少三路 lane: {missing}")
        return rows, issues
    spread = max(lane_seconds(lanes[r]) for r in ROUTES) - min(
        lane_seconds(lanes[r]) for r in ROUTES
    )
    if spread < MIN_SPREAD_S:
        issues.append(f"三路 timing 差 {spread:.1f}s < {MIN_SPREAD_S:.1f}s")
    return rows, issues


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--map", default="content/maps/de_ash.layout.json")
    parser.add_argument("--csv", default="log/verify/depth/lane_timing.csv")
    args = parser.parse_args()
    layout = json.loads((ROOT / args.map).read_text(encoding="utf-8"))
    rows, issues = collect(layout)
    csv_path = ROOT / args.csv
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=["id", "width", "seconds"])
        writer.writeheader()
        writer.writerows(rows)
    times = {r["id"]: r["seconds"] for r in rows}
    print(f"[lane] routes={dict((r, times.get(r)) for r in ROUTES)}")
    print(f"[lane] csv={csv_path}")
    if issues:
        for issue in issues:
            print(f"[lane] FAIL {issue}")
        return 1
    print("[lane] PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
