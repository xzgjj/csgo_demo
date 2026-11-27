"""武器表生成器：content/defs/weapons.json → 两张 constexpr 表。

产物（均入库，供内核免运行期 JSON 解析）：
  core/include/recoil/content/weapon_table.gen.h      WeaponRow（伤害/弹匣/价格）
  core/include/recoil/content/ballistics_table.gen.h  BallisticsRow（后坐力/扩散/击杀奖励）

用法：
  python -X utf8 tools/weapons/gen_weapon_table.py           # 写文件
  python -X utf8 tools/weapons/gen_weapon_table.py --check   # 只校验（不一致退出 1，CI 用）
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "content/defs/weapons.json"
TARGET = ROOT / "core/include/recoil/content/weapon_table.gen.h"
BALLISTICS_TARGET = ROOT / "core/include/recoil/content/ballistics_table.gen.h"
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
ROW_FIELDS = ("id", "enum", "name", "team", "category", "price") + WEAPON_DEF_FIELDS + (
    "rpm",
    "kill_award",
    "recoil",
    "inaccuracy",
)

PATTERN_LENGTH = 30
INACCURACY_FIELDS = (
    "base",
    "per_shot",
    "max",
    "move_mult",
    "jump_mult",
    "duck_mult",
    "recovery_initial",
    "recovery_final",
    "recovery_delay_s",
    "recovery_switch_s",
)
CATEGORY_ENUM = {
    "rifle": "Rifle",
    "pistol": "Pistol",
    "sniper": "Sniper",
    "smg": "Smg",
    "shotgun": "Shotgun",
}
# 类别击杀奖励基线（SPEC-04 §4.5；逐枪 kill_award 必须与类别一致，防止表漂移）。
CATEGORY_AWARD = {"rifle": 300, "pistol": 300, "sniper": 100, "smg": 600, "shotgun": 900}


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


def _finite(value: object, where: str) -> float:
    if not isinstance(value, (int, float)) or not math.isfinite(float(value)):
        _fail(f"{where} 必须是有限数值，实际 {value!r}")
    return float(value)


def _validate_category(weapon: dict) -> None:
    category = weapon.get("category")
    if category not in CATEGORY_ENUM:
        _fail(f"{weapon['id']}.category 非法：{category!r}")
    award = weapon.get("kill_award")
    if not isinstance(award, int) or award < 0:
        _fail(f"{weapon['id']}.kill_award 必须是非负整数")
    if award != CATEGORY_AWARD[category]:
        _fail(
            f"{weapon['id']}.kill_award={award} 与类别 {category} 基线 "
            f"{CATEGORY_AWARD[category]} 不一致"
        )


def _validate_pattern(weapon: dict, recoil: dict) -> None:
    pattern = recoil.get("pattern")
    if not isinstance(pattern, list) or len(pattern) != PATTERN_LENGTH:
        _fail(f"{weapon['id']}.recoil.pattern 必须是 {PATTERN_LENGTH} 段")
    for index, point in enumerate(pattern):
        if not isinstance(point, list) or len(point) != 2:
            _fail(f"{weapon['id']}.recoil.pattern[{index}] 必须是 [pitch, yaw]")
        pitch = _finite(point[0], f"{weapon['id']}.recoil.pattern[{index}][0]")
        yaw = _finite(point[1], f"{weapon['id']}.recoil.pattern[{index}][1]")
        if abs(pitch) > 30.0 or abs(yaw) > 30.0:
            _fail(f"{weapon['id']}.recoil.pattern[{index}] 超出 ±30° 合理范围")
    if pattern[0] != [0.0, 0.0]:
        _fail(f"{weapon['id']}.recoil.pattern[0] 必须是 [0, 0]（首发对准星）")
    camera_scale = _finite(recoil.get("camera_scale", 1.0), f"{weapon['id']}.recoil.camera_scale")
    if not 0.0 <= camera_scale <= 1.0:
        _fail(f"{weapon['id']}.recoil.camera_scale 必须在 [0,1]")


def _validate_inaccuracy(weapon: dict, inaccuracy: dict) -> None:
    for field in INACCURACY_FIELDS:
        value = _finite(inaccuracy.get(field), f"{weapon['id']}.inaccuracy.{field}")
        if value < 0.0:
            _fail(f"{weapon['id']}.inaccuracy.{field} 不能为负")
    if inaccuracy["max"] < inaccuracy["base"]:
        _fail(f"{weapon['id']}.inaccuracy.max 不能小于 base")
    for field in ("move_mult", "jump_mult", "duck_mult"):
        if inaccuracy[field] <= 0.0:
            _fail(f"{weapon['id']}.inaccuracy.{field} 必须为正（倍率）")
    if not (inaccuracy["duck_mult"] < 1.0 < inaccuracy["move_mult"] < inaccuracy["jump_mult"]):
        _fail(f"{weapon['id']}.inaccuracy 姿态序必须 duck < 站(1) < move < jump")


def _validate_ballistics(weapon: dict) -> None:
    _validate_category(weapon)
    recoil = weapon.get("recoil")
    if not isinstance(recoil, dict):
        _fail(f"{weapon['id']}.recoil 必须是对象")
    _validate_pattern(weapon, recoil)
    inaccuracy = weapon.get("inaccuracy")
    if not isinstance(inaccuracy, dict):
        _fail(f"{weapon['id']}.inaccuracy 必须是对象")
    _validate_inaccuracy(weapon, inaccuracy)


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
        _validate_ballistics(weapon)
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


def _pattern_lines(pattern: list) -> list[str]:
    lines = []
    for start in range(0, len(pattern), 5):
        chunk = pattern[start : start + 5]
        points = ", ".join(
            f"RecoilPoint{{{_float(p[0])}, {_float(p[1])}}}" for p in chunk
        )
        lines.append(f"            {points},")
    return lines


def render_ballistics(data: dict) -> str:
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
        '#include "recoil/sim/ballistics.h"',
        "",
        "namespace recoil {",
        "",
        f"inline constexpr int kBallisticsTableSize = {len(data['weapons'])};",
        "",
        "inline constexpr std::array<BallisticsRow, kBallisticsTableSize> kBallisticsTable = {{",
    ]
    for weapon in data["weapons"]:
        recoil = weapon["recoil"]
        inacc = weapon["inaccuracy"]
        category = CATEGORY_ENUM[weapon["category"]]
        lines.append(f"    {{WeaponId::{weapon['enum']}, WeaponCategory::{category},")
        lines.append("     BallisticsDef{RecoilDef{std::array<RecoilPoint, kRecoilPatternLength>{{")
        lines += _pattern_lines(recoil["pattern"])
        lines.append("                }}, " + _float(recoil["camera_scale"]) + "},")
        values = ", ".join(_float(inacc[field]) for field in INACCURACY_FIELDS)
        lines.append("      InaccuracyDef{" + values + "}},")
        lines.append("      " + str(int(weapon["kill_award"])) + "},")
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


def generated_ballistics_text() -> str:
    return render_ballistics(load_source())


def _check(path: Path, text: str) -> bool:
    current = path.read_text(encoding="utf-8") if path.exists() else ""
    if current != text:
        print(f"[gen_weapon_table] {path.relative_to(ROOT)} 已过期，请重新生成", file=sys.stderr)
        return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="weapons.json → weapon_table / ballistics_table")
    parser.add_argument("--check", action="store_true", help="只校验生成物是否最新（不一致退出 1）")
    args = parser.parse_args()
    texts = ((TARGET, generated_text()), (BALLISTICS_TARGET, generated_ballistics_text()))
    if args.check:
        if all(_check(path, text) for path, text in texts):
            print("[gen_weapon_table] OK：两张生成表与 JSON 一致")
            return 0
        return 1
    for path, text in texts:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        print(f"[gen_weapon_table] 写入 {path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
