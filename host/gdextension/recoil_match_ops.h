#pragma once

#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <vector>

#include "recoil/ai/nav.h"
#include "recoil/sim/state.h"
#include "recoil/world/map.h"

namespace recoil_host {

using godot::PackedVector3Array;
using godot::Vector3;

// 宿主查询/调试辅助：只读 MapDef/WorldState，或显式写入玩家位置（验收脚本用）。
inline int WallCount(const recoil::MapDef& map) {
    return map.wall_count;
}

inline int AliveCount(const recoil::WorldState& state, int team) {
    int count = 0;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        if (state.players[i].alive && static_cast<int>(state.players[i].team) == team) {
            ++count;
        }
    }
    return count;
}

inline int HasBomb(const recoil::WorldState& state, int player) {
    return state.players[player].has_bomb ? 1 : 0;
}

inline int NavPointCount(const recoil::MapDef& map, float fx, float fy, float fz, float tx,
                         float ty, float tz) {
    return static_cast<int>(recoil::NavPath(map, {fx, fy, fz}, {tx, ty, tz}).size());
}

inline PackedVector3Array NavPoints(const recoil::MapDef& map, float fx, float fy, float fz,
                                    float tx, float ty, float tz) {
    const std::vector<recoil::Vec3> path = recoil::NavPath(map, {fx, fy, fz}, {tx, ty, tz});
    PackedVector3Array points;
    for (const recoil::Vec3& point : path) {
        points.push_back(Vector3(point.x, point.y, point.z));
    }
    return points;
}

inline void SetPlayerPosition(recoil::WorldState& state, int player, float x, float y, float z) {
    if (player < 0 || player >= static_cast<int>(state.player_count)) {
        return;
    }
    state.players[player].position = {x, y, z};
    state.players[player].velocity = {};
    state.players[player].on_ground = false;
    state.players[player].alive = true;
    state.players[player].health = 100;
}

} // namespace recoil_host
