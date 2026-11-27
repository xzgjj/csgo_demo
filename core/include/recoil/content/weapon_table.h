#pragma once

#include <cstdint>

#include "recoil/content/content.h"
#include "recoil/content/weapon_table.gen.h"
#include "recoil/sim/state.h"

namespace recoil {

// 按 WeaponId 查表；WeaponId::None 或未知 id 返回 nullptr（调用方回退 Content::rifle）。
const WeaponRow* FindWeaponRow(WeaponId id);

// 玩家当前激活槽的 WeaponId（active_slot 越界按主武器处理）。
WeaponId ActiveWeaponId(const PlayerState& player);

// 玩家当前激活武器的数值；WeaponId::None 时回退 content.rifle（兼容无武器状态）。
WeaponDef ActiveWeaponDef(const PlayerState& player, const Content& content);

// 槽位 -> 状态/武器 id 映射：weapon 始终描述 active_slot，另一槽在 sidearm。
inline WeaponState& WeaponSlotState(PlayerState& player, uint8_t slot) {
    return slot == player.active_slot ? player.weapon : player.sidearm;
}

inline uint8_t& WeaponSlotId(PlayerState& player, uint8_t slot) {
    return slot == 0 ? player.primary_id : player.secondary_id;
}

inline uint8_t WeaponSlotId(const PlayerState& player, uint8_t slot) {
    return slot == 0 ? player.primary_id : player.secondary_id;
}

// 武器价格（未知道具返回 0）与展示名（未知返回 "-"）。
uint16_t WeaponPrice(WeaponId id);
const char* WeaponName(WeaponId id);

} // namespace recoil
