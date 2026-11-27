#pragma once

#include <cstdint>

#include "recoil/world/map.h"

namespace recoil {

// 可破坏体（木箱/木门/玻璃）的运行时规则：只减 HP，HP 归零后从地图移除碰撞盒。
// HP=0 表示不可破坏（金属/石/混凝土/沙袋），不参与扣血。
enum class BreakableResult : uint8_t {
    None = 0,
    Damaged = 1,
    Destroyed = 2,
    InvalidIndex = 3,
    Indestructible = 4,
    InvalidDamage = 5,
};

struct BreakableReport {
    BreakableResult result = BreakableResult::None;
    uint16_t remaining_hp = 0;
    int wall_index = -1;
};

// 对指定墙施加伤害。成功扣血返回 Damaged；扣到 0 时 RemoveWall（swap-erase）并返回 Destroyed。
// 失败不改地图：非法索引 / HP=0 / damage<=0 都原样返回。
BreakableReport DamageWall(MapDef& map, int wall_index, int damage);

// 该墙是否可破坏（索引合法且 hp>0）。
bool IsBreakable(const MapDef& map, int wall_index);

} // namespace recoil
