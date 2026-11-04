#include "recoil/sim/step.h"

#include <algorithm>
#include <cmath>

#include "recoil/ai/reaction.h"
#include "recoil/base/vec3.h"
#include "recoil/content/weapon_table.h"
#include "recoil/movement/movement.h"
#include "recoil/sim/bomb.h"
#include "recoil/sim/grenades.h"
#include "recoil/sim/ground.h"
#include "recoil/sim/reject.h"
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

void FinishReload(PlayerState& player, const WeaponDef& weapon, uint8_t source, EventList& events) {
    const int capacity =
        std::max(0, static_cast<int>(weapon.magazine) - static_cast<int>(player.weapon.mag));
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
            FinishReload(player, ActiveWeaponDef(player, content), i, events);
        }
    }
}

void StartReload(WorldState& state, const Content& content, const Command& command,
                 EventList& events) {
    if (!EnsureAlivePlayer(state, command.player, events)) {
        return;
    }
    PlayerState& player = state.players[command.player];
    const WeaponDef weapon = ActiveWeaponDef(player, content);
    if (player.weapon.reloading) {
        PushReject(events, command.player, RejectReason::Reloading);
        return;
    }
    if (player.weapon.mag >= weapon.magazine) {
        PushReject(events, command.player, RejectReason::MagazineFull);
        return;
    }
    if (player.weapon.reserve == 0) {
        PushReject(events, command.player, RejectReason::NoReserve);
        return;
    }
    player.weapon.reloading = true;
    player.weapon.reload_ticks_left = weapon.reload_ticks;
    events.Push({EventKind::ReloadStarted, command.player, 0, player.weapon.reload_ticks_left});
}

RayHit TraceAgainstPlayers(const WorldState& state, uint8_t shooter, const Vec3& origin,
                           const Vec3& dir, float max_range) {
    RayHit best;
    const uint8_t shooter_team = state.players[shooter].team;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        // 竞技默认 friendly fire off：队友不挡弹道、不受伤（也避免 Bot 误伤玩家）。
        if (i == shooter || !state.players[i].alive || state.players[i].team == shooter_team) {
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

void ResolveDamage(WorldState& state, const Content& content, const WeaponDef& weapon,
                   uint8_t shooter, uint8_t victim, float distance, float hit_z,
                   EventList& events) {
    PlayerState& target = state.players[victim];
    const float falloff = std::pow(weapon.range_modifier, distance / weapon.range_modifier_unit);
    const float height = hit_z - target.position.z;
    const float group =
        height >= 65.0f ? (target.helmet ? 2.0f : 4.0f) : (height < 32.0f ? 0.75f : 1.0f);
    const float raw = static_cast<float>(weapon.damage) * falloff * group *
                      (target.armor > 0 ? weapon.armor_ratio : 1.0f);
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
        DropPrimaryOnDeath(state, victim, events);
    }
}

// 沿射线最近实体面的 t（无命中返回极大值）。
float NearestWallT(const MapDef& map, const Vec3& origin, const Vec3& dir, float max_range) {
    float nearest = 1.0e9f;
    for (int i = 0; i < map.wall_count; ++i) {
        float t = 0.0f;
        if (!RayVsAabb(origin, dir, map.walls[i].min, map.walls[i].max, t)) {
            continue;
        }
        if (t >= 0.0f && t <= max_range && t < nearest) {
            nearest = t;
        }
    }
    return nearest;
}

void Fire(WorldState& state, const Content& content, const Command& command, const MapDef* map,
          EventList& events) {
    if (command.player >= state.player_count) {
        PushReject(events, command.player, RejectReason::InvalidPlayer);
        return;
    }
    PlayerState& shooter = state.players[command.player];
    if (!shooter.alive) {
        PushReject(events, command.player, RejectReason::Dead);
        return;
    }
    if (state.round.phase == RoundPhase::MatchEnd) {
        PushReject(events, command.player, RejectReason::None);
        return;
    }
    const WeaponDef weapon = ActiveWeaponDef(shooter, content);
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
    shooter.weapon.cooldown_ticks = weapon.fire_interval_ticks;
    shooter.view_yaw = command.yaw_degrees;
    ++shooter.fire_count;
    shooter.last_fire_tick = static_cast<uint32_t>(state.tick);
    events.Push({EventKind::WeaponFired, command.player, 0, shooter.weapon.mag});

    const Vec3 origin = shooter.position + Vec3{0.0f, 0.0f, content.movement.eye_height};
    const Vec3 dir = DirectionFromAngles(command.yaw_degrees, command.pitch_degrees);
    const RayHit hit = TraceAgainstPlayers(state, command.player, origin, dir, weapon.range);
    const float wall_t = (map != nullptr) ? NearestWallT(*map, origin, dir, weapon.range) : 1.0e9f;
    if (!hit.hit || wall_t < hit.t) {
        events.Push({EventKind::ShotMissed, command.player, 0, 0});
        return;
    }
    ResolveDamage(state, content, weapon, command.player, static_cast<uint8_t>(hit.player), hit.t,
                  hit.hit_z, events);
}

void ApplyMove(WorldState& state, const Command& command, EventList& events) {
    if (!EnsureAlivePlayer(state, command.player, events)) {
        return;
    }
    PlayerState& player = state.players[command.player];
    player.view_yaw = command.yaw_degrees;
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
            Fire(out.state, content, command, map, out.events);
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
        case CommandKind::BuyWeapon:
            ApplyBuyWeapon(out.state, content, command, out.events);
            break;
        case CommandKind::SwitchWeapon:
            ApplySwitchWeapon(out.state, command, out.events);
            break;
        case CommandKind::Pickup:
            ApplyPickup(out.state, command, out.events);
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
    UpdateReactions(out.state, map);
    return out;
}

} // namespace recoil
