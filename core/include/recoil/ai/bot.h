#pragma once

#include "recoil/content/content.h"
#include "recoil/sim/command.h"
#include "recoil/sim/state.h"
#include "recoil/world/map.h"

namespace recoil {

// Bot 决策（Command 生产者）：返回该 tick 的动作命令。
// 有可见敌人时开火；否则沿 NavPath 的下一航点推进（map 为空时直线推进）。
Command BotDecide(const WorldState& state, const Content& content, uint8_t self,
                  const MapDef* map = nullptr);

} // namespace recoil
