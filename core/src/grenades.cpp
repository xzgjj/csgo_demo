#include "recoil/sim/grenades.h"

#include <algorithm>
#include <cmath>

#include "recoil/base/vec3.h"

namespace recoil {
namespace {

constexpr float kEyeHeight = 64.0f;

float Distance(const Vec3& a, const Vec3& b) {
    const Vec3 d = a - b;
    return std::sqrt(Dot(d, d));
}

void Detonate(WorldState& state, const Content& content, GrenadeState& grenade, EventList& events) {
    const GrenadeDef& def = content.grenades;
    if (grenade.kind == static_cast<uint8_t>(GrenadeKind::HE)) {
        for (uint8_t i = 0; i < state.player_count; ++i) {
            PlayerState& player = state.players[i];
            if (!player.alive) {
                continue;
            }
            const float d = Distance(player.position, grenade.position);
            if (d > def.he_radius) {
                continue;
            }
            const float scale = 1.0f - d / def.he_radius;
            const float armor = player.armor > 0 ? content.rifle.armor_ratio : 1.0f;
            const uint16_t damage = static_cast<uint16_t>(
                std::lround(static_cast<float>(def.he_damage) * scale * armor));
            const uint16_t applied = std::min(damage, player.health);
            player.health = static_cast<uint16_t>(player.health - applied);
            if (player.armor > 0) {
                const uint16_t loss =
                    static_cast<uint16_t>(std::min<unsigned>(player.armor, applied / 2u));
                player.armor = static_cast<uint16_t>(player.armor - loss);
            }
            if (player.health == 0) {
                player.alive = false;
            }
            events.Push({EventKind::DamageApplied, grenade.owner, i, applied});
        }
        events.Push({EventKind::GrenadeExploded, grenade.owner, 0, grenade.kind});
        grenade.active = false;
        return;
    }
    if (grenade.kind == static_cast<uint8_t>(GrenadeKind::Flash)) {
        for (uint8_t i = 0; i < state.player_count; ++i) {
            PlayerState& player = state.players[i];
            const float d = Distance(player.position, grenade.position);
            if (d > def.flash_radius) {
                continue;
            }
            const float scale = 1.0f - d / def.flash_radius;
            const uint16_t ticks =
                static_cast<uint16_t>(std::lround(static_cast<float>(def.flash_max_ticks) * scale));
            player.blind_ticks = std::max(player.blind_ticks, ticks);
            events.Push({EventKind::FlashBlinded, grenade.owner, i, ticks});
        }
        events.Push({EventKind::GrenadeExploded, grenade.owner, 0, grenade.kind});
        grenade.active = false;
        return;
    }
    state.smoke.active = true;
    state.smoke.position = grenade.position;
    state.smoke.radius = def.smoke_radius;
    state.smoke.ticks_left = def.smoke_ticks;
    events.Push({EventKind::SmokeStarted, grenade.owner, 0, def.smoke_ticks});
    grenade.active = false;
    events.Push({EventKind::GrenadeExploded, grenade.owner, 0, grenade.kind});
}

} // namespace

void ThrowGrenade(WorldState& state, const Content& content, const Command& command,
                  EventList& events) {
    if (command.player >= state.player_count) {
        events.Push({EventKind::CommandRejected, command.player, 0,
                     static_cast<int32_t>(RejectReason::InvalidPlayer)});
        return;
    }
    PlayerState& player = state.players[command.player];
    if (!player.alive) {
        events.Push({EventKind::CommandRejected, command.player, 0,
                     static_cast<int32_t>(RejectReason::Dead)});
        return;
    }
    if (state.round.phase == RoundPhase::MatchEnd) {
        events.Push({EventKind::CommandRejected, command.player, 0,
                     static_cast<int32_t>(RejectReason::None)});
        return;
    }
    for (GrenadeState& grenade : state.grenades) {
        if (grenade.active) {
            continue;
        }
        grenade.active = true;
        grenade.kind = static_cast<uint8_t>(command.grenade);
        grenade.owner = command.player;
        grenade.position = player.position + Vec3{0.0f, 0.0f, kEyeHeight};
        grenade.velocity = DirectionFromAngles(command.yaw_degrees, command.pitch_degrees) *
                           content.grenades.throw_speed;
        grenade.fuse_ticks = content.grenades.fuse_ticks;
        events.Push({EventKind::GrenadeThrown, command.player, 0, grenade.kind});
        return;
    }
    events.Push({EventKind::CommandRejected, command.player, 0, 0});
}

void StepGrenades(WorldState& state, const Content& content, EventList& events) {
    const GrenadeDef& def = content.grenades;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        PlayerState& player = state.players[i];
        if (player.blind_ticks > 0) {
            --player.blind_ticks;
        }
    }
    if (state.smoke.active) {
        if (state.smoke.ticks_left > 0) {
            --state.smoke.ticks_left;
        }
        if (state.smoke.ticks_left == 0) {
            state.smoke.active = false;
            events.Push({EventKind::SmokeEnded, 0, 0, 0});
        }
    }
    for (GrenadeState& grenade : state.grenades) {
        if (!grenade.active) {
            continue;
        }
        const float dt = 1.0f / 64.0f;
        grenade.velocity.z -= def.gravity * dt;
        grenade.position = grenade.position + grenade.velocity * dt;
        if (grenade.position.z <= 0.0f) {
            grenade.position.z = 0.0f;
            grenade.velocity = {};
        }
        if (grenade.fuse_ticks > 0) {
            --grenade.fuse_ticks;
        }
        if (grenade.fuse_ticks == 0) {
            Detonate(state, content, grenade, events);
        }
    }
}

bool SmokeBlocksSegment(const WorldState& state, const Vec3& from, const Vec3& to) {
    if (!state.smoke.active) {
        return false;
    }
    const Vec3 seg = to - from;
    const float length_sq = Dot(seg, seg);
    if (length_sq <= 0.0f) {
        return false;
    }
    const Vec3 to_smoke = state.smoke.position - from;
    const float t = std::clamp(Dot(to_smoke, seg) / length_sq, 0.0f, 1.0f);
    const Vec3 closest = from + seg * t;
    return Distance(closest, state.smoke.position) <= state.smoke.radius;
}

} // namespace recoil
