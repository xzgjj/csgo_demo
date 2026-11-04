#include "recoil/sim/round.h"

#include <algorithm>

namespace recoil {
namespace {

// de_ash 布局出生点（content/maps/de_ash.layout.json：T [0,-1700]、CT [0,1700]）。
constexpr Vec3 kDeAshTSpawn{0.0f, -1700.0f, 0.0f};
constexpr Vec3 kDeAshCTSpawn{0.0f, 1700.0f, 0.0f};
// 旧版 2 人出生点，保持既有行为。
constexpr Vec3 kLegacyTSpawn{0.0f, -200.0f, 0.0f};
constexpr Vec3 kLegacyCTSpawn{400.0f, 0.0f, 0.0f};
constexpr int kTeamSize = 5;
constexpr float kSpawnSpacing = 64.0f;
constexpr float kSpawnOffsets[kTeamSize][2] = {
    {0.0f, 0.0f},          {kSpawnSpacing, 0.0f},  {-kSpawnSpacing, 0.0f},
    {0.0f, kSpawnSpacing}, {0.0f, -kSpawnSpacing},
};

// 同一出生点内按队列序号错开，避免同队玩家重叠。
Vec3 TeamSpawn(const Vec3& center, int slot) {
    const int index = slot < kTeamSize ? slot : kTeamSize - 1;
    return {center.x + kSpawnOffsets[index][0], center.y + kSpawnOffsets[index][1], center.z};
}

void ResetRound(WorldState& state, const Content& content, EventList& events) {
    ++state.round.number;
    state.round.phase = RoundPhase::Freeze;
    state.round.phase_ticks = content.round.freeze_ticks;
    state.round.winner = 0;
    state.bomb = BombState{};
    state.smoke = SmokeZone{};
    for (GrenadeState& grenade : state.grenades) {
        grenade = GrenadeState{};
    }
    const bool full_team = state.player_count == static_cast<uint8_t>(kTeamSize * 2);
    int t_slot = 0;
    int ct_slot = 0;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        PlayerState& player = state.players[i];
        player.alive = true;
        player.health = 100;
        player.armor = 0;
        player.helmet = false;
        player.velocity = {};
        player.wish_forward = 0.0f;
        player.wish_side = 0.0f;
        player.jump_queued = false;
        player.jump_held = false;
        player.on_ground = true;
        player.blind_ticks = 0;
        player.weapon.mag = content.rifle.magazine;
        player.weapon.reserve = content.rifle.reserve;
        player.weapon.cooldown_ticks = 0;
        player.weapon.reloading = false;
        player.weapon.reload_ticks_left = 0;
        const bool is_t = player.team == static_cast<uint8_t>(Team::T);
        if (!full_team) {
            player.position = is_t ? kLegacyTSpawn : kLegacyCTSpawn;
            continue;
        }
        player.position =
            is_t ? TeamSpawn(kDeAshTSpawn, t_slot++) : TeamSpawn(kDeAshCTSpawn, ct_slot++);
    }
    // C4 给第一名 T，拆弹器给第一名 CT（不重置经济，money 按玩家各自保留）。
    uint8_t carrier = 0xFF;
    uint8_t defuser = 0xFF;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        PlayerState& player = state.players[i];
        player.has_bomb = false;
        player.has_defuser = false;
        if (player.team == static_cast<uint8_t>(Team::T) && carrier == 0xFF) {
            carrier = i;
        }
        if (player.team == static_cast<uint8_t>(Team::CT) && defuser == 0xFF) {
            defuser = i;
        }
    }
    if (carrier != 0xFF) {
        state.players[carrier].has_bomb = true;
    }
    if (defuser != 0xFF) {
        state.players[defuser].has_defuser = true;
    }
    events.Push({EventKind::RoundReset, 0, 0, state.round.number});
}

void EndRoundInternal(WorldState& state, const Content& content, uint8_t winner,
                      EventList& events) {
    state.round.phase = RoundPhase::RoundEnd;
    state.round.phase_ticks = content.round.end_ticks;
    state.round.winner = winner;
    if (winner == static_cast<uint8_t>(Team::T)) {
        ++state.round.score_t;
    } else if (winner == static_cast<uint8_t>(Team::CT)) {
        ++state.round.score_ct;
    }
    for (uint8_t i = 0; i < state.player_count; ++i) {
        PlayerState& player = state.players[i];
        const bool won = player.team == winner;
        AwardMoney(player, won ? content.round.win_reward : content.round.loss_reward,
                   content.round);
    }
    events.Push({EventKind::RoundEnded, winner, 0, 0});
    if (state.round.score_t >= content.round.max_score ||
        state.round.score_ct >= content.round.max_score) {
        state.round.phase = RoundPhase::MatchEnd;
    }
}

} // namespace

void AssignTeams(WorldState& state) {
    if (state.player_count == 0) {
        return;
    }
    const int half = static_cast<int>(state.player_count) / 2;
    const uint8_t t_count = static_cast<uint8_t>(half > 0 ? half : 1);
    for (uint8_t i = 0; i < state.player_count; ++i) {
        state.players[i].team = static_cast<uint8_t>(i < t_count ? Team::T : Team::CT);
    }
}

void EndRound(WorldState& state, const Content& content, uint8_t winner, EventList& events) {
    EndRoundInternal(state, content, winner, events);
}

void AwardMoney(PlayerState& player, uint16_t amount, const RoundDef& def) {
    const uint32_t total = static_cast<uint32_t>(player.money) + amount;
    player.money = static_cast<uint16_t>(std::min<uint32_t>(total, def.max_money));
}

void AdvanceRound(WorldState& state, const Content& content, EventList& events) {
    RoundState& round = state.round;
    if (round.phase == RoundPhase::Freeze) {
        if (round.phase_ticks > 0) {
            --round.phase_ticks;
        }
        if (round.phase_ticks == 0) {
            round.phase = RoundPhase::Live;
            round.phase_ticks = content.round.live_ticks;
            events.Push({EventKind::RoundLive, 0, 0, round.number});
        }
        return;
    }
    if (round.phase == RoundPhase::Live) {
        int alive_t = 0;
        int alive_ct = 0;
        for (uint8_t i = 0; i < state.player_count; ++i) {
            const PlayerState& player = state.players[i];
            if (!player.alive) {
                continue;
            }
            alive_t += player.team == static_cast<uint8_t>(Team::T) ? 1 : 0;
            alive_ct += player.team == static_cast<uint8_t>(Team::CT) ? 1 : 0;
        }
        if (alive_t == 0 || alive_ct == 0) {
            EndRound(state, content,
                     alive_t == 0 ? static_cast<uint8_t>(Team::CT) : static_cast<uint8_t>(Team::T),
                     events);
            return;
        }
        if (round.phase_ticks > 0) {
            --round.phase_ticks;
        }
        if (round.phase_ticks == 0) {
            EndRound(state, content, static_cast<uint8_t>(Team::CT), events);
        }
        return;
    }
    if (round.phase == RoundPhase::RoundEnd) {
        if (round.phase_ticks > 0) {
            --round.phase_ticks;
        }
        if (round.phase_ticks == 0) {
            ResetRound(state, content, events);
        }
    }
}

void ApplyBuy(WorldState& state, const Content& content, const Command& command,
              EventList& events) {
    if (command.player >= state.player_count) {
        events.Push({EventKind::BuyFailed, command.player, 0, 0});
        return;
    }
    PlayerState& player = state.players[command.player];
    if (state.round.phase != RoundPhase::Freeze) {
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

} // namespace recoil
