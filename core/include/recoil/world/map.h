#pragma once

#include <cmath>
#include <cstdint>

#include "recoil/base/vec3.h"

namespace recoil {

// de_ash 派生盒子约 137 个碰撞盒；留 512 上限覆盖后续地图扩展。
constexpr int kMaxWalls = 512;

// 表面材质：决定弹孔/音效/是否可穿透（玻璃、木可穿；金属/混凝土阻挡）。
enum class Surface : uint8_t {
    Concrete = 0,
    Metal = 1,
    Wood = 2,
    Glass = 3,
    Dirt = 4,
    Fabric = 5,
    None = 255,
};

// 地图载入错误：只用于加载/校验阶段，不进 64Hz 循环。
enum class MapError : int32_t {
    None = 0,
    TooManyWalls = 1,
    InvalidExtent = 2,
    NonFinite = 3,
    InvalidSurface = 4,
    RotatedBrushUnsupported = 5,
};

struct Wall {
    Vec3 min{};
    Vec3 max{};
    Surface surface = Surface::Concrete;
    // 当前只支持轴对齐 brush；非 0 会在载入期拒绝，避免“画面转、碰撞不转”。
    float yaw_degrees = 0.0f;
};

struct MapDef;

// 统一入口：校验有限值/尺寸/表面/旋转与容量；失败时不修改地图。
inline MapError AddWall(MapDef& map, const Wall& wall);

struct MapDef {
    Wall walls[kMaxWalls]{};
    int wall_count = 0;
    float floor_z = 0.0f;
    // 导航网格覆盖范围（水平面；de_ash 默认 ±2048）。NavPath 用它生成均匀网格。
    Vec3 nav_min{-2048.0f, -2048.0f, 0.0f};
    Vec3 nav_max{2048.0f, 2048.0f, 256.0f};
};

inline bool IsFiniteVec(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline MapError AddWall(MapDef& map, const Wall& wall) {
    if (!IsFiniteVec(wall.min) || !IsFiniteVec(wall.max) || !std::isfinite(wall.yaw_degrees)) {
        return MapError::NonFinite;
    }
    if (wall.min.x >= wall.max.x || wall.min.y >= wall.max.y || wall.min.z >= wall.max.z) {
        return MapError::InvalidExtent;
    }
    if (wall.yaw_degrees != 0.0f) {
        return MapError::RotatedBrushUnsupported;
    }
    if (static_cast<uint8_t>(wall.surface) > static_cast<uint8_t>(Surface::Fabric)) {
        return MapError::InvalidSurface;
    }
    if (map.wall_count >= kMaxWalls) {
        return MapError::TooManyWalls;
    }
    map.walls[map.wall_count++] = wall;
    return MapError::None;
}

} // namespace recoil
