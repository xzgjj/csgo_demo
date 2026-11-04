#include "recoil/ai/bot.h"

#include <cmath>
#include <vector>

#include "recoil/ai/nav.h"
#include "recoil/ai/vision.h"
#include "recoil/base/vec3.h"

namespace recoil {
namespace {

constexpr float kPi = 3.14159265358979323846f;

struct EnemyPick {
    int nearest = -1;
    int visible = -1;
};

float YawTo(const Vec3& from, const Vec3& to) {
    const Vec3 d = to - from;
    return std::atan2(d.y, d.x) * (180.0f / kPi);
}

// 最近敌人用于推进目标；最近可见敌人用于开火（墙体/烟雾会挡住目标）。
EnemyPick PickEnemies(const WorldState& state, uint8_t self, const MapDef* map) {
    EnemyPick pick;
    const PlayerState& me = state.players[self];
    float nearest_dist = 1.0e30f;
    float visible_dist = 1.0e30f;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        const PlayerState& other = state.players[i];
        if (i == self || !other.alive || other.team == me.team) {
            continue;
        }
        const float dist = Length(other.position - me.position);
        if (dist < nearest_dist) {
            nearest_dist = dist;
            pick.nearest = i;
        }
        if (dist < visible_dist && HasLineOfSight(state, map, self, i)) {
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

Command FireAt(const WorldState& state, uint8_t self, int target, const Content& content) {
    const Vec3 eye = state.players[self].position + Vec3{0.0f, 0.0f, kEyeHeight};
    const Vec3 goal =
        state.players[static_cast<size_t>(target)].position + Vec3{0.0f, 0.0f, kChestHeight};
    const Vec3 d = goal - eye;
    Command command;
    command.kind = CommandKind::Fire;
    command.player = self;
    // 确定性瞄准误差：同一 tick/玩家/目标可复现，避免机器人“见面必中”。
    const int seed =
        (static_cast<int>(state.round.phase_ticks) * 37 + self * 101 + target * 17) % 201;
    const float jitter =
        (static_cast<float>(seed) / 100.0f - 1.0f) * content.round.bot_aim_error_deg;
    command.yaw_degrees = std::atan2(d.y, d.x) * (180.0f / kPi) + jitter;
    command.pitch_degrees =
        std::atan2(d.z, std::sqrt(d.x * d.x + d.y * d.y)) * (180.0f / kPi) + jitter * 0.35f;
    return command;
}

} // namespace

Command BotDecide(const WorldState& state, const Content& content, uint8_t self,
                  const MapDef* map) {
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
    // 反应延迟：必须连续看到同一个敌人 bot_reaction_ticks 个 tick 才能扣扳机
    // （状态由 Step 的 UpdateReactions 维护，确定性、无时钟）。
    const PlayerState& me = state.players[self];
    const bool ready = pick.visible >= 0 &&
                       me.reaction_target == static_cast<uint8_t>(pick.visible) &&
                       me.reaction_ticks >= content.round.bot_reaction_ticks;
    if (ready) {
        return FireAt(state, self, pick.visible, content);
    }
    command.kind = CommandKind::Move;
    command.yaw_degrees =
        YawTo(state.players[self].position, MoveAim(state, self, pick.nearest, map));
    command.move_forward = 1.0f;
    return command;
}

} // namespace recoil
