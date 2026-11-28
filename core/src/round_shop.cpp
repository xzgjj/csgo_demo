#include "recoil/sim/round.h"

#include <cstdint>
#include <utility>

#include "recoil/content/weapon_table.h"
#include "recoil/sim/mode.h"
#include "recoil/sim/reject.h"

namespace recoil {

bool CanBuy(const WorldState& state, const Content& content) {
    if (IsCompetitive(state)) {
        return CanBuyInMatch(state);
    }
    const RoundState& round = state.round;
    if (round.phase == RoundPhase::Freeze) {
        return true;
    }
    if (round.phase != RoundPhase::Live) {
        return false;
    }
    const int elapsed =
        static_cast<int>(content.round.live_ticks) - static_cast<int>(round.phase_ticks);
    return elapsed >= 0 && elapsed < static_cast<int>(content.round.buy_grace_ticks);
}

void ApplyBuy(WorldState& state, const Content& content, const Command& command,
              EventList& events) {
    if (command.player >= state.player_count) {
        events.Push({EventKind::BuyFailed, command.player, 0, 0});
        return;
    }
    PlayerState& player = state.players[command.player];
    if (!CanBuy(state, content)) {
        events.Push({EventKind::BuyFailed, command.player, 0, 0});
        return;
    }
    if (command.item == BuyItem::DefuseKit) {
        if (player.has_defuser || player.money < content.round.defuse_kit_price) {
            events.Push({EventKind::BuyFailed, command.player, 0, 3});
            return;
        }
        player.money = static_cast<uint16_t>(player.money - content.round.defuse_kit_price);
        player.has_defuser = true;
        events.Push({EventKind::BuySucceeded, command.player, 0, player.money});
        return;
    }
    const bool helmet = command.item == BuyItem::Helmet;
    if (!helmet && player.armor >= 100) {
        events.Push({EventKind::BuyFailed, command.player, 0, 2});
        return;
    }
    const uint16_t price = helmet ? content.round.helmet_price : content.round.armor_price;
    if (player.money < price) {
        events.Push({EventKind::BuyFailed, command.player, 0, 1});
        return;
    }
    player.money = static_cast<uint16_t>(player.money - price);
    player.armor = 100;
    player.helmet = player.helmet || helmet;
    events.Push({EventKind::BuySucceeded, command.player, 0, player.money});
}

void ApplyBuyWeapon(WorldState& state, const Content& content, const Command& command,
                    EventList& events) {
    (void)content;
    if (command.player >= state.player_count) {
        events.Push({EventKind::BuyFailed, command.player, 0, 0});
        return;
    }
    PlayerState& player = state.players[command.player];
    if (!CanBuy(state, content)) {
        events.Push({EventKind::BuyFailed, command.player, 0, 0});
        return;
    }
    const WeaponRow* row = FindWeaponRow(command.weapon_id);
    if (row == nullptr) {
        events.Push({EventKind::BuyFailed, command.player, 0, 0});
        return;
    }
    if (player.money < row->price) {
        events.Push({EventKind::BuyFailed, command.player, 0, 1});
        return;
    }
    player.money = static_cast<uint16_t>(player.money - row->price);
    const uint8_t slot = row->sidearm ? 1 : 0;
    WeaponSlotId(player, slot) = static_cast<uint8_t>(row->id);
    WeaponSlotState(player, slot) = WeaponState{row->def.magazine, row->def.reserve, 0, 0, false};
    events.Push({EventKind::BuySucceeded, command.player, 0, player.money});
}

void ApplySwitchWeapon(WorldState& state, const Command& command, EventList& events) {
    if (!EnsureAlivePlayer(state, command.player, events)) {
        return;
    }
    PlayerState& player = state.players[command.player];
    if (command.slot >= kWeaponSlotCount || command.slot == player.active_slot) {
        PushReject(events, command.player, RejectReason::InvalidSlot);
        return;
    }
    // 切枪打断换弹，并交换两槽状态（weapon 永远描述当前激活槽）。
    player.weapon.reloading = false;
    player.weapon.reload_ticks_left = 0;
    std::swap(player.weapon, player.sidearm);
    player.active_slot = command.slot;
    events.Push({EventKind::WeaponSwitched, command.player, 0, command.slot});
}

} // namespace recoil
