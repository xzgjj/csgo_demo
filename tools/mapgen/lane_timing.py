"""de_dust2 对线计时（lane timing）：移动 + 180° 转身单程，输出 CSV 证据。

用法：python -X utf8 tools/mapgen/lane_timing.py [--map de_dust2] [--write]
输出：log/map/lane_timing.csv（--write 时）
退出码：0 差异 <=15%；1 有超差（SPEC-01 MAP-06）。
"""

from __future__ import annotations

import argparse
import csv
import json
import sys

import geom
from paths import ROOT

WALK_SPEED = 250.0
TOLERANCE = 0.15


def load_layout(map_id: str) -> dict:
    path = ROOT / f"content/maps/{map_id}.layout.json"
    return json.loads(path.read_text(encoding="utf-8"))


def lane_seconds(lane: dict) -> float:
    return geom.lane_length(lane["points"]) / WALK_SPEED


def build_rows(layout: dict) -> list[dict]:
    rows = []
    for lane in layout["lanes"]:
        walk = lane_seconds(lane)
        rotate = walk * 2.0  # 180° 转身按单程 2 倍保守估计
        target = float(layout.get("rotation_seconds_target", {}).get(lane["id"], walk))
        delta = abs(walk - target) / max(target, 1e-6)
        rows.append({
            "lane": lane["id"],
            "width": lane["width"],
            "distance_m": round(walk * WALK_SPEED, 1),
            "walk_seconds": round(walk, 2),
            "rotate180_seconds": round(rotate, 2),
            "target_seconds": round(target, 2),
            "delta_pct": round(delta * 100.0, 2),
            "start": f"{lane['points'][0][0]},{lane['points'][0][1]}",
            "end": f"{lane['points'][-1][0]},{lane['points'][-1][1]}",
        })
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description="lane timing for de_dust2")
    parser.add_argument("--map", default="de_dust2")
    parser.add_argument("--write", action="store_true", help="写 log/map/lane_timing.csv")
    args = parser.parse_args()

    layout = load_layout(args.map)
    rows = build_rows(layout)
    bad = [row for row in rows if row["delta_pct"] > TOLERANCE * 100.0]
    if args.write:
        out = ROOT / "log/map/lane_timing.csv"
        out.parent.mkdir(parents=True, exist_ok=True)
        with open(out, "w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
            writer.writeheader()
            writer.writerows(rows)
        print(f"wrote {out.relative_to(ROOT)} rows={len(rows)}")
    for row in rows:
        print(f"{row['lane']:<10} walk={row['walk_seconds']:>5.2f}s "
              f"rot180={row['rotate180_seconds']:>5.2f}s "
              f"target={row['target_seconds']:>5.2f}s delta={row['delta_pct']:>5.2f}%")
    if bad:
        print(f"lane_timing FAIL over {TOLERANCE * 100:.0f}%: {[row['lane'] for row in bad]}")
        return 1
    print(f"lane_timing PASS lanes={len(rows)} tolerance={TOLERANCE * 100:.0f}%")
    return 0


if __name__ == "__main__":
    sys.exit(main())
