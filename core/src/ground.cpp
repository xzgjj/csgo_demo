#include "recoil/sim/ground.h"

#include <cmath>
#include <cstddef>

#include "recoil/content/weapon_table.h"
#include "recoil/sim/reject.h"

namespace recoil {

int GroundWeaponCount(const WorldState& state) {
    int count = 0;
    for (const GroundWeapon& drop : state.ground_weapons) {
        count += drop.active ? 1 : 0;
    }
    return count;
}

int NearestGroundWeapon(const WorldState& state, const Vec3& position, float radius) {
    int best = -1;
    float best_distance = radius;
    for (int i = 0; i < kMaxDrops; ++i) {
        const GroundWeapon& drop = state.ground_weapons[static_cast<size_t>(i)];
        if (!drop.active) {
            continue;
        }
        const float distance = Length(drop.position - position);
        if (distance <= best_distance) {
            best_distance = distance;
            best = i;
        }
    }
    return best;
}

bool DropPrimaryOnDeath(WorldState& state, uint8_t player, EventList& events) {
    if (player >= state.player_count) {
        return false;
    }
    PlayerState& victim = state.players[player];
    const WeaponId primary = static_cast<WeaponId>(victim.primary_id);
    if (primary == WeaponId::None) {
        return false;
    }
    int slot = -1;
    for (int i = 0; i < kMaxDrops; ++i) {
        if (!state.ground_weapons[static_cast<size_t>(i)].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        return false;
    }
    WeaponState& primary_state = WeaponSlotState(victim, 0);
    GroundWeapon& drop = state.ground_weapons[static_cast<size_t>(slot)];
    drop.active = true;
    drop.weapon_id = victim.primary_id;
    drop.position = victim.position;
    drop.mag = primary_state.mag;
    drop.reserve = primary_state.reserve;
    events.Push({EventKind::WeaponDropped, player, static_cast<uint8_t>(slot),
                 static_cast<int32_t>(primary)});
    // 主武器离手：清空主槽武器与其弹药。
    victim.primary_id = 0;
    primary_state = WeaponState{};
    if (victim.active_slot == 0) {
        // 手上武器已掉落：切到副武器槽（若有），否则空手。
        victim.active_slot = 1;
    }
    return true;
}

void ApplyPickup(WorldState& state, const Command& command, EventList& events) {
    if (!EnsureAlivePlayer(state, command.player, events)) {
        return;
    }
    PlayerState& player = state.players[command.player];
    const int slot = NearestGroundWeapon(state, player.position, kPickupRadius);
    if (slot < 0) {
        PushReject(events, command.player, RejectReason::NoGroundWeapon);
        return;
    }
    GroundWeapon& drop = state.ground_weapons[static_cast<size_t>(slot)];
    const WeaponId weapon = static_cast<WeaponId>(drop.weapon_id);
    WeaponSlotId(player, 0) = drop.weapon_id;
    WeaponSlotState(player, 0) = WeaponState{drop.mag, drop.reserve, 0, 0, false};
    drop = GroundWeapon{};
    events.Push({EventKind::WeaponPickedUp, command.player, static_cast<uint8_t>(slot),
                 static_cast<int32_t>(weapon)});
}

} // namespace recoil
