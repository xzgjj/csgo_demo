"""de_dust2 雷达对齐（scene alignment）：4 个锚点与设计台账对比，差异 <=5% 图幅。

用法：python -X utf8 tools/mapgen/scene_alignment.py [--map de_dust2] [--write]
输出：log/map/scene_alignment.csv（--write 时）
退出码：0 通过；1 超差（SPEC-01 MAP-09）。
"""

from __future__ import annotations

import argparse
import csv
import json
import sys

from paths import ROOT

# 设计台账（docs/design/01-总体布局.md）；只作为尺子，不反向生成几何。
EXPECTED = {
    "T_spawn": (-565, -1835),
    "CT_spawn": (380, 1420),
    "A_site": (1500, 1566),
    "B_site": (-1506, 1771),
}
MAX_DELTA_PCT = 0.05


def anchors(layout: dict) -> dict:
    return {
        "T_spawn": tuple(layout["spawns"]["T"]["center"][:2]),
        "CT_spawn": tuple(layout["spawns"]["CT"]["center"][:2]),
        "A_site": tuple(layout["sites"]["A"]["center"][:2]),
        "B_site": tuple(layout["sites"]["B"]["center"][:2]),
    }


def build_rows(layout: dict) -> list[dict]:
    span = float(layout["bounds"]["max"][0] - layout["bounds"]["min"][0])
    rows = []
    for name, (ex, ey) in EXPECTED.items():
        ax, ay = anchors(layout)[name]
        dx, dy = ax - ex, ay - ey
        dist = (dx * dx + dy * dy) ** 0.5
        rows.append({
            "anchor": name,
            "expected": f"{ex},{ey}",
            "actual": f"{ax:.0f},{ay:.0f}",
            "dx": round(dx, 1),
            "dy": round(dy, 1),
            "distance_u": round(dist, 1),
            "delta_pct": round(dist / span * 100.0, 2),
        })
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description="scene anchor alignment")
    parser.add_argument("--map", default="de_dust2")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    path = ROOT / f"content/maps/{args.map}.layout.json"
    layout = json.loads(path.read_text(encoding="utf-8"))
    rows = build_rows(layout)
    if args.write:
        out = ROOT / "log/map/scene_alignment.csv"
        out.parent.mkdir(parents=True, exist_ok=True)
        with open(out, "w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
            writer.writeheader()
            writer.writerows(rows)
        print(f"wrote {out.relative_to(ROOT)} rows={len(rows)}")

    bad = [row for row in rows if row["delta_pct"] > MAX_DELTA_PCT * 100.0]
    center_bad = [name for name in ("A_site", "B_site")
                  if abs(anchors(layout)[name][0]) < 300 and abs(anchors(layout)[name][1]) < 300]
    for row in rows:
        print(f"{row['anchor']:<10} actual=({row['actual']}) "
              f"dx={row['dx']} dy={row['dy']} delta={row['delta_pct']}%")
    if bad or center_bad:
        print(f"scene_alignment FAIL delta_over_5pct={[r['anchor'] for r in bad]} "
              f"center_overlap={center_bad}")
        return 1
    print(f"scene_alignment PASS anchors={len(rows)} max_delta={MAX_DELTA_PCT * 100:.0f}%")
    return 0


if __name__ == "__main__":
    sys.exit(main())
