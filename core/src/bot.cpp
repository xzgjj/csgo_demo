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

// 最近敌人用于推进目标；最近可见敌人用于开火（烟雾会挡住部分目标）。
EnemyPick PickEnemies(const WorldState& state, uint8_t self) {
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
        if (dist < visible_dist && !SmokeBlocksSegment(state, eye, chest)) {
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
    const EnemyPick pick = PickEnemies(state, self);
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
