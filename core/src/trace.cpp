#include "recoil/world/trace.h"

#include <algorithm>
#include <cmath>

namespace recoil {
namespace {

constexpr float kParallel = 1.0e-6f;
constexpr float kInfinity = 1.0e30f;
constexpr float kEpsilon = 1.0e-4f;

struct BoxSweep {
    bool hit = false;
    bool startsolid = false;
    float fraction = 1.0f;
    Vec3 normal{};
};

// 轴对齐 slab：o + d*t 与 [lo,hi] 的进入/离开比例。
BoxSweep SweepBox(const Vec3& o, const Vec3& d, const Vec3& lo, const Vec3& hi) {
    BoxSweep out;
    const float origin[3] = {o.x, o.y, o.z};
    const float delta[3] = {d.x, d.y, d.z};
    const float lower[3] = {lo.x, lo.y, lo.z};
    const float upper[3] = {hi.x, hi.y, hi.z};
    float enter = -kInfinity;
    float exit = kInfinity;
    int axis = -1;
    float sign = 0.0f;
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(delta[i]) < kParallel) {
            // 贴面视为在盒外：站在平台顶面时水平移动不应被判成 startsolid。
            if (origin[i] <= lower[i] + kEpsilon || origin[i] >= upper[i] - kEpsilon) {
                return out;
            }
            continue;
        }
        const float inv = 1.0f / delta[i];
        float t0 = (lower[i] - origin[i]) * inv;
        float t1 = (upper[i] - origin[i]) * inv;
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        if (t0 > enter) {
            enter = t0;
            axis = i;
            sign = delta[i] > 0.0f ? -1.0f : 1.0f;
        }
        exit = std::min(exit, t1);
        if (enter > exit) {
            return out;
        }
    }
    if (exit <= 0.0f) {
        return out;
    }
    out.startsolid = enter < 0.0f;
    const float fraction = out.startsolid ? 0.0f : enter;
    if (fraction > 1.0f) {
        return out;
    }
    out.hit = true;
    out.fraction = fraction;
    if (!out.startsolid && axis >= 0) {
        if (axis == 0) {
            out.normal = {sign, 0.0f, 0.0f};
        } else if (axis == 1) {
            out.normal = {0.0f, sign, 0.0f};
        } else {
            out.normal = {0.0f, 0.0f, sign};
        }
    }
    return out;
}

} // namespace

TraceHit TraceHull(const MapDef& map, const Aabb& hull, const Vec3& from, const Vec3& delta,
                   const TraceFilter& filter) {
    TraceHit best;
    TraceHit start;
    if ((filter.mask & kTraceWorld) == 0) {
        return best;
    }
    // 扫掠体包围盒预剔除：线段 AABB 按 hull 的 min/max 扩张后，与墙不相交的盒子无需进 slab 计算。
    // 只是加速，不改变判定：被剔除的盒子在 SweepBox 里本来也必然 miss。
    const Vec3 to = from + delta;
    const Vec3 sweep_lo{std::min(from.x, to.x) + hull.min.x, std::min(from.y, to.y) + hull.min.y,
                        std::min(from.z, to.z) + hull.min.z};
    const Vec3 sweep_hi{std::max(from.x, to.x) + hull.max.x, std::max(from.y, to.y) + hull.max.y,
                        std::max(from.z, to.z) + hull.max.z};
    for (int i = 0; i < map.wall_count; ++i) {
        if (i == filter.ignore_brush) {
            continue;
        }
        const Wall& wall = map.walls[i];
        if (wall.max.x < sweep_lo.x || wall.min.x > sweep_hi.x || wall.max.y < sweep_lo.y ||
            wall.min.y > sweep_hi.y || wall.max.z < sweep_lo.z || wall.min.z > sweep_hi.z) {
            continue;
        }
        const Vec3 lo = wall.min - hull.max;
        const Vec3 hi = wall.max - hull.min;
        const BoxSweep sweep = SweepBox(from, delta, lo, hi);
        if (!sweep.hit) {
            continue;
        }
        if (sweep.startsolid) {
            if (!start.hit) {
                start.hit = true;
                start.startsolid = true;
                start.fraction = 0.0f;
                start.point = from;
                start.brush = i;
                start.surface = wall.surface;
            }
            continue;
        }
        if (!best.hit || sweep.fraction < best.fraction) {
            best.hit = true;
            best.startsolid = false;
            best.fraction = sweep.fraction;
            best.point = from + delta * sweep.fraction;
            best.normal = sweep.normal;
            best.brush = i;
            best.surface = wall.surface;
        }
    }
    if (start.hit) {
        return start;
    }
    if (!best.hit) {
        best.fraction = 1.0f;
        best.point = from + delta;
    }
    return best;
}

TraceHit TraceRay(const MapDef& map, const Vec3& from, const Vec3& to, const TraceFilter& filter) {
    const Vec3 delta = to - from;
    return TraceHull(map, Aabb{}, from, delta, filter);
}

bool TraceOverlap(const MapDef& map, const Aabb& hull, const Vec3& from,
                  const TraceFilter& filter) {
    if ((filter.mask & kTraceWorld) == 0) {
        return false;
    }
    const Vec3 lo{hull.min.x + from.x, hull.min.y + from.y, hull.min.z + from.z};
    const Vec3 hi{hull.max.x + from.x, hull.max.y + from.y, hull.max.z + from.z};
    for (int i = 0; i < map.wall_count; ++i) {
        if (i == filter.ignore_brush) {
            continue;
        }
        const Wall& wall = map.walls[i];
        if (lo.x < wall.max.x - kEpsilon && hi.x > wall.min.x + kEpsilon &&
            lo.y < wall.max.y - kEpsilon && hi.y > wall.min.y + kEpsilon &&
            lo.z < wall.max.z - kEpsilon && hi.z > wall.min.z + kEpsilon) {
            return true;
        }
    }
    return false;
}

} // namespace recoil
