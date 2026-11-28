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

// 几何视线本体：调用方负责索引/存活校验，这里只做遮挡计算。
bool TraceLineOfSight(const WorldState& state, const MapDef* map, uint8_t viewer, uint8_t target) {
    return !SegmentBlocked(state, map, EyePosition(state.players[viewer]),
                           ChestPosition(state.players[target]));
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
    return TraceLineOfSight(state, map, viewer, target);
}

bool Sees(const WorldState& state, const MapDef* map, uint8_t viewer, uint8_t target) {
    if (viewer >= state.player_count || target >= state.player_count || viewer == target) {
        return false;
    }
    if (!state.players[viewer].alive || !state.players[target].alive) {
        return false;
    }
    const VisibilityMemo& memo = state.visibility;
    if (memo.tick == state.tick) {
        const uint8_t cell = memo.cells[viewer][target];
        if (cell != 0) {
            return cell == 1;
        }
    }
    // 未命中不写回：本函数是 const 契约，调用方可能并发读同一状态。
    return TraceLineOfSight(state, map, viewer, target);
}

bool SeesMemo(WorldState& state, const MapDef* map, uint8_t viewer, uint8_t target) {
    if (viewer >= state.player_count || target >= state.player_count || viewer == target) {
        return false;
    }
    VisibilityMemo& memo = state.visibility;
    if (memo.tick != state.tick) {
        memo.cells = {};
        memo.tick = state.tick;
    }
    if (!state.players[viewer].alive || !state.players[target].alive) {
        return false;
    }
    uint8_t& cell = memo.cells[viewer][target];
    if (cell == 0) {
        cell = TraceLineOfSight(state, map, viewer, target) ? 1 : 2;
    }
    return cell == 1;
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
        if (!Sees(state, map, viewer, i)) {
            continue;
        }
        best_dist = dist;
        best = i;
    }
    return best;
}

} // namespace recoil
