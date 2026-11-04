#pragma once

#include <cstdint>

#include "recoil/base/vec3.h"

namespace recoil {

constexpr int kMaxWalls = 64;

struct Wall {
    Vec3 min{};
    Vec3 max{};
};

struct MapDef {
    Wall walls[kMaxWalls]{};
    int wall_count = 0;
    float floor_z = 0.0f;
};

} // namespace recoil
