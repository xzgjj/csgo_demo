#pragma once

#include <cstdint>
#include <span>

#include "recoil/content/content.h"
#include "recoil/sim/command.h"
#include "recoil/sim/event.h"
#include "recoil/sim/state.h"

namespace recoil {

struct StepResult {
    WorldState state;
    EventList events;
};

// 纯函数：不读时钟/随机/IO；同输入必同输出。
StepResult Step(const WorldState& prev, const Content& content, std::span<const Command> commands);

// 规范化状态哈希（FNV-1a，覆盖 tick 之外的确定性字段）。
uint64_t HashState(const WorldState& state);

} // namespace recoil
