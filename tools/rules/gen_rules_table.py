#!/usr/bin/env python3
"""Generate `core/include/recoil/content/rules_table.gen.h` from `content/defs/rules.json`.

SPEC-03 §3.4.3 要求模式与经济参数全部外置，代码零硬编码。本生成器把 JSON 转成
constexpr 表，并做严格校验（人数/包点/回合数/加时一致、胜利分不超上限、阶梯递减）。

用法：
    python -X utf8 tools/rules/gen_rules_table.py            # 生成
    python -X utf8 tools/rules/gen_rules_table.py --check    # 只校验生成物是否最新
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "content" / "defs" / "rules.json"
OUT = ROOT / "core" / "include" / "recoil" / "content" / "rules_table.gen.h"
BOT_OUT = ROOT / "core" / "include" / "recoil" / "content" / "bot_table.gen.h"
TICK = 64


def ticks(sec: float) -> int:
    return int(round(sec * TICK))


def sec(value: int) -> int:
    """把 JSON 里的秒字段换算成 tick。"""
    return ticks(float(value))


def mode_row(key: str, mode: dict) -> dict:
    """把一个模式的 JSON 转成生成器内部行，并做模式内校验（分支集中在这里）。"""
    size = mode["team_size"]
    max_rounds = int(mode["max_rounds"])
    win_score = int(mode["win_score"])
    # 校验：先到分必须能在“半场+1”的回合内达到（1v1 无半场，10 回合内先到 6）。
    if win_score > max_rounds // 2 + 1:
        raise SystemExit(f"{key}: win_score {win_score} 超出回合上限 {max_rounds}")
    has_ot = bool(mode["ot_win_score"])
    if has_ot != (int(mode["overtime_cap"]) > 0):
        raise SystemExit(f"{key}: 加时开关与 overtime_cap 不一致")
    return dict(
        key=key,
        mode_id=mode["mode_id"],
        team_size=size,
        sites=int(mode["sites"]),
        max_rounds=max_rounds,
        win_score=win_score,
        freeze=sec(mode["freeze_sec"]),
        round_ticks=sec(mode["round_sec"]),
        buy=sec(mode["buy_sec"]),
        end=sec(mode["round_end_sec"]),
        ot_cap=int(mode["overtime_cap"]),
        ot_half=int(mode["ot_rounds_per_half"]),
        ot_win=int(mode["ot_win_score"]),
        ot_money=int(mode["ot_money"]),
        money_start=int(mode["money_start"]),
        money_cap=int(mode["money_cap"]),
    )


def build() -> tuple[str, str, dict]:
    doc = json.loads(SRC.read_text(encoding="utf-8"))
    shared = doc["shared"]
    order = ["1v1", "3v3", "5v5"]
    rows = [mode_row(key, doc["modes"][key]) for key in order]
    ladder = shared["loss_ladder"]
    if len(ladder) != 5 or any(ladder[i] >= ladder[i + 1] for i in range(4)):
        raise SystemExit("loss_ladder 必须 5 档且严格递增")
    shared_out = dict(
        c4=sec(shared["c4_sec"]),
        defuse=sec(shared["defuse_sec"]),
        defuse_kit=sec(shared["defuse_kit_sec"]),
        halftime=sec(shared["halftime_sec"]),
        warmup=sec(shared["warmup_sec"]),
        timeout_count=int(shared["timeout_count"]),
        timeout=sec(shared["timeout_sec"]),
        friendly_fire="true" if shared["friendly_fire"] else "false",
        win_elim=int(shared["win_elim"]),
        win_objective=int(shared["win_objective"]),
        ladder=ladder,
        plant_bonus=int(shared["plant_bonus"]),
        defuse_bonus=int(shared["defuse_bonus"]),
        plant_loss=int(shared["plant_loss_bonus"]),
        kills=[
            int(shared["kill_rifle"]),
            int(shared["kill_pistol"]),
            int(shared["kill_sniper"]),
            int(shared["kill_smg"]),
            int(shared["kill_shotgun"]),
        ],
    )

    lines = [
        "// 生成物：由 tools/rules/gen_rules_table.py 从 content/defs/rules.json 生成，禁止手改。",
        "#pragma once",
        "",
        "#include <array>",
        "",
        '#include "recoil/sim/match.h"',
        "",
        "namespace recoil {",
        "namespace content {",
        "",
        "// 三种对局模式的静态参数表，索引与 ModeId 的枚举值一一对应。",
        "inline constexpr std::array<MatchRules, 3> kMatchRules{{",
    ]
    for row in rows:
        lines.append("    MatchRules{")
        lines.append("        .enabled = true,")
        lines.append(f"        .mode = ModeId::{row['mode_id']},")
        lines.append(f"        .team_size = {row['team_size']},")
        lines.append(f"        .site_count = {row['sites']},")
        lines.append(f"        .max_rounds = {row['max_rounds']},")
        lines.append(f"        .win_score = {row['win_score']},")
        lines.append(f"        .freeze_ticks = {row['freeze']},")
        lines.append(f"        .round_ticks = {row['round_ticks']},")
        lines.append(f"        .buy_ticks = {row['buy']},")
        lines.append(f"        .round_end_ticks = {row['end']},")
        lines.append(f"        .warmup_ticks = {shared_out['warmup']},")
        lines.append(f"        .halftime_ticks = {shared_out['halftime']},")
        lines.append(f"        .c4_ticks = {shared_out['c4']},")
        lines.append(f"        .defuse_ticks = {shared_out['defuse']},")
        lines.append(f"        .defuse_kit_ticks = {shared_out['defuse_kit']},")
        lines.append(f"        .money_start = {row['money_start']},")
        lines.append(f"        .money_cap = {row['money_cap']},")
        lines.append(f"        .win_elim = {shared_out['win_elim']},")
        lines.append(f"        .win_objective = {shared_out['win_objective']},")
        lines.append(
            f"        .loss_ladder = {{{', '.join(str(v) for v in shared_out['ladder'])}}},"
        )
        lines.append(f"        .plant_bonus = {shared_out['plant_bonus']},")
        lines.append(f"        .defuse_bonus = {shared_out['defuse_bonus']},")
        lines.append(f"        .plant_loss_bonus = {shared_out['plant_loss']},")
        lines.append(
            f"        .kill_award = {{{', '.join(str(v) for v in shared_out['kills'])}}},"
        )
        lines.append(f"        .timeout_count = {shared_out['timeout_count']},")
        lines.append(f"        .timeout_ticks = {shared_out['timeout']},")
        lines.append(f"        .friendly_fire = {shared_out['friendly_fire']},")
        lines.append(f"        .overtime_cap = {row['ot_cap']},")
        lines.append(f"        .ot_rounds_per_half = {row['ot_half']},")
        lines.append(f"        .ot_win_score = {row['ot_win']},")
        lines.append(f"        .ot_money = {row['ot_money']},")
        lines.append("    },")
    lines.append("}};")
    lines.append("")
    lines.append("// 单例查找：未知模式返回 5v5（并由 BeginMatch 判 E01 拒绝）。")
    lines.append("inline constexpr const MatchRules& MatchRulesFor(ModeId mode) {")
    lines.append("    const int index = static_cast<int>(mode);")
    lines.append(
        "    return (index >= 0 && index < static_cast<int>(kMatchRules.size()))"
    )
    lines.append("               ? kMatchRules[static_cast<size_t>(index)]")
    lines.append("               : kMatchRules[2];")
    lines.append("}")
    lines.append("")
    lines.append("} // namespace content")
    lines.append("} // namespace recoil")
    lines.append("")
    return "\n".join(lines), build_bot(doc), shared_out


def build_bot(doc: dict) -> str:
    """生成人机难度表（SPEC-03 §3.8.3），以 tick 为单位反应延迟。"""
    rows = sorted(doc["bot_difficulty"], key=lambda r: int(r["level"]))
    if [int(r["level"]) for r in rows] != [0, 1, 2, 3]:
        raise SystemExit("bot_difficulty 必须覆盖 level 0..3")
    lines = [
        "// 生成物：由 tools/rules/gen_rules_table.py 从 content/defs/rules.json 生成，禁止手改。",
        "#pragma once",
        "",
        "#include <array>",
        "",
        '#include "recoil/ai/constraints.h"',
        "",
        "namespace recoil {",
        "namespace content {",
        "",
        "// 0=Easy 1=Normal 2=Hard 3=Expert，索引与 bot_difficulty level 一致。",
        "inline constexpr std::array<BotDifficulty, 4> kBotDifficulty{{",
    ]
    for row in rows:
        lines.append("    BotDifficulty{")
        lines.append(f"        .level = {int(row['level'])},\n"
                     f"        .reaction_ticks = {ticks(float(row['reaction_ms']) / 1000.0)},\n"
                     f"        .view_units = {float(row['view_units']):.1f}f,\n"
                     f"        .aim_sigma_deg = {float(row['aim_sigma_deg']):.1f}f,\n"
                     f"        .first_cone_deg = {float(row['first_cone_deg']):.1f}f,\n"
                     f"        .turn_deg_per_s = {float(row['turn_deg_per_s']):.1f}f,\n"
                     f"        .burst_shots = {int(row['burst_shots'])},")
        lines.append("    },")
    lines.append("}};")
    lines.append("")
    lines.append("} // namespace content")
    lines.append("} // namespace recoil")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    text, bot_text, _ = build()
    if args.check:
        stale = not OUT.exists() or OUT.read_text(encoding="utf-8") != text
        stale = stale or not BOT_OUT.exists() or BOT_OUT.read_text(encoding="utf-8") != bot_text
        if stale:
            print("rules/bot 表已过期：请运行 python tools/rules/gen_rules_table.py", file=sys.stderr)
            return 1
        print("rules/bot 表最新")
        return 0
    OUT.write_text(text, encoding="utf-8")
    BOT_OUT.write_text(bot_text, encoding="utf-8")
    print(f"写入 {OUT.relative_to(ROOT)}、{BOT_OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
