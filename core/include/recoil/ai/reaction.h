#pragma once

#include <cstdint>

#include "recoil/sim/state.h"
#include "recoil/world/map.h"

namespace recoil {

// 没有可见敌人时的 reaction_target 取值（0xFF = 无效索引）。
constexpr uint8_t kNoReactionTarget = 0xFF;
// reaction_ticks 上限（长时间看到同一敌人时的封顶，避免无意义增长）。
constexpr uint16_t kMaxReactionTicks = 1000;

// 机器人视觉反应计时：每 tick 用几何视线（vision.h）判断最近的可见敌人，
// 维护 PlayerState::reaction_target / reaction_ticks（连续看到该敌人的 tick 数）。
// 纯函数、确定性（无时钟/随机）；结果进 WorldState 并参与 HashState，
// BotDecide 用 reaction_ticks 与 RoundDef::bot_reaction_ticks 比较决定能否开火。
void UpdateReactions(WorldState& state, const MapDef* map);

} // namespace recoil
