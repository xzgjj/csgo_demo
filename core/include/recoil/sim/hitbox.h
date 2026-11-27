#pragma once

#include "recoil/base/vec3.h"
#include "recoil/sim/state.h"

namespace recoil {

// 命中盒分组：几何决定分组，伤害表按分组取倍率（不再用 z 高度猜）。
enum class HitGroup : uint8_t {
    None = 0,
    Head = 1,
    Chest = 2,
    Stomach = 3,
    Legs = 4,
};

struct PlayerHit {
    bool hit = false;
    float t = 0.0f; // 沿 dir 的距离比例（dir 按 max_range 归一化时单位为 u）
    HitGroup group = HitGroup::None;
    Vec3 point{};
};

// 射线 vs 玩家命中盒（yaw 旋转的 OBB，蹲伏整体下移）。
PlayerHit TracePlayerHitboxes(const PlayerState& target, const Vec3& origin, const Vec3& dir,
                              float max_range);

} // namespace recoil
