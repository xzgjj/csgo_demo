#include "recoil/world/placement.h"

#include <cmath>

namespace recoil {
namespace {

// 8 个方向 + 原点。顺序固定（东、东北、北、西北、西、西南、南、东南）保证确定性。
constexpr float kDirections[9][2] = {
    {1.0f, 0.0f},  {0.70710678f, 0.70710678f},   {0.0f, 1.0f},  {-0.70710678f, 0.70710678f},
    {-1.0f, 0.0f}, {-0.70710678f, -0.70710678f}, {0.0f, -1.0f}, {0.70710678f, -0.70710678f},
    {0.0f, 0.0f},
};

bool Overlaps(const Wall& a, const Vec3& feet, float half_width, float height) {
    return feet.x + half_width > a.min.x && feet.x - half_width < a.max.x &&
           feet.y + half_width > a.min.y && feet.y - half_width < a.max.y &&
           feet.z + height > a.min.z && feet.z < a.max.z;
}

} // namespace

bool SpotFree(const MapDef& map, const Vec3& feet, float half_width, float height) {
    if (!std::isfinite(feet.x) || !std::isfinite(feet.y) || !std::isfinite(feet.z)) {
        return false;
    }
    const int count = map.wall_count < kMaxWalls ? map.wall_count : kMaxWalls;
    for (int i = 0; i < count; ++i) {
        if (map.walls[i].max.z <= feet.z) {
            continue; // 纯地板/脚下台阶顶面：不挡站立
        }
        if (Overlaps(map.walls[i], feet, half_width, height)) {
            return false;
        }
    }
    return true;
}

Vec3 FindFreeSpot(const MapDef& map, const Vec3& desired, float half_width, float height,
                  float max_radius, float step) {
    if (SpotFree(map, desired, half_width, height)) {
        return desired;
    }
    const float stride = step > 0.0f ? step : kPlacementStep;
    const float limit = max_radius > 0.0f ? max_radius : kPlacementMaxRadius;
    for (int ring = 1; static_cast<float>(ring) * stride <= limit + 0.01f; ++ring) {
        const float radius = static_cast<float>(ring) * stride;
        for (int direction = 0; direction < 8; ++direction) {
            const Vec3 candidate{desired.x + kDirections[direction][0] * radius,
                                 desired.y + kDirections[direction][1] * radius, desired.z};
            if (SpotFree(map, candidate, half_width, height)) {
                return candidate;
            }
        }
    }
    return desired;
}

} // namespace recoil
