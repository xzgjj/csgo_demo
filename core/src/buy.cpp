#include "recoil/sim/buy.h"

#include <cmath>

#include "recoil/content/weapon_table.h"
#include "recoil/sim/round.h"

namespace recoil {
namespace {

constexpr uint8_t kBuySideNone = 0;

uint8_t SideBit(uint8_t team) {
    if (team == static_cast<uint8_t>(Team::T)) {
        return kBuySideT;
    }
    if (team == static_cast<uint8_t>(Team::CT)) {
        return kBuySideCT;
    }
    return kBuySideNone;
}

bool SidesAllow(uint8_t sides, uint8_t team) {
    return (sides & SideBit(team)) != 0;
}

// 目录窗口：冻结期全程；Live 仅 catalog.window_ticks 内（默认 0 = 开打即关）。
bool BuyWindowOpen(const WorldState& state, const Content& content) {
    if (!CanBuy(state, content)) {
        return false;
    }
    if (state.round.phase == RoundPhase::Live) {
        const int elapsed =
            static_cast<int>(content.round.live_ticks) - static_cast<int>(state.round.phase_ticks);
        return elapsed >= 0 && elapsed < static_cast<int>(content.buy.window_ticks);
    }
    return true;
}

BuyResult PushFail(EventList& events, uint8_t player, uint8_t entry, BuyResult reason) {
    events.Push({EventKind::BuyFailed, player, entry, static_cast<int32_t>(reason)});
    return reason;
}

// 本回合已购数量：bought_counts 只增不减（退款也保留计数，防止“退款→重买”刷限购）。
uint8_t EntryCount(const PlayerState& player, uint8_t entry) {
    return player.bought_counts[entry];
}

void SetSlotWeapon(PlayerState& player, const BuyEntry& entry) {
    const WeaponRow* row = FindWeaponRow(entry.weapon);
    if (row == nullptr) {
        return;
    }
    const uint8_t slot = entry.slot == BuySlot::Secondary ? 1 : 0;
    WeaponSlotId(player, slot) = static_cast<uint8_t>(row->id);
    WeaponState state{row->def.magazine, row->def.reserve, 0, 0, false};
    WeaponSlotState(player, slot) = state;
    if (slot == 0) {
        player.weapon = state;
    } else {
        player.sidearm = state;
    }
    player.active_slot = slot;
}

bool OwnsEntry(const PlayerState& player, const BuyEntry& entry) {
    if (entry.kind == BuyKind::Weapon) {
        const uint8_t slot = entry.slot == BuySlot::Secondary ? 1 : 0;
        return WeaponSlotId(player, slot) == static_cast<uint8_t>(entry.weapon);
    }
    if (entry.kind == BuyKind::Armor) {
        return player.armor >= 100;
    }
    if (entry.kind == BuyKind::Helmet) {
        return player.helmet;
    }
    if (entry.kind == BuyKind::DefuseKit) {
        return player.has_defuser;
    }
    const uint8_t index = static_cast<uint8_t>(entry.grenade);
    return player.grenade_counts[index] > 0;
}

void GrantEntry(PlayerState& player, const BuyEntry& entry) {
    switch (entry.kind) {
    case BuyKind::Weapon:
        SetSlotWeapon(player, entry);
        break;
    case BuyKind::Armor:
        player.armor = 100;
        break;
    case BuyKind::Helmet:
        player.helmet = true;
        player.armor = player.armor < 100 ? 100 : player.armor;
        break;
    case BuyKind::DefuseKit:
        player.has_defuser = true;
        break;
    default:
        player.grenade_counts[static_cast<uint8_t>(entry.grenade)] += 1;
        break;
    }
}

void RevokeEntry(PlayerState& player, const BuyEntry& entry) {
    switch (entry.kind) {
    case BuyKind::Weapon: {
        const uint8_t slot = entry.slot == BuySlot::Secondary ? 1 : 0;
        WeaponSlotId(player, slot) = 0;
        WeaponSlotState(player, slot) = WeaponState{};
        if (player.active_slot == slot && slot == 0) {
            player.weapon = WeaponState{};
        }
        break;
    }
    case BuyKind::Armor:
        player.armor = 0;
        break;
    case BuyKind::Helmet:
        player.helmet = false;
        break;
    case BuyKind::DefuseKit:
        player.has_defuser = false;
        break;
    default: {
        const uint8_t index = static_cast<uint8_t>(entry.grenade);
        if (player.grenade_counts[index] > 0) {
            player.grenade_counts[index] -= 1;
        }
        break;
    }
    }
}

bool UsedSinceBuy(const PlayerState& player, const BuyEntry& entry) {
    if (entry.kind != BuyKind::Weapon) {
        return false;
    }
    const uint8_t slot = entry.slot == BuySlot::Secondary ? 1 : 0;
    return player.fire_count > player.buy_fire_marks[slot];
}

} // namespace

bool IsBuyWindowOpen(const WorldState& state, const Content& content) {
    return BuyWindowOpen(state, content);
}

const BuyEntry* FindBuyEntryByName(const Content& content, const char* name) {
    for (uint8_t i = 0; i < content.buy.count; ++i) {
        const BuyEntry& entry = content.buy.entries[i];
        const char* id = entry.id;
        int j = 0;
        while (id[j] != '\0' && name[j] != '\0' && id[j] == name[j]) {
            ++j;
        }
        if (id[j] == '\0' && name[j] == '\0') {
            return &entry;
        }
    }
    return nullptr;
}

const BuyEntry* FindBuyEntry(const Content& content, WeaponId weapon) {
    for (uint8_t i = 0; i < content.buy.count; ++i) {
        const BuyEntry& entry = content.buy.entries[i];
        if (entry.kind == BuyKind::Weapon && entry.weapon == weapon) {
            return &entry;
        }
    }
    return nullptr;
}

bool InBuyZone(const WorldState& state, const PlayerState& player, const Content& content) {
    if (!content.buy.zone_required || !state.buy_zone_set) {
        return true;
    }
    const Vec3& p = player.position;
    return p.x >= state.buy_zone_min.x && p.x <= state.buy_zone_max.x &&
           p.y >= state.buy_zone_min.y && p.y <= state.buy_zone_max.y &&
           p.z >= state.buy_zone_min.z && p.z <= state.buy_zone_max.z;
}

bool CanBuyEntry(const WorldState& state, const Content& content, uint8_t player, uint8_t entry,
                 BuyResult& reason) {
    if (player >= state.player_count) {
        reason = BuyResult::InvalidPlayer;
        return false;
    }
    if (entry >= content.buy.count) {
        reason = BuyResult::InvalidEntry;
        return false;
    }
    const PlayerState& p = state.players[player];
    if (!p.alive) {
        reason = BuyResult::NotAllowed;
        return false;
    }
    if (!BuyWindowOpen(state, content)) {
        reason = BuyResult::OutOfWindow;
        return false;
    }
    if (!InBuyZone(state, p, content)) {
        reason = BuyResult::OutOfZone;
        return false;
    }
    const BuyEntry& item = content.buy.entries[entry];
    if ((p.refunded_mask & (1u << entry)) != 0) {
        reason = BuyResult::RefundedBlocked;
        return false;
    }
    if (!SidesAllow(item.sides, p.team)) {
        reason = BuyResult::NotAllowed;
        return false;
    }
    // 已持有同件装备/武器：先于限购给出更准确原因（手雷按库存数量+限购判断）。
    if (item.kind != BuyKind::GrenadeHE && item.kind != BuyKind::GrenadeFlash &&
        item.kind != BuyKind::GrenadeSmoke && OwnsEntry(p, item)) {
        reason = BuyResult::AlreadyOwned;
        return false;
    }
    if (EntryCount(p, entry) >= item.limit) {
        reason = BuyResult::LimitReached;
        return false;
    }
    if (p.money < item.price) {
        reason = BuyResult::NoMoney;
        return false;
    }
    reason = BuyResult::Ok;
    return true;
}

BuyResult ApplyBuyOrder(WorldState& state, const Content& content, BuyRequest request,
                        EventList& events) {
    BuyResult reason = BuyResult::Ok;
    if (!CanBuyEntry(state, content, request.player, request.entry, reason)) {
        return PushFail(events, request.player, request.entry, reason);
    }
    PlayerState& player = state.players[request.player];
    const BuyEntry& item = content.buy.entries[request.entry];
    player.money = static_cast<uint16_t>(player.money - item.price);
    GrantEntry(player, item);
    player.bought_counts[request.entry] += 1;
    const uint8_t slot =
        item.slot == BuySlot::Secondary ? 1 : (item.slot == BuySlot::Primary ? 0 : 2);
    if (slot < 2) {
        player.buy_fire_marks[slot] = player.fire_count;
    }
    if (request.entry < 16) {
        player.round_buy_mask |= static_cast<uint16_t>(1u << request.entry);
    }
    events.Push({EventKind::BuySucceeded, request.player, request.entry, player.money});
    return BuyResult::Ok;
}

BuyResult ApplyRefund(WorldState& state, const Content& content, BuyRequest request,
                      EventList& events) {
    if (request.player >= state.player_count) {
        return PushFail(events, request.player, request.entry, BuyResult::InvalidPlayer);
    }
    if (request.entry >= content.buy.count) {
        return PushFail(events, request.player, request.entry, BuyResult::InvalidEntry);
    }
    PlayerState& player = state.players[request.player];
    const BuyEntry& item = content.buy.entries[request.entry];
    if (player.refund_count >= content.buy.max_refunds) {
        return PushFail(events, request.player, request.entry, BuyResult::RefundLimit);
    }
    if (EntryCount(player, request.entry) == 0 ||
        (player.refunded_mask & (1u << request.entry)) != 0) {
        return PushFail(events, request.player, request.entry, BuyResult::NotRefundable);
    }
    if (!BuyWindowOpen(state, content) || !InBuyZone(state, player, content)) {
        return PushFail(events, request.player, request.entry, BuyResult::OutOfWindow);
    }
    if (item.kind == BuyKind::Weapon && !OwnsEntry(player, item)) {
        return PushFail(events, request.player, request.entry, BuyResult::NotRefundable);
    }
    if (UsedSinceBuy(player, item)) {
        return PushFail(events, request.player, request.entry, BuyResult::NotRefundable);
    }
    RevokeEntry(player, item);
    player.money = static_cast<uint16_t>(player.money + item.price);
    player.refund_count += 1;
    player.refunded_mask |= (1u << request.entry);
    events.Push({EventKind::RefundDone, request.player, request.entry, player.money});
    return BuyResult::Ok;
}

void ApplyCatalogCommand(WorldState& state, const Content& content, const Command& command,
                         EventList& events) {
    if (command.kind == CommandKind::Rebuy) {
        if (content.buy.enabled) {
            ApplyRebuy(state, content, command.player, events);
        }
        return;
    }
    if (!content.buy.enabled) {
        events.Push({EventKind::BuyFailed, command.player, command.entry,
                     static_cast<int32_t>(BuyResult::NotAllowed)});
        return;
    }
    const BuyRequest request{command.player, command.entry};
    if (command.kind == CommandKind::Refund) {
        ApplyRefund(state, content, request, events);
    } else {
        ApplyBuyOrder(state, content, request, events);
    }
}

int ApplyRebuy(WorldState& state, const Content& content, uint8_t player, EventList& events) {
    if (player >= state.player_count) {
        return 0;
    }
    const uint16_t mask = state.players[player].last_buy_mask;
    int bought = 0;
    for (uint8_t entry = 0; entry < content.buy.count; ++entry) {
        if ((mask & (1u << (entry % 16))) == 0) {
            continue;
        }
        if (ApplyBuyOrder(state, content, {player, entry}, events) == BuyResult::Ok) {
            ++bought;
        }
    }
    return bought;
}

void ApplyLoadoutInheritance(PlayerState& player, bool survived, uint8_t team) {
    if (survived) {
        return;
    }
    player.armor = 0;
    player.helmet = false;
    player.has_defuser = false;
    player.grenade_counts = {};
    player.primary_id = 0;
    player.weapon = WeaponState{};
    player.active_slot = 1;
    const WeaponRow* sidearm =
        FindWeaponRow(team == static_cast<uint8_t>(Team::T) ? WeaponId::Glock : WeaponId::USP);
    if (sidearm != nullptr) {
        player.secondary_id = static_cast<uint8_t>(sidearm->id);
        player.sidearm = WeaponState{sidearm->def.magazine, sidearm->def.reserve, 0, 0, false};
    }
}

} // namespace recoil
