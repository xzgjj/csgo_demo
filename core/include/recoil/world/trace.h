#pragma once

#include <cstdint>

#include "recoil/base/vec3.h"
#include "recoil/world/map.h"

namespace recoil {

// 查询掩码：当前只实现 World；Players/Dynamic/Smoke 预留给上层组合查询。
enum TraceMask : uint32_t {
    kTraceWorld = 1u << 0,
    kTracePlayers = 1u << 1,
    kTraceDynamic = 1u << 2,
    kTraceSmoke = 1u << 3,
};
constexpr uint32_t kTraceAll = kTraceWorld | kTracePlayers | kTraceDynamic | kTraceSmoke;

struct TraceFilter {
    uint32_t mask = kTraceWorld;
    int32_t ignore_brush = -1;
};

// 查询结果是“正常值”，不是错误：miss/startsolid 都通过字段表达。
struct TraceHit {
    bool hit = false;
    bool startsolid = false;
    bool allsolid = false;
    float fraction = 1.0f; // [0,1]
    Vec3 point{};          // 命中点（hull 中心接触位置）
    Vec3 normal{};         // 入口面法线；startsolid 时为 0
    int32_t brush = -1;
    Surface surface = Surface::None;
};

// 射线：from→to；hull 为零。零长度返回 startsolid（不做除法）。
TraceHit TraceRay(const MapDef& map, const Vec3& from, const Vec3& to,
                  const TraceFilter& filter = {});

// 扫掠：hull 相对中心的 min/max（如玩家 {-16,-16,0}..{16,16,72}）。
// 返回 hull 中心可前进的比例；fraction=0 + startsolid 表示起点已重叠。
TraceHit TraceHull(const MapDef& map, const Aabb& hull, const Vec3& from, const Vec3& delta,
                   const TraceFilter& filter = {});

// 重叠：hull 放在 from 处是否与任一 brush 相交（不含仅贴面）。
bool TraceOverlap(const MapDef& map, const Aabb& hull, const Vec3& from,
                  const TraceFilter& filter = {});

} // namespace recoil
