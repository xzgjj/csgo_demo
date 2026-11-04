#pragma once

#include <cstdint>

#include "recoil/content/content.h"
#include "recoil/sim/command.h"
#include "recoil/sim/event.h"
#include "recoil/sim/state.h"

namespace recoil {

void AwardMoney(PlayerState& player, uint16_t amount, const RoundDef& def);

// 回合状态机：冻结 → 进行中 → 结算 → 重置；含胜负判定与经济结算。
void AdvanceRound(WorldState& state, const Content& content, EventList& events);

// 购买（当前仅护甲）；只允许冻结期。
void ApplyBuy(WorldState& state, const Content& content, const Command& command, EventList& events);

} // namespace recoil
