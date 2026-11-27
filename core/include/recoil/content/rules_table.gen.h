// 生成物：由 tools/rules/gen_rules_table.py 从 content/defs/rules.json 生成，禁止手改。
#pragma once

#include <array>

#include "recoil/sim/match.h"

namespace recoil {
namespace content {

// 三种对局模式的静态参数表，索引与 ModeId 的枚举值一一对应。
inline constexpr std::array<MatchRules, 3> kMatchRules{{
    MatchRules{
        .enabled = true,
        .mode = ModeId::Duel1v1,
        .team_size = 1,
        .site_count = 1,
        .max_rounds = 10,
        .win_score = 6,
        .freeze_ticks = 640,
        .round_ticks = 3840,
        .buy_ticks = 960,
        .round_end_ticks = 320,
        .warmup_ticks = 1920,
        .halftime_ticks = 11520,
        .c4_ticks = 2560,
        .defuse_ticks = 640,
        .defuse_kit_ticks = 320,
        .money_start = 800,
        .money_cap = 8000,
        .win_elim = 3250,
        .win_objective = 3500,
        .loss_ladder = {1400, 1900, 2400, 2900, 3400},
        .plant_bonus = 300,
        .defuse_bonus = 300,
        .plant_loss_bonus = 600,
        .kill_award = {300, 300, 100, 600, 900},
        .timeout_count = 3,
        .timeout_ticks = 1920,
        .friendly_fire = false,
        .overtime_cap = 0,
        .ot_rounds_per_half = 0,
        .ot_win_score = 0,
        .ot_money = 800,
    },
    MatchRules{
        .enabled = true,
        .mode = ModeId::Squad3v3,
        .team_size = 3,
        .site_count = 1,
        .max_rounds = 16,
        .win_score = 9,
        .freeze_ticks = 960,
        .round_ticks = 6400,
        .buy_ticks = 1280,
        .round_end_ticks = 320,
        .warmup_ticks = 1920,
        .halftime_ticks = 11520,
        .c4_ticks = 2560,
        .defuse_ticks = 640,
        .defuse_kit_ticks = 320,
        .money_start = 800,
        .money_cap = 12000,
        .win_elim = 3250,
        .win_objective = 3500,
        .loss_ladder = {1400, 1900, 2400, 2900, 3400},
        .plant_bonus = 300,
        .defuse_bonus = 300,
        .plant_loss_bonus = 600,
        .kill_award = {300, 300, 100, 600, 900},
        .timeout_count = 3,
        .timeout_ticks = 1920,
        .friendly_fire = false,
        .overtime_cap = 10,
        .ot_rounds_per_half = 3,
        .ot_win_score = 4,
        .ot_money = 10000,
    },
    MatchRules{
        .enabled = true,
        .mode = ModeId::Comp5v5,
        .team_size = 5,
        .site_count = 2,
        .max_rounds = 24,
        .win_score = 13,
        .freeze_ticks = 960,
        .round_ticks = 7360,
        .buy_ticks = 1280,
        .round_end_ticks = 320,
        .warmup_ticks = 1920,
        .halftime_ticks = 11520,
        .c4_ticks = 2560,
        .defuse_ticks = 640,
        .defuse_kit_ticks = 320,
        .money_start = 800,
        .money_cap = 16000,
        .win_elim = 3250,
        .win_objective = 3500,
        .loss_ladder = {1400, 1900, 2400, 2900, 3400},
        .plant_bonus = 300,
        .defuse_bonus = 300,
        .plant_loss_bonus = 600,
        .kill_award = {300, 300, 100, 600, 900},
        .timeout_count = 3,
        .timeout_ticks = 1920,
        .friendly_fire = false,
        .overtime_cap = 10,
        .ot_rounds_per_half = 3,
        .ot_win_score = 4,
        .ot_money = 10000,
    },
}};

// 单例查找：未知模式返回 5v5（并由 BeginMatch 判 E01 拒绝）。
inline constexpr const MatchRules& MatchRulesFor(ModeId mode) {
    const int index = static_cast<int>(mode);
    return (index >= 0 && index < static_cast<int>(kMatchRules.size()))
               ? kMatchRules[static_cast<size_t>(index)]
               : kMatchRules[2];
}

} // namespace content
} // namespace recoil
