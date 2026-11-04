#pragma once

#include "recoil/content/content.h"
#include "recoil/sim/command.h"
#include "recoil/sim/state.h"

namespace recoil {

// Bot 决策（Command 生产者）：返回该 tick 的动作命令。
Command BotDecide(const WorldState& state, const Content& content, uint8_t self);

} // namespace recoil
