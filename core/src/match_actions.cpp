#include "recoil/sim/match.h"

#include <algorithm>

#include "recoil/sim/round.h"

namespace recoil {

bool IsActionAllowed(const WorldState& state, Action action) {
    const RoundPhase phase = state.round.phase;
    if (action == Action::SwitchView) {
        return phase == RoundPhase::Warmup || phase == RoundPhase::Live ||
               phase == RoundPhase::RoundEnd || phase == RoundPhase::Halftime ||
               phase == RoundPhase::Timeout || phase == RoundPhase::MatchEnd;
    }
    if (action == Action::Vote) {
        return phase != RoundPhase::Timeout && phase != RoundPhase::MatchEnd;
    }
    switch (phase) {
    case RoundPhase::Warmup:
        return action == Action::Move || action == Action::Fire || action == Action::Buy ||
               action == Action::DropWeapon || action == Action::Pickup ||
               action == Action::Respawn;
    case RoundPhase::Freeze:
        return action == Action::Buy || action == Action::DropWeapon;
    case RoundPhase::Live:
        return action == Action::Move || action == Action::Fire || action == Action::DropWeapon ||
               action == Action::Pickup || action == Action::Plant || action == Action::Defuse;
    case RoundPhase::Halftime:
        return action == Action::Move || action == Action::Buy || action == Action::DropWeapon;
    case RoundPhase::RoundEnd:
        return false;
    case RoundPhase::Timeout:
    case RoundPhase::MatchEnd:
    default:
        return false;
    }
}

bool RequestTimeout(WorldState& state, uint8_t team_id) {
    RoundState& round = state.round;
    if (team_id >= 2 || round.phase != RoundPhase::Freeze) {
        return false;
    }
    if (round.timeout_left[team_id] == 0) {
        return false;
    }
    --round.timeout_left[team_id];
    round.resume_phase = static_cast<uint8_t>(round.phase);
    round.phase = RoundPhase::Timeout;
    round.timeout_ticks_left = state.match.timeout_ticks;
    return true;
}

void EndTimeout(WorldState& state, EventList& events) {
    RoundState& round = state.round;
    if (round.phase != RoundPhase::Timeout) {
        return;
    }
    round.phase = static_cast<RoundPhase>(round.resume_phase);
    round.timeout_ticks_left = 0;
    events.Push({EventKind::TimeoutEnded, 0, 0, round.number});
}

bool VoteSurrender(WorldState& state, const Content& content, uint8_t team_id, EventList& events) {
    RoundState& round = state.round;
    if (team_id >= 2 || round.phase == RoundPhase::MatchEnd) {
        return false;
    }
    // 门槛（D3-6）：5v5 第 10 回合后、3v3 第 6 回合后、1v1 任意回合。
    const uint16_t min_round =
        state.match.mode == ModeId::Comp5v5 ? 10 : (state.match.mode == ModeId::Squad3v3 ? 6 : 1);
    if (round.number < min_round) {
        return false;
    }
    MatchEndRound(state, content, state.round.side[team_id], RoundEndReason::Surrender, events);
    return true;
}

bool CanBuyInMatch(const WorldState& state) {
    const RoundState& round = state.round;
    if (round.phase == RoundPhase::Warmup) {
        return true;
    }
    return (round.phase == RoundPhase::Freeze || round.phase == RoundPhase::Live) &&
           !round.buy_closed;
}

uint16_t ScoreOf(const WorldState& state, uint8_t team_id) {
    return state.round.score_team[team_id < 2 ? team_id : 0];
}

uint8_t RosterSize(const WorldState& state, uint8_t team_id) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        count += static_cast<uint8_t>(state.players[i].team_id == team_id ? 1 : 0);
    }
    return count;
}

} // namespace recoil
