"""武器表生成器：content/defs/weapons.json → core/include/recoil/content/weapon_table.gen.h。

生成物入库（供内核 constexpr 查表，免运行期 JSON 解析）；头注释标明"生成文件勿手改"。
用法：
  python -X utf8 tools/weapons/gen_weapon_table.py           # 写文件
  python -X utf8 tools/weapons/gen_weapon_table.py --check   # 只校验（不一致退出 1，CI 用）
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "content/defs/weapons.json"
TARGET = ROOT / "core/include/recoil/content/weapon_table.gen.h"
SCHEMA = "recoil.weapons/1"

# WeaponDef 字段（顺序必须与 core/include/recoil/content/content.h 的声明顺序一致）。
WEAPON_DEF_FIELDS = (
    "damage",
    "magazine",
    "reserve",
    "fire_interval_ticks",
    "reload_ticks",
    "armor_ratio",
    "range",
    "range_modifier",
    "range_modifier_unit",
    "scope_magnification",
)
INT_FIELDS = {"damage", "magazine", "reserve", "fire_interval_ticks", "reload_ticks"}
ROW_FIELDS = ("id", "enum", "name", "team", "category", "price") + WEAPON_DEF_FIELDS + ("rpm",)


def _fail(message: str) -> None:
    print(f"[gen_weapon_table] {message}", file=sys.stderr)
    raise SystemExit(1)


def _read_json(path: Path) -> dict:
    if not path.exists():
        _fail(f"缺少数据源 {path.relative_to(ROOT)}")
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema") != SCHEMA:
        _fail(f"schema 应为 {SCHEMA}，实际 {data.get('schema')!r}")
    weapons = data.get("weapons")
    if not isinstance(weapons, list) or not weapons:
        _fail("weapons 必须是非空数组")
    return data


def _validate_names(weapon: dict, index: int) -> None:
    for field in ("id", "enum", "name"):
        if not isinstance(weapon[field], str) or not weapon[field]:
            _fail(f"第 {index} 把武器 {field} 必须是非空字符串")
    if weapon["enum"] == "None":
        _fail("enum=None 与 WeaponId::None 冲突")


def _validate_unique(weapon: dict, seen_ids: set[str], seen_enums: set[str]) -> None:
    if weapon["id"] in seen_ids or weapon["enum"] in seen_enums:
        _fail(f"武器 id/enum 重复：{weapon['id']}/{weapon['enum']}")
    seen_ids.add(weapon["id"])
    seen_enums.add(weapon["enum"])


def _validate_numbers(weapon: dict, tickrate: float) -> None:
    for field in INT_FIELDS:
        if not isinstance(weapon[field], int) or weapon[field] <= 0:
            _fail(f"{weapon['id']}.{field} 必须是正整数")
    expected = round(60.0 * tickrate / float(weapon["rpm"]))
    if abs(expected - weapon["fire_interval_ticks"]) > 1:
        _fail(
            f"{weapon['id']}: fire_interval_ticks={weapon['fire_interval_ticks']} "
            f"与 {weapon['rpm']}rpm@{tickrate:g}tick 不一致（期望≈{expected}）"
        )


def load_source(path: Path = SOURCE) -> dict:
    data = _read_json(path)
    tickrate = float(data.get("tickrate", 64))
    seen_ids: set[str] = set()
    seen_enums: set[str] = set()
    for index, weapon in enumerate(data["weapons"]):
        missing = [field for field in ROW_FIELDS if field not in weapon]
        if missing:
            _fail(f"第 {index} 把武器缺少字段 {missing}")
        _validate_names(weapon, index)
        _validate_unique(weapon, seen_ids, seen_enums)
        _validate_numbers(weapon, tickrate)
    return data


def _float(value: float) -> str:
    text = f"{float(value):.6f}".rstrip("0").rstrip(".")
    if "." not in text:
        text += ".0"
    return f"{text}f"


def _weapon_def(weapon: dict) -> str:
    parts = []
    for field in WEAPON_DEF_FIELDS:
        value = weapon[field]
        parts.append(f"{_float(value) if field not in INT_FIELDS else int(value)}")
    return "WeaponDef{" + ", ".join(parts) + "}"


def render(data: dict) -> str:
    weapons = data["weapons"]
    lines = [
        "// 生成文件，勿手改！",
        "// 由 tools/weapons/gen_weapon_table.py 从 content/defs/weapons.json 生成；",
        f"// schema: {data.get('schema')}（tickrate {data.get('tickrate', 64)}）。",
        "// 修改数值请改 JSON 后重新生成：python -X utf8 tools/weapons/gen_weapon_table.py",
        "// clang-format off",
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstdint>",
        "",
        '#include "recoil/content/content.h"',
        '#include "recoil/sim/state.h"',
        "",
        "namespace recoil {",
        "",
        f"inline constexpr int kWeaponTableSize = {len(weapons)};",
        "",
        "// 一把武器的静态数据：WeaponId 枚举 + 展示名 + 内核数值 + 购买价格 + 是否副武器。",
        "struct WeaponRow {",
        "    WeaponId id;",
        "    const char* name;",
        "    WeaponDef def;",
        "    uint16_t price;",
        "    bool sidearm; // true=手枪（进副武器槽）",
        "};",
        "",
        "inline constexpr std::array<WeaponRow, kWeaponTableSize> kWeaponTable = {{",
    ]
    for weapon in weapons:
        sidearm = "true" if weapon["category"] == "pistol" else "false"
        lines.append(
            f'    {{WeaponId::{weapon["enum"]}, "{weapon["name"]}",\n'
            f'     {_weapon_def(weapon)}, {int(weapon["price"])}, {sidearm}}},'
        )
    lines += [
        "}};",
        "",
        "} // namespace recoil",
        "// clang-format on",
        "",
    ]
    return "\n".join(lines)


def generated_text() -> str:
    return render(load_source())


def main() -> int:
    parser = argparse.ArgumentParser(description="weapons.json → weapon_table.gen.h")
    parser.add_argument("--check", action="store_true", help="只校验生成物是否最新（不一致退出 1）")
    args = parser.parse_args()
    text = generated_text()
    if args.check:
        current = TARGET.read_text(encoding="utf-8") if TARGET.exists() else ""
        if current != text:
            print(f"[gen_weapon_table] {TARGET.relative_to(ROOT)} 已过期，请重新生成", file=sys.stderr)
            return 1
        print(f"[gen_weapon_table] OK：{TARGET.relative_to(ROOT)} 与 JSON 一致")
        return 0
    TARGET.parent.mkdir(parents=True, exist_ok=True)
    TARGET.write_text(text, encoding="utf-8")
    print(f"[gen_weapon_table] 写入 {TARGET.relative_to(ROOT)}（{len(load_source()['weapons'])} 把武器）")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
