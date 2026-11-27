#include "recoil/ai/vision.h"

#include "recoil/sim/grenades.h"
#include "recoil/world/trace.h"

namespace recoil {
namespace {

Vec3 EyePosition(const PlayerState& player) {
    return player.position + Vec3{0.0f, 0.0f, player.ducking ? kDuckEyeHeight : kEyeHeight};
}

Vec3 ChestPosition(const PlayerState& player) {
    return player.position + Vec3{0.0f, 0.0f, player.ducking ? kDuckChestHeight : kChestHeight};
}

} // namespace

bool SegmentBlockedByWalls(const MapDef* map, const Vec3& from, const Vec3& to) {
    if (map == nullptr) {
        return false;
    }
    return TraceRay(*map, from, to).hit;
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
