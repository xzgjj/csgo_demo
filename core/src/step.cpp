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
#include "recoil/sim/hitbox.h"
#include "recoil/sim/reject.h"
#include "recoil/sim/round.h"
#include "recoil/world/breakable.h"
#include "recoil/world/trace.h"

namespace recoil {
namespace {

struct RayHit {
    bool hit = false;
    float t = 0.0f;
    uint8_t player = 0xFF;
    HitGroup group = HitGroup::None;
};

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

// 射线离开指定 AABB 的距离（入口之后），用于穿透后从墙背面继续追踪。
float WallExitDistance(const Wall& wall, const Vec3& origin, const Vec3& dir, float entry_t) {
    constexpr float kEps = 1.0e-6f;
    float exit_t = 1.0e9f;
    const float d[3] = {dir.x, dir.y, dir.z};
    const float o[3] = {origin.x, origin.y, origin.z};
    const float lo[3] = {wall.min.x, wall.min.y, wall.min.z};
    const float hi[3] = {wall.max.x, wall.max.y, wall.max.z};
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(d[i]) < kEps) {
            continue;
        }
        const float far_t = ((d[i] > 0.0f ? hi[i] : lo[i]) - o[i]) / d[i];
        if (far_t >= entry_t) {
            exit_t = std::min(exit_t, far_t);
        }
    }
    return exit_t;
}

// 材质穿透：玻璃/木最多两层，每层 0.75 衰减；混凝土/金属等直接挡住。
// 返回伤害倍率；不可穿透返回 -1。
float PenetrationScale(const MapDef& map, const Vec3& origin, const Vec3& dir, float range,
                       float target_t) {
    constexpr int kMaxLayers = 2;
    constexpr float kLayerDamageScale = 0.75f;
    float scale = 1.0f;
    float traveled = 0.0f;
    for (int layer = 0; layer < kMaxLayers; ++layer) {
        const Vec3 from = origin + dir * traveled;
        const float remaining = range - traveled;
        const TraceHit wall = TraceRay(map, from, origin + dir * range);
        if (!wall.hit) {
            return scale;
        }
        const float wall_t = traveled + wall.fraction * remaining;
        if (wall_t > target_t) {
            return scale;
        }
        if (wall.surface != Surface::Glass && wall.surface != Surface::Wood) {
            return -1.0f;
        }
        scale *= kLayerDamageScale;
        const float exit_t = WallExitDistance(map.walls[wall.brush], origin, dir, wall_t);
        traveled = (exit_t < 1.0e8f ? exit_t : wall_t) + 0.1f;
    }
    const float remaining = range - traveled;
    const TraceHit wall = TraceRay(map, origin + dir * traveled, origin + dir * range);
    if (wall.hit && traveled + wall.fraction * remaining <= target_t) {
        return -1.0f;
    }
    return scale;
}

RayHit TraceAgainstPlayers(const WorldState& state, uint8_t shooter, const Vec3& origin,
                           const Vec3& dir, float max_range) {
    RayHit best;
    const uint8_t shooter_team = state.players[shooter].team;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        const PlayerState& target = state.players[i];
        // 竞技默认 friendly fire off：队友不挡弹道、不受伤（也避免 Bot 误伤玩家）。
        if (i == shooter || !target.alive || target.team == shooter_team) {
            continue;
        }
        if (SmokeBlocksSegment(state, origin,
                               {target.position.x, target.position.y,
                                target.position.z + (target.ducking ? 27.0f : 36.0f)})) {
            continue;
        }
        const PlayerHit player_hit = TracePlayerHitboxes(target, origin, dir, max_range);
        if (!player_hit.hit || (best.hit && player_hit.t >= best.t)) {
            continue;
        }
        best.hit = true;
        best.t = player_hit.t;
        best.player = i;
        best.group = player_hit.group;
    }
    return best;
}

void ResolveDamage(WorldState& state, const Content& content, const WeaponDef& weapon,
                   uint8_t shooter, uint8_t victim, float distance, HitGroup group,
                   float penetration_scale, EventList& events) {
    PlayerState& target = state.players[victim];
    const float falloff = std::pow(weapon.range_modifier, distance / weapon.range_modifier_unit);
    float group_multiplier = 1.0f;
    if (group == HitGroup::Head) {
        group_multiplier = target.helmet ? 2.0f : 4.0f;
    } else if (group == HitGroup::Stomach) {
        group_multiplier = 1.25f;
    } else if (group == HitGroup::Legs) {
        group_multiplier = 0.75f;
    }
    const float raw = static_cast<float>(weapon.damage) * falloff * group_multiplier *
                      penetration_scale * (target.armor > 0 ? weapon.armor_ratio : 1.0f);
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

void DamageWallOnShot(MapDef& map, const Vec3& origin, const Vec3& dir, float range,
                      const RayHit& player_hit, uint16_t damage, uint8_t shooter,
                      EventList& events) {
    const TraceHit wall = TraceRay(map, origin, origin + dir * range);
    if (!wall.hit || wall.brush < 0) {
        return;
    }
    if (player_hit.hit && wall.fraction * range > player_hit.t) {
        return;
    }
    const BreakableReport report = DamageWall(map, wall.brush, damage);
    if (report.result == BreakableResult::Damaged || report.result == BreakableResult::Destroyed) {
        events.Push(
            {EventKind::WallDamaged, shooter, static_cast<uint8_t>(report.result), wall.brush});
    }
}

void Fire(WorldState& state, const Content& content, const Command& command, const MapDef* map,
          MapDef* mutable_map, EventList& events) {
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

    const float eye =
        shooter.ducking ? content.movement.duck_eye_height : content.movement.eye_height;
    const Vec3 origin = shooter.position + Vec3{0.0f, 0.0f, eye};
    const Vec3 dir = DirectionFromAngles(command.yaw_degrees, command.pitch_degrees);
    const RayHit hit = TraceAgainstPlayers(state, command.player, origin, dir, weapon.range);
    float penetration_scale = 1.0f;
    if (map != nullptr && hit.hit) {
        penetration_scale = PenetrationScale(*map, origin, dir, weapon.range, hit.t);
    }
    if (!hit.hit || penetration_scale < 0.0f) {
        if (mutable_map != nullptr) {
            DamageWallOnShot(*mutable_map, origin, dir, weapon.range, hit, weapon.damage,
                             command.player, events);
        }
        events.Push({EventKind::ShotMissed, command.player, 0, 0});
        return;
    }
    if (mutable_map != nullptr) {
        DamageWallOnShot(*mutable_map, origin, dir, weapon.range, hit, weapon.damage,
                         command.player, events);
    }
    ResolveDamage(state, content, weapon, command.player, hit.player, hit.t, hit.group,
                  penetration_scale, events);
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
                const MapDef* map, MapDef* mutable_map) {
    StepResult out;
    out.state = prev;
    ++out.state.tick;
    ApplyTimers(out.state, content, out.events);
    for (const Command& command : commands) {
        switch (command.kind) {
        case CommandKind::Fire:
            Fire(out.state, content, command, map, mutable_map, out.events);
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
    StepGrenades(out.state, content, out.events, map);
    AdvanceRound(out.state, content, out.events, map);
    for (uint8_t i = 0; i < out.state.player_count; ++i) {
        Aabb blockers[kMaxPlayers];
        int blocker_count = 0;
        for (uint8_t j = 0; j < out.state.player_count; ++j) {
            if (j == i || !out.state.players[j].alive) {
                continue;
            }
            const PlayerState& other = out.state.players[j];
            const float height =
                other.ducking ? content.movement.duck_height : content.movement.stand_height;
            const float half = content.movement.half_width;
            blockers[blocker_count++] = Aabb{
                {other.position.x - half, other.position.y - half, other.position.z},
                {other.position.x + half, other.position.y + half, other.position.z + height},
            };
        }
        StepMovement(out.state.players[i], content.movement, map,
                     std::span<const Aabb>(blockers, static_cast<size_t>(blocker_count)));
    }
    UpdateReactions(out.state, map);
    return out;
}

} // namespace recoil
