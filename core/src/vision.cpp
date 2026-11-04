#include "recoil/ai/vision.h"

#include <cmath>

#include "recoil/sim/grenades.h"

namespace recoil {
namespace {

// 线段与轴对齐盒求交：[0,1] 参数域，含端点接触视为遮挡。
bool SegmentHitsAabb(const Vec3& from, const Vec3& to, const Wall& wall) {
    const Vec3 delta = to - from;
    float t_min = 0.0f;
    float t_max = 1.0f;
    for (int axis = 0; axis < 3; ++axis) {
        const float origin = axis == 0 ? from.x : (axis == 1 ? from.y : from.z);
        const float d = axis == 0 ? delta.x : (axis == 1 ? delta.y : delta.z);
        const float lo = axis == 0 ? wall.min.x : (axis == 1 ? wall.min.y : wall.min.z);
        const float hi = axis == 0 ? wall.max.x : (axis == 1 ? wall.max.y : wall.max.z);
        if (std::fabs(d) < 1.0e-6f) {
            if (origin < lo || origin > hi) {
                return false;
            }
            continue;
        }
        const float inv = 1.0f / d;
        float t0 = (lo - origin) * inv;
        float t1 = (hi - origin) * inv;
        if (t0 > t1) {
            const float swap = t0;
            t0 = t1;
            t1 = swap;
        }
        t_min = t0 > t_min ? t0 : t_min;
        t_max = t1 < t_max ? t1 : t_max;
        if (t_min > t_max) {
            return false;
        }
    }
    return t_max > 0.0f && t_min < 1.0f;
}

Vec3 EyePosition(const PlayerState& player) {
    return player.position + Vec3{0.0f, 0.0f, kEyeHeight};
}

Vec3 ChestPosition(const PlayerState& player) {
    return player.position + Vec3{0.0f, 0.0f, kChestHeight};
}

} // namespace

bool SegmentBlockedByWalls(const MapDef* map, const Vec3& from, const Vec3& to) {
    if (map == nullptr) {
        return false;
    }
    for (int i = 0; i < map->wall_count; ++i) {
        if (SegmentHitsAabb(from, to, map->walls[i])) {
            return true;
        }
    }
    return false;
}

bool SegmentBlockedBySmoke(const WorldState& state, const Vec3& from, const Vec3& to) {
    return SmokeBlocksSegment(state, from, to);
}

bool SegmentBlocked(const WorldState& state, const MapDef* map, const Vec3& from, const Vec3& to) {
    return SegmentBlockedBySmoke(state, from, to) || SegmentBlockedByWalls(map, from, to);
}

bool HasLineOfSight(const WorldState& state, const MapDef* map, uint8_t viewer, uint8_t target) {
    if (viewer >= state.player_count || target >= state.player_count || viewer == target) {
        return false;
    }
    const PlayerState& from_player = state.players[viewer];
    const PlayerState& to_player = state.players[target];
    if (!from_player.alive || !to_player.alive) {
        return false;
    }
    return !SegmentBlocked(state, map, EyePosition(from_player), ChestPosition(to_player));
}

int NearestVisibleEnemy(const WorldState& state, const MapDef* map, uint8_t viewer) {
    if (viewer >= state.player_count || !state.players[viewer].alive) {
        return -1;
    }
    int best = -1;
    float best_dist = 1.0e30f;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        if (i == viewer) {
            continue;
        }
        const PlayerState& other = state.players[i];
        if (!other.alive || other.team == state.players[viewer].team) {
            continue;
        }
        const float dist = Length(other.position - state.players[viewer].position);
        if (dist >= best_dist) {
            continue;
        }
        if (!HasLineOfSight(state, map, viewer, i)) {
            continue;
        }
        best_dist = dist;
        best = i;
    }
    return best;
}

} // namespace recoil
