#pragma once

#include "recoil/content/content.h"
#include "recoil/sim/command.h"
#include "recoil/sim/event.h"
#include "recoil/sim/state.h"

namespace recoil {

void ApplyBombCommand(WorldState& state, const Content& content, const Command& command,
                      EventList& events);

// C4 计时、拆包进度、爆炸/拆包成功后的回合结算。
void StepBomb(WorldState& state, const Content& content, EventList& events);

} // namespace recoil
