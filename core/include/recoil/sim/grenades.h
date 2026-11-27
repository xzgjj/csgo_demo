#pragma once

#include "recoil/content/content.h"
#include "recoil/sim/command.h"
#include "recoil/sim/event.h"
#include "recoil/sim/state.h"
#include "recoil/world/map.h"

namespace recoil {

// 投掷（在 Step 命令阶段调用）。
void ThrowGrenade(WorldState& state, const Content& content, const Command& command,
                  EventList& events);

// 投掷物飞行/引信/爆炸效果 + 烟雾计时 + 致盲计时。
// map 非空时对墙体/楼板做扫掠与反弹；nullptr 保持旧的无墙落地行为（测试/空场景）。
void StepGrenades(WorldState& state, const Content& content, EventList& events,
                  const MapDef* map = nullptr);

// 烟雾是否阻挡线段（用于命中判定）。
bool SmokeBlocksSegment(const WorldState& state, const Vec3& from, const Vec3& to);

} // namespace recoil
