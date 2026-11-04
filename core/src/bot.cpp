#include "recoil/ai/bot.h"

#include <cmath>

#include "recoil/base/vec3.h"
#include "recoil/sim/grenades.h"

namespace recoil {
namespace {

constexpr float kPi = 3.14159265358979323846f;

} // namespace

Command BotDecide(const WorldState& state, const Content& content, uint8_t self) {
    (void)content;
    Command command;
    command.kind = CommandKind::Stop;
    command.player = self;
    if (self >= state.player_count || !state.players[self].alive) {
        return command;
    }
    int target = -1;
    float best = 1.0e30f;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        const PlayerState& other = state.players[i];
        if (i == self || !other.alive || other.team == state.players[self].team) {
            continue;
        }
        const Vec3 d = other.position - state.players[self].position;
        const float dist = std::sqrt(Dot(d, d));
        if (dist < best) {
            best = dist;
            target = i;
        }
    }
    if (target < 0) {
        return command;
    }
    const Vec3 eye = state.players[self].position + Vec3{0.0f, 0.0f, 64.0f};
    const Vec3 goal = state.players[static_cast<size_t>(target)].position + Vec3{0.0f, 0.0f, 36.0f};
    if (SmokeBlocksSegment(state, eye, goal)) {
        return command;
    }
    const Vec3 d = goal - eye;
    command.kind = CommandKind::Fire;
    command.yaw_degrees = std::atan2(d.y, d.x) * (180.0f / kPi);
    command.pitch_degrees = std::atan2(d.z, std::sqrt(d.x * d.x + d.y * d.y)) * (180.0f / kPi);
    return command;
}

} // namespace recoil
