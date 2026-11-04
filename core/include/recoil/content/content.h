#pragma once

#include <cstdint>

#include "recoil/movement/movement.h"

namespace recoil {

// 数值为 CS:GO 参考基线（AK-47 量级）；后续由 content/defs JSON 加载并校准。
struct WeaponDef {
    uint16_t damage = 36;
    uint16_t magazine = 30;
    uint16_t reserve = 90;
    uint16_t fire_interval_ticks = 6;
    uint16_t reload_ticks = 156;
    float armor_ratio = 0.775f;
    float range = 8192.0f;
};

struct Content {
    WeaponDef rifle{};
    MovementDef movement{};
};

Content DefaultContent();

} // namespace recoil
