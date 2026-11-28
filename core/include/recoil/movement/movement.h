#pragma once

#include <span>

#include "recoil/sim/state.h"
#include "recoil/world/map.h"
#include "recoil/world/trace.h"

namespace recoil {

struct MovementDef {
    float max_speed = 250.0f;
    float walk_speed = 130.0f;
    float duck_speed = 85.0f;
    float accelerate = 5.5f;
    float air_accelerate = 12.0f;
    float air_speed_cap = 30.0f;
    float friction = 5.2f;
    float stop_speed = 80.0f;
    float gravity = 800.0f;
    // 跳高 72u：v = sqrt(2 * 800 * 72) ≈ 339.411。64u 箱/72u 台面均可直接上（旧 301.993 只有
    // 57u）。
    float jump_impulse = 339.4113f;
    float tick_dt = 1.0f / 64.0f;
    float half_width = 16.0f;
    float stand_height = 72.0f;
    float duck_height = 54.0f;
    float eye_height = 64.0f;
    // 蹲伏眼高：射击原点/视线随蹲伏下移（Source VEC_DUCK_VIEW 46）。
    float duck_eye_height = 46.0f;
};

// 纯函数：根据 stored wish/速度/地图推进一个 tick 的移动与碰撞。
// blockers：其他玩家的 AABB（由 Step 组装，movement 不依赖 WorldState）。
void StepMovement(PlayerState& player, const MovementDef& def, const MapDef* map,
                  std::span<const Aabb> blockers = {});

} // namespace recoil
