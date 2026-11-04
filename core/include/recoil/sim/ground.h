#pragma once

#include <cstdint>

#include "recoil/base/vec3.h"
#include "recoil/sim/command.h"
#include "recoil/sim/event.h"
#include "recoil/sim/state.h"

namespace recoil {

// 拾取半径（客户端交互距离，CS:GO 约 64u）。
constexpr float kPickupRadius = 64.0f;

// 最近的可用地面武器索引；没有返回 -1（平局取索引小的，保证确定性）。
int NearestGroundWeapon(const WorldState& state, const Vec3& position, float radius);

// 当前地面掉落物数量。
int GroundWeaponCount(const WorldState& state);

// 玩家死亡掉落：把主武器（ID + 剩余弹匣/备弹）放到死亡位置；无主武器不掉落。
// 掉落槽满时忽略（记录在返回值里：true=成功掉落）。
bool DropPrimaryOnDeath(WorldState& state, uint8_t player, EventList& events);

// 拾取命令：范围内取最近一件，主武器换成掉落武器（弹药取掉落时值），掉落物消失。
// 不在范围内/已满：写 CommandRejected(NoGroundWeapon)。
void ApplyPickup(WorldState& state, const Command& command, EventList& events);

} // namespace recoil
