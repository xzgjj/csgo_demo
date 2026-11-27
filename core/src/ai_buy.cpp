#include "recoil/ai/buy_policy.h"

#include "recoil/sim/buy.h"

namespace recoil {
namespace {

constexpr int kPlanMax = 8;

bool TryPush(const WorldState& state, const Content& content, uint8_t player, const char* id,
             uint16_t budget, uint16_t& spent, uint8_t* entries, int& count) {
    const BuyEntry* entry = FindBuyEntryByName(content, id);
    if (entry == nullptr || count >= kPlanMax) {
        return false;
    }
    const uint8_t index = static_cast<uint8_t>(entry - content.buy.entries);
    BuyResult reason = BuyResult::Ok;
    if (!CanBuyEntry(state, content, player, index, reason)) {
        return false;
    }
    if (static_cast<uint32_t>(entry->price) + spent > budget) {
        return false;
    }
    entries[count++] = index;
    spent = static_cast<uint16_t>(spent + entry->price);
    return true;
}

} // namespace

AiBuyPolicy ChooseBuyPolicy(uint16_t money, uint8_t loss_streak) {
    if (money <= 350 || loss_streak >= 5) {
        return AiBuyPolicy::Save;
    }
    if (money <= 1200) {
        return AiBuyPolicy::Eco;
    }
    if (loss_streak >= 1) {
        return money < 5500 ? AiBuyPolicy::Force : AiBuyPolicy::Full;
    }
    if (money < 5000) {
        return AiBuyPolicy::Force;
    }
    return AiBuyPolicy::Full;
}

int PlanBotBuys(const WorldState& state, const Content& content, uint8_t player, uint8_t* entries,
                int max_entries) {
    if (entries == nullptr || max_entries <= 0 || player >= state.player_count) {
        return 0;
    }
    const PlayerState& p = state.players[player];
    const uint8_t loss = state.round.loss_streak[p.team_id < 2 ? p.team_id : 0];
    const AiBuyPolicy policy = ChooseBuyPolicy(p.money, loss);
    uint16_t budget = p.money;
    if (policy == AiBuyPolicy::Save) {
        budget = 0;
    }
    const bool t_side = p.team == static_cast<uint8_t>(Team::T);
    const char* rifle = t_side ? "ak47" : "m4a4";
    uint16_t spent = 0;
    int count = 0;
    const int cap = max_entries < kPlanMax ? max_entries : kPlanMax;

    if (policy == AiBuyPolicy::Full || policy == AiBuyPolicy::Force) {
        if (policy == AiBuyPolicy::Full && budget >= 4750) {
            TryPush(state, content, player, "awp", budget, spent, entries, count);
        }
        if (count == 0) {
            TryPush(state, content, player, rifle, budget, spent, entries, count);
        }
        TryPush(state, content, player, "kevlar", budget, spent, entries, count);
        if (policy == AiBuyPolicy::Full) {
            TryPush(state, content, player, "helmet", budget, spent, entries, count);
            TryPush(state, content, player, "defuse_kit", budget, spent, entries, count);
            TryPush(state, content, player, "he", budget, spent, entries, count);
            TryPush(state, content, player, "smoke", budget, spent, entries, count);
            TryPush(state, content, player, "flash", budget, spent, entries, count);
        }
    } else if (policy == AiBuyPolicy::Eco) {
        TryPush(state, content, player, "kevlar", budget, spent, entries, count);
        TryPush(state, content, player, "flash", budget, spent, entries, count);
    }
    if (count > cap) {
        count = cap;
    }
    return count;
}

} // namespace recoil
