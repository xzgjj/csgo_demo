#pragma once

#include <cstdint>

#include "recoil/base/vec3.h"

namespace recoil {

// de_ash 派生盒子约 90 个碰撞盒；留 512 上限覆盖后续地图扩展。
constexpr int kMaxWalls = 512;

struct Wall {
    Vec3 min{};
    Vec3 max{};
};

struct MapDef {
    Wall walls[kMaxWalls]{};
    int wall_count = 0;
    float floor_z = 0.0f;
    // 导航网格覆盖范围（水平面；de_ash 默认 ±2048）。NavPath 用它生成均匀网格。
    Vec3 nav_min{-2048.0f, -2048.0f, 0.0f};
    Vec3 nav_max{2048.0f, 2048.0f, 256.0f};
};

} // namespace recoil
