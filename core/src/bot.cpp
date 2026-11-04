#include "recoil/ai/bot.h"

#include <cmath>
#include <vector>

#include "recoil/ai/nav.h"
#include "recoil/base/vec3.h"
#include "recoil/sim/grenades.h"

namespace recoil {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEyeHeight = 64.0f;
constexpr float kChestHeight = 36.0f;

struct EnemyPick {
    int nearest = -1;
    int visible = -1;
};

float YawTo(const Vec3& from, const Vec3& to) {
    const Vec3 d = to - from;
    return std::atan2(d.y, d.x) * (180.0f / kPi);
}

// 实体遮挡：eye→chest 线段与 MapDef AABB 是否相交（可见性用）。
bool WallBlocks(const MapDef* map, const Vec3& from, const Vec3& to) {
    if (map == nullptr) {
        return false;
    }
    const Vec3 d = to - from;
    for (int i = 0; i < map->wall_count; ++i) {
        const Wall& wall = map->walls[i];
        float t_min = 0.0f;
        float t_max = 1.0f;
        bool hit = true;
        for (int axis = 0; axis < 3 && hit; ++axis) {
            const float origin = axis == 0 ? from.x : (axis == 1 ? from.y : from.z);
            const float delta = axis == 0 ? d.x : (axis == 1 ? d.y : d.z);
            const float lo = axis == 0 ? wall.min.x : (axis == 1 ? wall.min.y : wall.min.z);
            const float hi = axis == 0 ? wall.max.x : (axis == 1 ? wall.max.y : wall.max.z);
            if (std::fabs(delta) < 1.0e-6f) {
                hit = origin >= lo && origin <= hi;
                continue;
            }
            const float inv = 1.0f / delta;
            float t0 = (lo - origin) * inv;
            float t1 = (hi - origin) * inv;
            if (t0 > t1) {
                const float swap = t0;
                t0 = t1;
                t1 = swap;
            }
            t_min = t0 > t_min ? t0 : t_min;
            t_max = t1 < t_max ? t1 : t_max;
            hit = t_min <= t_max;
        }
        if (hit && t_max > 0.0f && t_min < 1.0f) {
            return true;
        }
    }
    return false;
}

// 最近敌人用于推进目标；最近可见敌人用于开火（墙体/烟雾会挡住目标）。
EnemyPick PickEnemies(const WorldState& state, uint8_t self, const MapDef* map) {
    EnemyPick pick;
    const PlayerState& me = state.players[self];
    const Vec3 eye = me.position + Vec3{0.0f, 0.0f, kEyeHeight};
    float nearest_dist = 1.0e30f;
    float visible_dist = 1.0e30f;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        const PlayerState& other = state.players[i];
        if (i == self || !other.alive || other.team == me.team) {
            continue;
        }
        const float dist = Length(other.position - me.position);
        const Vec3 chest = other.position + Vec3{0.0f, 0.0f, kChestHeight};
        if (dist < nearest_dist) {
            nearest_dist = dist;
            pick.nearest = i;
        }
        if (dist < visible_dist && !SmokeBlocksSegment(state, eye, chest) &&
            !WallBlocks(map, eye, chest)) {
            visible_dist = dist;
            pick.visible = i;
        }
    }
    return pick;
}

Vec3 MoveAim(const WorldState& state, uint8_t self, int target, const MapDef* map) {
    const Vec3 position = state.players[self].position;
    const Vec3 goal = state.players[static_cast<size_t>(target)].position;
    if (map == nullptr) {
        return goal;
    }
    const std::vector<Vec3> path = NavPath(*map, position, goal);
    return path.empty() ? goal : path.front();
}

Command FireAt(const WorldState& state, uint8_t self, int target) {
    const Vec3 eye = state.players[self].position + Vec3{0.0f, 0.0f, kEyeHeight};
    const Vec3 goal =
        state.players[static_cast<size_t>(target)].position + Vec3{0.0f, 0.0f, kChestHeight};
    const Vec3 d = goal - eye;
    Command command;
    command.kind = CommandKind::Fire;
    command.player = self;
    command.yaw_degrees = std::atan2(d.y, d.x) * (180.0f / kPi);
    command.pitch_degrees = std::atan2(d.z, std::sqrt(d.x * d.x + d.y * d.y)) * (180.0f / kPi);
    return command;
}

} // namespace

Command BotDecide(const WorldState& state, const Content& content, uint8_t self,
                  const MapDef* map) {
    (void)content;
    Command command;
    command.kind = CommandKind::Stop;
    command.player = self;
    if (self >= state.player_count || !state.players[self].alive) {
        return command;
    }
    const EnemyPick pick = PickEnemies(state, self, map);
    if (pick.nearest < 0) {
        return command;
    }
    if (pick.visible >= 0) {
        return FireAt(state, self, pick.visible);
    }
    command.kind = CommandKind::Move;
    command.yaw_degrees =
        YawTo(state.players[self].position, MoveAim(state, self, pick.nearest, map));
    command.move_forward = 1.0f;
    return command;
}

} // namespace recoil
