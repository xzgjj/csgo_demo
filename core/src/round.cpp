#include "recoil/sim/round.h"

#include <algorithm>

namespace recoil {
namespace {

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
        player.position = is_t ? Vec3{0.0f, -200.0f, 0.0f} : Vec3{400.0f, 0.0f, 0.0f};
    }
    for (uint8_t i = 0; i < state.player_count; ++i) {
        state.players[i].has_bomb = false;
        state.players[i].has_defuser = false;
    }
    state.players[0].has_bomb = state.players[0].team == static_cast<uint8_t>(Team::T);
    for (uint8_t i = 0; i < state.player_count; ++i) {
        state.players[i].has_defuser = false;
        if (state.players[i].team == static_cast<uint8_t>(Team::CT)) {
            state.players[i].has_defuser = true;
            break;
        }
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
    const bool helmet = command.item == BuyItem::Helmet;
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
