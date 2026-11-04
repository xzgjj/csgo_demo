#pragma once

#include "recoil/sim/state.h"
#include "recoil/world/map.h"

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
    float jump_impulse = 301.993f;
    float tick_dt = 1.0f / 64.0f;
    float half_width = 16.0f;
    float stand_height = 72.0f;
    float duck_height = 54.0f;
    float eye_height = 64.0f;
};

// 纯函数：根据 stored wish/速度/地图推进一个 tick 的移动与碰撞。
void StepMovement(PlayerState& player, const MovementDef& def, const MapDef* map);

} // namespace recoil
