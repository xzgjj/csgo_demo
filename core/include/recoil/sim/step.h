#pragma once

#include <cstdint>
#include <span>

#include "recoil/content/content.h"
#include "recoil/sim/command.h"
#include "recoil/sim/event.h"
#include "recoil/sim/state.h"
#include "recoil/world/map.h"

namespace recoil {

struct StepResult {
    WorldState state;
    EventList events;
};

// 纯函数：不读时钟/随机/IO；同输入必同输出。map 为 nullptr 时只有水平地板。
// mutable_map 非空时允许子弹对可破坏墙扣血/移除（与 map 指向同一对象；地图状态随射击演进）。
StepResult Step(const WorldState& prev, const Content& content, std::span<const Command> commands,
                const MapDef* map = nullptr, MapDef* mutable_map = nullptr);

// 规范化状态哈希（FNV-1a，覆盖 tick 与全部规则字段）。
uint64_t HashState(const WorldState& state);

} // namespace recoil
