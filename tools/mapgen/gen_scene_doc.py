"""由 scene-elements.csv 生成 docs/design/dust2/05-场景元素总表.md（防文档漂移）。

用法：python -X utf8 tools/mapgen/gen_scene_doc.py
只读台账、只写目标 md；不做任何几何计算，保证「文档 = 台账」逐行一致。
"""

from __future__ import annotations

import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CSV = ROOT / "docs/scene-design/scene-elements.csv"
OUT = ROOT / "docs/design/dust2/05-场景元素总表.md"

COLUMNS = ("id", "zone", "name", "type", "rect_x1", "rect_y1", "rect_x2", "rect_y2",
           "height_u", "enterable", "window", "breakable", "hp", "surface", "reuse", "note")


def load_rows() -> list[dict[str, str]]:
    with open(CSV, encoding="utf-8") as handle:
        return [{key: row[key] for key in row} for row in csv.DictReader(handle)]


def render(rows: list[dict[str, str]]) -> str:
    zones: dict[str, list[dict[str, str]]] = {}
    for row in rows:
        zones.setdefault(row["zone"], []).append(row)
    lines = [
        "# 05 场景元素总表（机器生成，勿手改）",
        "",
        "> 来源：`docs/scene-design/scene-elements.csv`；重生成：`python -X utf8 tools/mapgen/gen_scene_doc.py`。",
        f"> 共 {len(rows)} 行；高度单位 u（1u≈2.54cm）；坐标 (x1,y1)-(x2,y2)。",
        "",
        "## 分区分项",
        "",
        "| 区 | 元素数 | 说明 |",
        "|---|---|---|",
    ]
    zone_note = {
        "A": "A 包点：高台 + 坡道 + 木箱堆 + 两处窗",
        "B": "B 包点：+64 台 + 内台 + 木棚 + 东侧台阶",
        "M": "中路：中门、猫道、短道三段、凹室",
        "L": "长道：门厅、双开门、两折角、侧屋、大坑",
        "T": "隧道：下/上隧道、折角、隧道口、侧室",
        "S": "出生与买区",
    }
    for zone in ("A", "B", "M", "L", "T", "S"):
        items = zones.get(zone, [])
        lines.append(f"| {zone} | {len(items)} | {zone_note.get(zone, '')} |")
    lines += ["", "## 明细", ""]
    header = "| id | 区 | 名称 | 类型 | 范围 | 高度 | 可破坏/HP | 材质 | 备注 |"
    sep = "|---|---|---|---|---|---|---|---|---|"
    for zone in ("A", "B", "M", "L", "T", "S"):
        items = zones.get(zone, [])
        if not items:
            continue
        lines += [f"### {zone} 区（{len(items)} 项）", "", header, sep]
        for row in items:
            rng = f"({row['rect_x1']},{row['rect_y1']})-({row['rect_x2']},{row['rect_y2']})"
            breakable = "否"
            if row["breakable"] == "yes":
                breakable = f"是 / {row['hp'] or '-'}HP"
            lines.append(
                f"| {row['id']} | {row['zone']} | {row['name']} | {row['type']} | {rng} | "
                f"{row['height_u']} | {breakable} | {row['surface']} | {row['note']} |")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def main() -> int:
    rows = load_rows()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(render(rows), encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)} rows={len(rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
