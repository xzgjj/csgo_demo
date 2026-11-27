#include "recoil/sim/step.h"

#include <bit>
#include <cstdint>

namespace recoil {

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
    mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(state.bomb.position.x)));
    mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(state.bomb.position.y)));
    mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(state.bomb.position.z)));
    for (const GroundWeapon& drop : state.ground_weapons) {
        mix(static_cast<uint64_t>(drop.active) | (static_cast<uint64_t>(drop.weapon_id) << 1) |
            (static_cast<uint64_t>(drop.mag) << 8) | (static_cast<uint64_t>(drop.reserve) << 24));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(drop.position.x)));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(drop.position.y)));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(drop.position.z)));
    }
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
        mix(static_cast<uint64_t>(player.primary_id) |
            (static_cast<uint64_t>(player.secondary_id) << 8) |
            (static_cast<uint64_t>(player.active_slot) << 16));
        mix(static_cast<uint64_t>(player.sidearm.mag) |
            (static_cast<uint64_t>(player.sidearm.reserve) << 16) |
            (static_cast<uint64_t>(player.sidearm.cooldown_ticks) << 32) |
            (static_cast<uint64_t>(player.sidearm.reload_ticks_left) << 48));
        mix(static_cast<uint64_t>(player.sidearm.reloading));
        mix(static_cast<uint64_t>(player.reaction_target) |
            (static_cast<uint64_t>(player.reaction_ticks) << 8));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(player.view_yaw)));
        mix(static_cast<uint64_t>(player.fire_count) |
            (static_cast<uint64_t>(player.last_fire_tick) << 16));
        // SPEC-02 弹道状态：连发序号、图案偏移、连发扩散都必须进哈希（BAL-07）。
        mix(static_cast<uint64_t>(player.shot_index) |
            (static_cast<uint64_t>(std::bit_cast<uint32_t>(player.recoil_pitch)) << 8));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(player.recoil_yaw)));
        mix(static_cast<uint64_t>(std::bit_cast<uint32_t>(player.inaccuracy)));
        mix(static_cast<uint64_t>(player.has_bomb) |
            (static_cast<uint64_t>(player.has_defuser) << 1));
        // SPEC-06 购买态：手雷库存/限购计数/退款位/购买位，全部参与确定性与回放。
        mix(static_cast<uint64_t>(player.grenade_counts[0]) |
            (static_cast<uint64_t>(player.grenade_counts[1]) << 8) |
            (static_cast<uint64_t>(player.grenade_counts[2]) << 16) |
            (static_cast<uint64_t>(player.refund_count) << 24));
        mix(static_cast<uint64_t>(player.refunded_mask));
        mix(static_cast<uint64_t>(player.round_buy_mask) |
            (static_cast<uint64_t>(player.last_buy_mask) << 16) |
            (static_cast<uint64_t>(player.buy_fire_marks[0]) << 32));
        for (uint8_t k = 0; k < 16; ++k) {
            mix(static_cast<uint64_t>(player.bought_counts[k]) |
                (static_cast<uint64_t>(player.bought_counts[k + 16]) << 8));
        }
    }
    return hash;
}

} // namespace recoil
