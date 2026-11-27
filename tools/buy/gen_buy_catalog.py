"""购买目录生成器（SPEC-06 BUY-09）。

输入：`content/defs/buy_catalog.json`（分类/价格/限购/阵营）+ `content/defs/weapons.json`（武器 enum/价格）。
输出：`core/include/recoil/content/buy_catalog.gen.h`（只含数据，不含逻辑）。
校验：6 分类、分类槽位合法、武器存在且目录价 == weapons.json 价、限购 >0、阵营合法；
      `--check` 只比对生成内容是否最新（CI 用），不写文件。

用法：
  python -X utf8 tools/buy/gen_buy_catalog.py
  python -X utf8 tools/buy/gen_buy_catalog.py --check --csv log/verify/buy/catalog.csv
"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CATALOG = ROOT / "content" / "defs" / "buy_catalog.json"
WEAPONS = ROOT / "content" / "defs" / "weapons.json"
OUT = ROOT / "core" / "include" / "recoil" / "content" / "buy_catalog.gen.h"

CATEGORIES = 6
SLOTS = {"secondary": "BuySlot::Secondary", "primary": "BuySlot::Primary",
         "gear": "BuySlot::Gear", "grenade": "BuySlot::Grenade"}
ITEM_KIND = {
    "kevlar": ("BuyKind::Armor", "BuySlot::Gear", "WeaponId::None", "GrenadeKind::HE"),
    "helmet": ("BuyKind::Helmet", "BuySlot::Gear", "WeaponId::None", "GrenadeKind::HE"),
    "defuse_kit": ("BuyKind::DefuseKit", "BuySlot::Gear", "WeaponId::None", "GrenadeKind::HE"),
    "he": ("BuyKind::GrenadeHE", "BuySlot::Grenade", "WeaponId::None", "GrenadeKind::HE"),
    "flash": ("BuyKind::GrenadeFlash", "BuySlot::Grenade", "WeaponId::None", "GrenadeKind::Flash"),
    "smoke": ("BuyKind::GrenadeSmoke", "BuySlot::Grenade", "WeaponId::None", "GrenadeKind::Smoke"),
}
TICKRATE = 64


def _weapons() -> dict[str, dict]:
    data = json.loads(WEAPONS.read_text(encoding="utf-8"))
    return {w["id"]: w for w in data["weapons"]}


def _side_bits(sides: list[str]) -> int:
    bits = 0
    for side in sides:
        bits |= 1 if side == "T" else 2 if side == "CT" else 0
    return bits


def _entry(item: dict, slot: str, cid: str, cat_index: int, weapons: dict,
           errors: list[str]) -> dict | None:
    name = str(item.get("weapon") or item.get("item") or "")
    sides = item.get("sides", ["T", "CT"])
    bits = _side_bits(sides)
    if bits == 0:
        errors.append(f"{name}:sides 非法 {sides}")
    limit = int(item.get("limit", 0))
    if limit <= 0:
        errors.append(f"{name}:limit 必须 > 0")
    if "weapon" in item:
        weapon = weapons.get(name)
        if weapon is None:
            errors.append(f"{name}:weapons.json 中不存在")
            return None
        if int(item["price"]) != int(weapon["price"]):
            errors.append(f"{name}:目录价 {item['price']} != weapons.json {weapon['price']}")
        kind, wslot = "BuyKind::Weapon", slot
        enum = f"WeaponId::{weapon['enum']}"
        grenade = "GrenadeKind::HE"
    else:
        if name not in ITEM_KIND:
            errors.append(f"{name}:未知装备/投掷物")
            return None
        kind, wslot, enum, grenade = ITEM_KIND[name]
    if wslot != slot:
        errors.append(f"{name}:槽位 {wslot} 与分类 {cid} 不一致")
    return {"category": cid, "category_index": cat_index, "id": name, "kind": kind,
            "slot": wslot, "weapon": enum, "grenade": grenade,
            "price": int(item["price"]), "limit": limit, "sides": bits}


def collect(data: dict) -> tuple[list[dict], list[str]]:
    errors: list[str] = []
    categories = data.get("categories")
    if not isinstance(categories, list) or len(categories) != CATEGORIES:
        return [], [f"分类必须为 {CATEGORIES} 个，实际 {len(categories or [])}"]
    weapons = _weapons()
    rows: list[dict] = []
    for cat_index, category in enumerate(categories):
        slot = SLOTS.get(str(category.get("slot", "")))
        if slot is None:
            errors.append(f"{category.get('id', '')}:非法 slot {category.get('slot')!r}")
            continue
        for item in category.get("items", []):
            row = _entry(item, slot, str(category.get("id", "")), cat_index, weapons, errors)
            if row is not None:
                rows.append(row)
    return rows, errors


def render(data: dict, rows: list[dict]) -> str:
    window = data.get("window", {})
    grace_ticks = int(window.get("grace_s", 0)) * TICKRATE
    zone = "true" if window.get("zone_required") else "false"
    max_refunds = int(window.get("max_refunds_per_round", 3))
    lines = [
        "// 生成文件，勿手改！",
        "// 由 tools/buy/gen_buy_catalog.py 从 content/defs/buy_catalog.json 生成。",
        "// 修改价格/限购/阵营请改 JSON 后重新生成：python -X utf8 tools/buy/gen_buy_catalog.py",
        "// clang-format off",
        "#pragma once",
        "",
        '#include "recoil/content/buy_catalog.h"',
        "",
        "namespace recoil {",
        "",
        f"inline constexpr int kBuyEntryCount = {len(rows)};",
        f"inline constexpr uint16_t kBuyWindowTicks = {grace_ticks};",
        f"inline constexpr bool kBuyZoneRequired = {zone};",
        f"inline constexpr uint8_t kBuyMaxRefunds = {max_refunds};",
        "inline constexpr const char* kBuyCategoryNames[6] = {"
        + ", ".join(f'"{c["name"]}"' for c in data["categories"]) + "};",
        "",
        "inline constexpr BuyEntry kBuyEntries[kBuyEntryCount] = {",
    ]
    for row in rows:
        lines.append(
            f'    {{"{row["id"]}", {row["category_index"]}, {row["kind"]}, {row["weapon"]}, '
            f'{row["grenade"]}, {row["slot"]}, {row["price"]}, {row["limit"]}, {row["sides"]}}},'
        )
    lines += [
        "};",
        "",
        "} // namespace recoil",
        "",
    ]
    return "\n".join(lines)


def write_csv(rows: list[dict], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()) if rows else ["id"])
        writer.writeheader()
        writer.writerows(rows)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="购买目录生成/校验")
    parser.add_argument("--check", action="store_true", help="只校验生成文件是否最新")
    parser.add_argument("--csv", default="", help="可选：写出目录 CSV 供证据链")
    args = parser.parse_args(argv)
    data = json.loads(CATALOG.read_text(encoding="utf-8"))
    rows, errors = collect(data)
    text = render(data, rows)
    if errors:
        for error in errors:
            print(f"{CATALOG.name}:{error}")
        print("[buy] FAIL")
        return 1
    if args.check:
        if not OUT.exists() or OUT.read_text(encoding="utf-8") != text:
            print("[buy] FAIL 生成文件过期，请运行 tools/buy/gen_buy_catalog.py")
            return 1
    else:
        OUT.write_text(text, encoding="utf-8")
    if args.csv:
        write_csv(rows, ROOT / args.csv)
    heavy = sum(1 for row in rows if row["slot"] == "BuySlot::Primary")
    print(f"[buy] categories={len(data['categories'])} entries={len(rows)} primary={heavy} PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
