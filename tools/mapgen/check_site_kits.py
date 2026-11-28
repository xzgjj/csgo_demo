"""静态检查 A/B 共用 kit 与 CSV 实例是否同源。

不生成地图、不启 Godot：只读 site_kits.json 与 scene-elements.csv。
CI 里先跑它，任何漂移在 Blender 之前就停。
"""

from __future__ import annotations

import csv
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
KITS = ROOT / "content/defs/site_kits.json"
CSV = ROOT / "docs/scene-design/scene-elements.csv"
SURFACES = {"concrete", "stone", "wood", "metal", "fabric", "dirt", "glass"}


def fail(message: str) -> int:
    print(f"[site_kits] FAIL {message}")
    return 1


def load_assets(defs: dict) -> dict[str, dict]:
    shared = defs.get("shared", {})
    return {kit.get("asset_id", f"Kit-{key}"): kit for key, kit in shared.items()}


def validate_shared(assets: dict[str, dict]) -> str:
    for asset_id, kit in assets.items():
        size = kit.get("size", [])
        if len(size) != 3 or any(int(v) <= 0 for v in size):
            return f"{asset_id} size 非法: {size}"
        if kit.get("surface") not in SURFACES:
            return f"{asset_id} surface 非法: {kit.get('surface')}"
        if not isinstance(kit.get("collision"), bool):
            return f"{asset_id} collision 必须是 bool"
    return ""


def validate_props(rows: list[dict], assets: dict[str, dict]) -> tuple[str, dict[str, set[str]]]:
    used: dict[str, set[str]] = {"A": set(), "B": set()}
    for row in rows:
        kit = assets.get(row["reuse"])
        if kit is None:
            return f"{row['id']} 引用未定义 kit {row['reuse']}", used
        got = [int(row["rect_x2"]) - int(row["rect_x1"]),
               int(row["rect_y2"]) - int(row["rect_y1"]), int(row["height_u"])]
        if got != [int(v) for v in kit["size"]]:
            return f"{row['id']} 尺寸 {got} != {kit['size']}", used
        if row["surface"] != kit["surface"]:
            return f"{row['id']} surface {row['surface']} != {kit['surface']}", used
        if not kit["collision"]:
            return f"{row['id']} 引用了非碰撞 kit {row['reuse']}，道具盒必须参与碰撞", used
        used[row["zone"]].add(row["reuse"])
    return "", used


def validate_site(site_id: str, site: dict, used: dict[str, set[str]]) -> str:
    rect = site.get("rect", [])
    if len(rect) != 4:
        return f"{site_id} rect 非法: {rect}"
    if len(site.get("lights", [])) < 2:
        return f"{site_id} 至少需要 2 盏灯"
    for key, color in site.get("tint", {}).items():
        if key not in SURFACES:
            return f"{site_id} tint 未知 surface: {key}"
        if not (isinstance(color, str) and color.startswith("#") and len(color) == 7):
            return f"{site_id} tint {key} 颜色非法: {color}"
    declared = set(site.get("signature_kits", []))
    if len(used[site_id]) < 4:
        return f"{site_id} 实际只用了 {len(used[site_id])} 种 kit，至少 4"
    if declared != used[site_id]:
        return f"{site_id} signature_kits {sorted(declared)} != 实际 {sorted(used[site_id])}"
    return ""


def main() -> int:
    defs = json.loads(KITS.read_text(encoding="utf-8"))
    if defs.get("schema") != "site-kits/1":
        return fail("schema 必须是 site-kits/1")
    assets = load_assets(defs)
    message = validate_shared(assets)
    if message:
        return fail(message)

    rows = list(csv.DictReader(CSV.open(encoding="utf-8", newline="")))
    props = [row for row in rows if row["zone"] in {"A", "B"} and row["reuse"].startswith("Kit-")]
    message, used = validate_props(props, assets)
    if message:
        return fail(message)

    for site_id in ("A", "B"):
        message = validate_site(site_id, defs.get("sites", {}).get(site_id, {}), used)
        if message:
            return fail(message)

    print(f"[site_kits] PASS shared={len(assets)} A={len(used['A'])} B={len(used['B'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
