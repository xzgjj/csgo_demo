"""从 core 头文件解析项目数值（Def 默认值），供数值校准比较使用。

不改 core、不重新编译：直接读 `content.h` / `movement.h` 的字段默认值，
再补少量派生量（RPM、各阶段秒数、跳跃高度等）。解析不到的字段由调用方标 SKIP。
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CONTENT_HEADER = ROOT / "core/include/recoil/content/content.h"
MOVEMENT_HEADER = ROOT / "core/include/recoil/movement/movement.h"
TICKRATE = 64.0

_STRUCT_RE = re.compile(r"struct\s+(\w+)\s*\{(.*?)\n\};", re.S)
_FIELD_RE = re.compile(r"^\s*(?:float|uint16_t|uint32_t|int32_t|int)\s+(\w+)\s*=\s*([^;]+);", re.M)

# 头文件结构名 -> 项目字段前缀（与 content.h 的 Content 成员一致）。
PREFIXES = (("WeaponDef", "rifle"), ("MovementDef", "movement"),
            ("RoundDef", "round"), ("GrenadeDef", "grenades"))

# 派生量：参考表用秒/RPM 等口径，项目存 tick/间隔，比较前换算。
TICK_FIELDS = (
    ("round.freeze_seconds", "round.freeze_ticks"),
    ("round.live_seconds", "round.live_ticks"),
    ("round.end_seconds", "round.end_ticks"),
    ("round.bomb_seconds", "round.bomb_ticks"),
    ("round.defuse_seconds", "round.defuse_ticks"),
    ("round.defuse_kit_seconds", "round.defuse_kit_ticks"),
    ("grenades.he_fuse_seconds", "grenades.fuse_ticks"),
    ("grenades.smoke_seconds", "grenades.smoke_ticks"),
    ("grenades.flash_max_seconds", "grenades.flash_max_ticks"),
)


def _to_number(expr: str) -> float:
    """支持 `36`、`0.775f`、`1.0f / 64.0f` 这类默认值表达式。"""
    text = expr.replace("f", "").strip()
    if "/" in text:
        divisor_iter = iter(part.strip() for part in text.split("/"))
        value = float(next(divisor_iter))
        for divisor in divisor_iter:
            value /= float(divisor)
        return value
    return float(text)


def parse_structs(path: Path) -> dict[str, dict[str, float]]:
    """解析一个头文件里的所有 struct 数值字段；文件缺失时返回空 dict。"""
    if not path.exists():
        return {}
    structs: dict[str, dict[str, float]] = {}
    for name, body in _STRUCT_RE.findall(path.read_text(encoding="utf-8")):
        fields: dict[str, float] = {}
        for field, expr in _FIELD_RE.findall(body):
            try:
                fields[field] = _to_number(expr)
            except ValueError:
                continue
        structs[name] = fields
    return structs


def _add_derived(project: dict[str, float]) -> None:
    tick_dt = project.get("movement.tick_dt")
    if tick_dt:
        project["movement.tickrate"] = round(1.0 / tick_dt)
    interval = project.get("rifle.fire_interval_ticks")
    if interval:
        project["rifle.rpm"] = round(60.0 * TICKRATE / interval, 2)
    damage = project.get("rifle.damage")
    ratio = project.get("rifle.armor_ratio")
    if damage is not None and ratio is not None:
        project["rifle.armored_body_damage"] = round(damage * ratio, 2)
    impulse = project.get("movement.jump_impulse")
    gravity = project.get("movement.gravity")
    if impulse is not None and gravity:
        project["movement.jump_height"] = round(impulse * impulse / (2.0 * gravity), 2)
    for seconds_key, ticks_key in TICK_FIELDS:
        ticks = project.get(ticks_key)
        if ticks is not None:
            project[seconds_key] = round(ticks / TICKRATE, 2)


def load_project() -> dict[str, float]:
    """返回扁平化项目数值表，如 {"rifle.damage": 36.0, "round.freeze_seconds": 3.0}。"""
    structs = parse_structs(CONTENT_HEADER)
    structs.update(parse_structs(MOVEMENT_HEADER))
    project: dict[str, float] = {}
    for struct, prefix in PREFIXES:
        for field, value in structs.get(struct, {}).items():
            project[f"{prefix}.{field}"] = value
    _add_derived(project)
    return project


if __name__ == "__main__":
    for key, value in sorted(load_project().items()):
        print(f"{key} = {value}")
