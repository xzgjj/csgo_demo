"""校验 de_ash 布局数据：边界/包点/出生/道具/材质一致性。用法：python -X utf8 tools/mapgen/check_layout.py"""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAYOUT = ROOT / "content/maps/de_ash.layout.json"


def inside(lo, hi, point, margin=0.0) -> bool:
    return all(lo[i] + margin <= point[i] <= hi[i] - margin for i in range(2))


def _site_issues(data, lo, hi):
    issues = []
    for site in data["bombsites"]:
        if not inside(lo, hi, site["center"]):
            issues.append(f"包点越界: {site['id']}")
        if site["size"][0] != 512 or site["size"][1] != 512:
            issues.append(f"包点尺寸异常: {site['id']}")
    return issues


def _spawn_issues(data, lo, hi):
    return [f"出生点越界: {team}" for team, spawn in data["spawns"].items()
            if not inside(lo, hi, spawn["center"])]


def _prop_issues(data, lo, hi):
    return [f"道具越界: {prop['id']}" for prop in data["props"]
            if not inside(lo, hi, prop["center"])]


def main() -> int:
    data = json.loads(LAYOUT.read_text(encoding="utf-8"))
    lo, hi = data["bounds"]["min"], data["bounds"]["max"]
    errors = _site_issues(data, lo, hi) + _spawn_issues(data, lo, hi) + _prop_issues(data, lo, hi)
    if len(set(data.get("materials", []))) != 6:
        errors.append("材质数量应为 6")
    if not data.get("lanes"):
        errors.append("缺少路线")
    if errors:
        print("mapcheck FAIL")
        for e in errors:
            print(" -", e)
        return 1
    print(f"mapcheck PASS name={data['name']} sites={len(data['bombsites'])} "
          f"lanes={len(data['lanes'])} props={len(data['props'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
