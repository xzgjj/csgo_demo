#pragma once

#include <cmath>
#include <cstdint>
#include <span>

#include "recoil/base/vec3.h"

namespace recoil {

// 内核墙体容量：de_dust2 151 盒（147 碰撞）仍留余量；单次加载可用更小预算验证拒绝路径。
constexpr int kMaxWalls = 1024;

// 轴对齐盒（世界/玩家 hull 共用）；放在 map.h 以便 LoadWalls 直接接收导航范围。
struct Aabb {
    Vec3 min{};
    Vec3 max{};
};

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
    SlopeTooSteep = 6, // 坡道超过 45°（可走坡面上限，与 Source 的 MAX_SLOPE 一致）
};

struct Wall {
    Vec3 min{};
    Vec3 max{};
    Surface surface = Surface::Concrete;
    // 只支持 90° 倍数：0/90/180/270。加载期把 OBB 归一成等价 AABB（宽深互换）。
    float yaw_degrees = 0.0f;
    // 可破坏物 HP（0 表示不可破坏）；与 Wall 同生命周期，破坏后整盒移除。
    uint16_t hp = 0;
};

// 可走坡道：不是盒体，而是一张斜面（水平矩形 + 高度插值）。
// 单独存放而不是混进 walls，原因：斜面若当成 AABB，射线/导航会把它判成实心墙，
// 玩家站在坡中会被“推出”，这正是「斜坡做成台阶」的根因（见 docs/spec-debug2.md §4）。
constexpr int kMaxRamps = 64;

struct Ramp {
    Aabb bounds{};                // min.z = 低端标高，max.z = 高端标高
    int8_t axis = 0;              // 0 = 沿 +x 变化，1 = 沿 +y 变化
    bool ascends_positive = true; // 坐标增大时是否升高
};

// 坡面标高：把玩家中心投影到斜面上（越界点按最近端夹取）。
inline float RampFloorAt(const Ramp& ramp, float x, float y) {
    const float span_x = ramp.bounds.max.x - ramp.bounds.min.x;
    const float span_y = ramp.bounds.max.y - ramp.bounds.min.y;
    float t = 0.0f;
    if (ramp.axis == 0) {
        t = span_x > 1.0e-6f ? (x - ramp.bounds.min.x) / span_x : 0.0f;
    } else {
        t = span_y > 1.0e-6f ? (y - ramp.bounds.min.y) / span_y : 0.0f;
    }
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    if (!ramp.ascends_positive) {
        t = 1.0f - t;
    }
    return ramp.bounds.min.z + t * (ramp.bounds.max.z - ramp.bounds.min.z);
}

struct MapDef;

// 统一入口：校验有限值/尺寸/表面/旋转与容量；失败时不修改地图。
inline MapError AddWall(MapDef& map, const Wall& wall);

// 载入报告：written 成功写入数，rejected 被跳过数（非法盒/超容量），first_error 第一条原因。
struct MapLoadReport {
    int written = 0;
    int rejected = 0;
    MapError first_error = MapError::None;
};

// 跳过非法盒并记录首因（宿主加载与批量加载共用，避免两处复制同一段计数逻辑）。
inline void NoteReject(MapLoadReport& report, MapError error) {
    ++report.rejected;
    if (report.first_error == MapError::None) {
        report.first_error = error;
    }
}

struct MapDef {
    Wall walls[kMaxWalls]{};
    // 每面墙的剩余 HP（0 = 不可破坏）；破坏后 wall_count 减一，swap-erase 同步搬移。
    uint16_t wall_hp[kMaxWalls]{};
    int wall_count = 0;
    // 本次加载的墙体预算（默认 kMaxWalls）；容量测试可传 512 验证拒绝路径。
    int wall_limit = kMaxWalls;
    float floor_z = 0.0f;
    // 导航网格覆盖范围（水平面）。de_ash 默认 ±2048；de_dust2 由 layout bounds 注入 ±2300。
    Vec3 nav_min{-2048.0f, -2048.0f, 0.0f};
    Vec3 nav_max{2048.0f, 2048.0f, 256.0f};
    // 出生中心；custom_spawns=false 时回合逻辑沿用 de_ash 常量（回归不变）。
    Vec3 spawn_t{0.0f, -1700.0f, 0.0f};
    Vec3 spawn_ct{0.0f, 1700.0f, 0.0f};
    bool custom_spawns = false;
    // 布局里的 5 个出生槽（sim 坐标）；custom_slots=false 时回合逻辑沿用 64u 横排。
    Vec3 spawn_slots_t[5]{};
    Vec3 spawn_slots_ct[5]{};
    bool custom_slots = false;
    // 可走坡道（不成盒、不参与射线阻挡）。
    Ramp ramps[kMaxRamps]{};
    int ramp_count = 0;
};

inline bool IsFiniteVec(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// 合法 yaw：0/90/180/270（含浮点相等容差）；返回归一后的 0 或 90（180 视同 0）。
inline bool NormalizeYaw(float yaw_degrees, float& out_quarter_turns) {
    constexpr float kEps = 1.0e-3f;
    const float turns = yaw_degrees / 90.0f;
    const float rounded = std::round(turns);
    if (std::fabs(turns - rounded) > kEps) {
        return false;
    }
    out_quarter_turns = rounded;
    return true;
}

// 90° 倍数旋转：绕盒中心交换 x/y 半宽；非 90° 返回 RotatedBrushUnsupported。
inline Wall RotateToAabb(const Wall& wall, float quarter_turns) {
    Wall out = wall;
    out.yaw_degrees = 0.0f;
    const int turns = static_cast<int>(std::fmod(std::fabs(quarter_turns), 4.0f));
    if (turns == 1 || turns == 3) {
        const Vec3 center{(wall.min.x + wall.max.x) * 0.5f, (wall.min.y + wall.max.y) * 0.5f,
                          (wall.min.z + wall.max.z) * 0.5f};
        const float half_x = (wall.max.x - wall.min.x) * 0.5f;
        const float half_y = (wall.max.y - wall.min.y) * 0.5f;
        out.min = {center.x - half_y, center.y - half_x, wall.min.z};
        out.max = {center.x + half_y, center.y + half_x, wall.max.z};
    }
    return out;
}

inline MapError AddWall(MapDef& map, const Wall& wall) {
    if (!IsFiniteVec(wall.min) || !IsFiniteVec(wall.max) || !std::isfinite(wall.yaw_degrees)) {
        return MapError::NonFinite;
    }
    float turns = 0.0f;
    if (!NormalizeYaw(wall.yaw_degrees, turns)) {
        return MapError::RotatedBrushUnsupported;
    }
    const Wall aabb = RotateToAabb(wall, turns);
    if (aabb.min.x >= aabb.max.x || aabb.min.y >= aabb.max.y || aabb.min.z >= aabb.max.z) {
        return MapError::InvalidExtent;
    }
    if (static_cast<uint8_t>(aabb.surface) > static_cast<uint8_t>(Surface::Fabric)) {
        return MapError::InvalidSurface;
    }
    const int limit = map.wall_limit > 0 ? map.wall_limit : kMaxWalls;
    if (map.wall_count >= limit) {
        return MapError::TooManyWalls;
    }
    const int slot = map.wall_count;
    map.walls[slot] = aabb;
    map.wall_hp[slot] = aabb.hp;
    ++map.wall_count;
    return MapError::None;
}

// 坡道：只校验有限值/尺寸/坡度（<= 45°），不占 walls 容量；失败时不修改地图。
inline MapError AddRamp(MapDef& map, const Aabb& bounds, int axis, bool ascends_positive) {
    if (!IsFiniteVec(bounds.min) || !IsFiniteVec(bounds.max)) {
        return MapError::NonFinite;
    }
    if (axis != 0 && axis != 1) {
        return MapError::InvalidExtent;
    }
    if (bounds.min.x >= bounds.max.x || bounds.min.y >= bounds.max.y) {
        return MapError::InvalidExtent;
    }
    const float rise = bounds.max.z - bounds.min.z;
    const float run = (axis == 0) ? (bounds.max.x - bounds.min.x) : (bounds.max.y - bounds.min.y);
    if (rise <= 0.0f || run <= 0.0f) {
        return MapError::InvalidExtent;
    }
    if (rise > run + 1.0e-3f) {
        return MapError::SlopeTooSteep;
    }
    if (map.ramp_count >= kMaxRamps) {
        return MapError::TooManyWalls;
    }
    Ramp ramp;
    ramp.bounds = bounds;
    ramp.axis = static_cast<int8_t>(axis);
    ramp.ascends_positive = ascends_positive;
    map.ramps[map.ramp_count] = ramp;
    ++map.ramp_count;
    return MapError::None;
}

// 批量载入：跳过非法盒并计数（不静默截断）；导航范围随图注入。
inline MapLoadReport LoadWalls(MapDef& map, std::span<const Wall> walls, const Aabb& nav_bounds,
                               int wall_limit = kMaxWalls) {
    map = MapDef{};
    map.wall_limit = wall_limit > 0 ? wall_limit : kMaxWalls;
    map.nav_min = nav_bounds.min;
    map.nav_max = nav_bounds.max;
    MapLoadReport report;
    for (const Wall& wall : walls) {
        const MapError error = AddWall(map, wall);
        if (error != MapError::None) {
            NoteReject(report, error);
            continue;
        }
        ++report.written;
    }
    return report;
}

// 破坏后移除碰撞盒：从尾部搬一块覆盖目标（保持 O(1)），同步 HP；返回是否移除。
inline bool RemoveWall(MapDef& map, int index) {
    if (index < 0 || index >= map.wall_count) {
        return false;
    }
    const int last = map.wall_count - 1;
    if (index != last) {
        map.walls[index] = map.walls[last];
        map.wall_hp[index] = map.wall_hp[last];
    }
    map.walls[last] = Wall{};
    map.wall_hp[last] = 0;
    --map.wall_count;
    return true;
}

} // namespace recoil
