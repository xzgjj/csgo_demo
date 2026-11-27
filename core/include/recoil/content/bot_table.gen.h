// 生成物：由 tools/rules/gen_rules_table.py 从 content/defs/rules.json 生成，禁止手改。
#pragma once

#include <array>

#include "recoil/ai/constraints.h"

namespace recoil {
namespace content {

// 0=Easy 1=Normal 2=Hard 3=Expert，索引与 bot_difficulty level 一致。
inline constexpr std::array<BotDifficulty, 4> kBotDifficulty{{
    BotDifficulty{
        .level = 0,
        .reaction_ticks = 45,
        .view_units = 1200.0f,
        .aim_sigma_deg = 3.2f,
        .first_cone_deg = 4.0f,
        .turn_deg_per_s = 240.0f,
        .burst_shots = 0,
    },
    BotDifficulty{
        .level = 1,
        .reaction_ticks = 32,
        .view_units = 1500.0f,
        .aim_sigma_deg = 2.2f,
        .first_cone_deg = 2.5f,
        .turn_deg_per_s = 300.0f,
        .burst_shots = 7,
    },
    BotDifficulty{
        .level = 2,
        .reaction_ticks = 22,
        .view_units = 1800.0f,
        .aim_sigma_deg = 1.4f,
        .first_cone_deg = 1.5f,
        .turn_deg_per_s = 400.0f,
        .burst_shots = 5,
    },
    BotDifficulty{
        .level = 3,
        .reaction_ticks = 14,
        .view_units = 2000.0f,
        .aim_sigma_deg = 0.8f,
        .first_cone_deg = 0.8f,
        .turn_deg_per_s = 540.0f,
        .burst_shots = 4,
    },
}};

} // namespace content
} // namespace recoil
