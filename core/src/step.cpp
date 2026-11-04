#include "recoil/sim/step.h"

#include <algorithm>
#include <bit>
#include <cmath>

#include "recoil/base/vec3.h"
#include "recoil/movement/movement.h"
#include "recoil/sim/bomb.h"
#include "recoil/sim/grenades.h"
#include "recoil/sim/round.h"

namespace recoil {
namespace {

constexpr float kHalfWidth = 16.0f;
constexpr float kBodyHeight = 72.0f;

struct RayHit {
    bool hit = false;
    float t = 0.0f;
    float hit_z = 0.0f;
    int player = -1;
};

bool RayVsAabb(const Vec3& origin, const Vec3& dir, const Vec3& lo, const Vec3& hi, float& t_out) {
    float t_min = 0.0f;
    float t_max = 1.0e9f;
    const float o[3] = {origin.x, origin.y, origin.z};
    const float d[3] = {dir.x, dir.y, dir.z};
    const float l[3] = {lo.x, lo.y, lo.z};
    const float h[3] = {hi.x, hi.y, hi.z};
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(d[i]) < 1.0e-6f) {
            if (o[i] < l[i] || o[i] > h[i]) {
                return false;
            }
            continue;
        }
        const float inv = 1.0f / d[i];
        float t0 = (l[i] - o[i]) * inv;
        float t1 = (h[i] - o[i]) * inv;
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        t_min = std::max(t_min, t0);
        t_max = std::min(t_max, t1);
        if (t_min > t_max) {
            return false;
        }
    }
    t_out = t_min;
    return true;
}

void PushReject(EventList& events, uint8_t player, RejectReason reason) {
    events.Push({EventKind::CommandRejected, player, 0, static_cast<int32_t>(reason)});
}

void FinishReload(PlayerState& player, const Content& content, uint8_t source, EventList& events) {
    const int capacity =
        std::max(0, static_cast<int>(content.rifle.magazine) - static_cast<int>(player.weapon.mag));
    const int take = std::min(static_cast<int>(player.weapon.reserve), capacity);
    player.weapon.mag = static_cast<uint16_t>(player.weapon.mag + take);
    player.weapon.reserve = static_cast<uint16_t>(player.weapon.reserve - take);
    player.weapon.reloading = false;
    player.weapon.reload_ticks_left = 0;
    events.Push({EventKind::ReloadFinished, source, 0, player.weapon.mag});
}

void ApplyTimers(WorldState& state, const Content& content, EventList& events) {
    for (uint8_t i = 0; i < state.player_count; ++i) {
        PlayerState& player = state.players[i];
        if (player.weapon.cooldown_ticks > 0) {
            --player.weapon.cooldown_ticks;
        }
        if (!player.weapon.reloading) {
            continue;
        }
        if (player.weapon.reload_ticks_left > 0) {
            --player.weapon.reload_ticks_left;
        }
        if (player.weapon.reload_ticks_left == 0) {
            FinishReload(player, content, i, events);
        }
    }
}

void StartReload(WorldState& state, const Content& content, const Command& command,
                 EventList& events) {
    if (command.player >= state.player_count) {
        PushReject(events, command.player, RejectReason::InvalidPlayer);
        return;
    }
    PlayerState& player = state.players[command.player];
    if (!player.alive) {
        PushReject(events, command.player, RejectReason::Dead);
        return;
    }
    if (player.weapon.reloading) {
        PushReject(events, command.player, RejectReason::Reloading);
        return;
    }
    if (player.weapon.mag >= content.rifle.magazine) {
        PushReject(events, command.player, RejectReason::MagazineFull);
        return;
    }
    if (player.weapon.reserve == 0) {
        PushReject(events, command.player, RejectReason::NoReserve);
        return;
    }
    player.weapon.reloading = true;
    player.weapon.reload_ticks_left = content.rifle.reload_ticks;
    events.Push({EventKind::ReloadStarted, command.player, 0, player.weapon.reload_ticks_left});
}

RayHit TraceAgainstPlayers(const WorldState& state, uint8_t shooter, const Vec3& origin,
                           const Vec3& dir, float max_range) {
    RayHit best;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        if (i == shooter || !state.players[i].alive) {
            continue;
        }
        const Vec3& pos = state.players[i].position;
        const Vec3 lo{pos.x - kHalfWidth, pos.y - kHalfWidth, pos.z};
        const Vec3 hi{pos.x + kHalfWidth, pos.y + kHalfWidth, pos.z + kBodyHeight};
        if (SmokeBlocksSegment(state, origin, {pos.x, pos.y, pos.z + kBodyHeight * 0.5f})) {
            continue;
        }
        float t = 0.0f;
        if (!RayVsAabb(origin, dir, lo, hi, t)) {
            continue;
        }
        if (t > max_range || (best.hit && t >= best.t)) {
            continue;
        }
        best.hit = true;
        best.t = t;
        best.hit_z = origin.z + dir.z * t;
        best.player = i;
    }
    return best;
}

void ResolveDamage(WorldState& state, const Content& content, uint8_t shooter, uint8_t victim,
                   float distance, float hit_z, EventList& events) {
    PlayerState& target = state.players[victim];
    const float falloff =
        std::pow(content.rifle.range_modifier, distance / content.rifle.range_modifier_unit);
    const float height = hit_z - target.position.z;
    const float group =
        height >= 65.0f ? (target.helmet ? 2.0f : 4.0f) : (height < 32.0f ? 0.75f : 1.0f);
    const float raw = static_cast<float>(content.rifle.damage) * falloff * group *
                      (target.armor > 0 ? content.rifle.armor_ratio : 1.0f);
    const uint16_t damage = static_cast<uint16_t>(std::lround(raw));
    const uint16_t applied = std::min(damage, target.health);
    target.health = static_cast<uint16_t>(target.health - applied);
    if (target.armor > 0) {
        const unsigned armor_loss = std::min<unsigned>(target.armor, applied / 2u);
        target.armor = static_cast<uint16_t>(target.armor - armor_loss);
    }
    events.Push({EventKind::DamageApplied, shooter, victim, applied});
    if (target.health == 0) {
        target.alive = false;
        target.has_bomb = false;
        target.weapon.reloading = false;
        target.weapon.reload_ticks_left = 0;
        if (target.team != state.players[shooter].team) {
            ++state.players[shooter].kills;
            AwardMoney(state.players[shooter], content.round.kill_reward, content.round);
        }
        events.Push({EventKind::PlayerDied, shooter, victim, 0});
    }
}

void Fire(WorldState& state, const Content& content, const Command& command, EventList& events) {
    if (command.player >= state.player_count) {
        PushReject(events, command.player, RejectReason::InvalidPlayer);
        return;
    }
    PlayerState& shooter = state.players[command.player];
    if (!shooter.alive) {
        PushReject(events, command.player, RejectReason::Dead);
        return;
    }
    if (shooter.weapon.reloading) {
        PushReject(events, command.player, RejectReason::Reloading);
        return;
    }
    if (shooter.weapon.cooldown_ticks > 0) {
        PushReject(events, command.player, RejectReason::Cooldown);
        return;
    }
    if (shooter.weapon.mag == 0) {
        events.Push({EventKind::WeaponDryFire, command.player, 0, 0});
        return;
    }

    --shooter.weapon.mag;
    shooter.weapon.cooldown_ticks = content.rifle.fire_interval_ticks;
    events.Push({EventKind::WeaponFired, command.player, 0, shooter.weapon.mag});

    const Vec3 origin = shooter.position + Vec3{0.0f, 0.0f, content.movement.eye_height};
    const Vec3 dir = DirectionFromAngles(command.yaw_degrees, command.pitch_degrees);
    const RayHit hit = TraceAgainstPlayers(state, command.player, origin, dir, content.rifle.range);
    if (!hit.hit) {
        events.Push({EventKind::ShotMissed, command.player, 0, 0});
        return;
    }
    ResolveDamage(state, content, command.player, static_cast<uint8_t>(hit.player), hit.t,
                  hit.hit_z, events);
}

void ApplyMove(WorldState& state, const Command& command, EventList& events) {
    if (command.player >= state.player_count) {
        PushReject(events, command.player, RejectReason::InvalidPlayer);
        return;
    }
    PlayerState& player = state.players[command.player];
    if (!player.alive) {
        PushReject(events, command.player, RejectReason::Dead);
        return;
    }
    constexpr float kPi = 3.14159265358979323846f;
    const float yaw = command.yaw_degrees * (kPi / 180.0f);
    const float forward_x = std::cos(yaw);
    const float forward_y = std::sin(yaw);
    const float left_x = -forward_y;
    const float left_y = forward_x;
    player.wish_forward = forward_x * command.move_forward + left_x * command.move_side;
    player.wish_side = forward_y * command.move_forward + left_y * command.move_side;
    player.ducking = command.duck;
    const bool jump_edge = command.jump && !player.jump_held;
    player.jump_held = command.jump;
    if (jump_edge) {
        player.jump_queued = true;
    }
}

} // namespace

StepResult Step(const WorldState& prev, const Content& content, std::span<const Command> commands,
                const MapDef* map) {
    StepResult out;
    out.state = prev;
    ++out.state.tick;
    ApplyTimers(out.state, content, out.events);
    for (const Command& command : commands) {
        switch (command.kind) {
        case CommandKind::Fire:
            Fire(out.state, content, command, out.events);
            break;
        case CommandKind::Reload:
            StartReload(out.state, content, command, out.events);
            break;
        case CommandKind::Move:
            ApplyMove(out.state, command, out.events);
            break;
        case CommandKind::Buy:
            ApplyBuy(out.state, content, command, out.events);
            break;
        case CommandKind::Throw:
            ThrowGrenade(out.state, content, command, out.events);
            break;
        case CommandKind::Plant:
        case CommandKind::Defuse:
            ApplyBombCommand(out.state, content, command, out.events);
            break;
        case CommandKind::Stop:
            break;
        }
    }
    StepBomb(out.state, content, out.events);
    StepGrenades(out.state, content, out.events);
    AdvanceRound(out.state, content, out.events);
    for (uint8_t i = 0; i < out.state.player_count; ++i) {
        StepMovement(out.state.players[i], content.movement, map);
    }
    return out;
}

uint64_t HashState(const WorldState& state) {
    uint64_t hash = 1469598103934665603ull;
    const auto mix = [&hash](uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    mix(state.tick);
    mix(state.rng);
    mix(state.player_count);
    mix(static_cast<uint64_t>(state.round.phase) |
        (static_cast<uint64_t>(state.round.phase_ticks) << 8) |
        (static_cast<uint64_t>(state.round.number) << 32) |
        (static_cast<uint64_t>(state.round.winner) << 48));
    mix(static_cast<uint64_t>(state.round.score_t) |
        (static_cast<uint64_t>(state.round.score_ct) << 16));
    mix(static_cast<uint64_t>(state.bomb.planted) |
        (static_cast<uint64_t>(state.bomb.defused) << 1) |
        (static_cast<uint64_t>(state.bomb.exploded) << 2) |
        (static_cast<uint64_t>(state.bomb.timer_ticks) << 8) |
        (static_cast<uint64_t>(state.bomb.defuse_progress) << 32));
    mix(static_cast<uint64_t>(state.smoke.active) |
        (static_cast<uint64_t>(state.smoke.ticks_left) << 8));
    mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(state.smoke.position.x)));
    mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(state.smoke.position.y)));
    for (const GrenadeState& grenade : state.grenades) {
        mix(static_cast<uint64_t>(grenade.active) | (static_cast<uint64_t>(grenade.kind) << 1) |
            (static_cast<uint64_t>(grenade.fuse_ticks) << 8));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(grenade.position.x)));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(grenade.position.y)));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(grenade.position.z)));
    }
    for (uint8_t i = 0; i < state.player_count; ++i) {
        const PlayerState& player = state.players[i];
        mix(static_cast<uint64_t>(player.alive) | (static_cast<uint64_t>(player.helmet) << 1) |
            (static_cast<uint64_t>(player.health) << 8) |
            (static_cast<uint64_t>(player.armor) << 24) |
            (static_cast<uint64_t>(player.kills) << 40));
        mix(static_cast<uint64_t>(player.team) | (static_cast<uint64_t>(player.money) << 8));
        mix(static_cast<uint64_t>(player.blind_ticks));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(player.position.x)));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(player.position.y)));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(player.position.z)));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(player.velocity.x)));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(player.velocity.y)));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(player.velocity.z)));
        mix(static_cast<uint64_t>(player.on_ground) | (static_cast<uint64_t>(player.ducking) << 1) |
            (static_cast<uint64_t>(player.jump_held) << 2) |
            (static_cast<uint64_t>(player.jump_queued) << 3));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(player.wish_forward)));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(player.wish_side)));
        mix(static_cast<uint64_t>(player.weapon.mag) |
            (static_cast<uint64_t>(player.weapon.reserve) << 16) |
            (static_cast<uint64_t>(player.weapon.cooldown_ticks) << 32) |
            (static_cast<uint64_t>(player.weapon.reload_ticks_left) << 48));
        mix(static_cast<uint64_t>(player.weapon.reloading));
    }
    return hash;
}

} // namespace recoil
