#include "recoil/sim/bomb.h"

#include "recoil/sim/round.h"

namespace recoil {

void ApplyBombCommand(WorldState& state, const Content& content, const Command& command,
                      EventList& events) {
    if (command.player >= state.player_count) {
        return;
    }
    PlayerState& player = state.players[command.player];
    if (!player.alive) {
        return;
    }
    if (command.kind == CommandKind::Plant) {
        if (state.bomb.planted || !player.has_bomb || state.round.phase != RoundPhase::Live) {
            return;
        }
        state.bomb.planted = true;
        state.bomb.timer_ticks = content.round.bomb_ticks;
        state.bomb.carrier = command.player;
        state.bomb.position = player.position;
        player.has_bomb = false;
        events.Push({EventKind::BombPlanted, command.player, 0, state.bomb.timer_ticks});
        return;
    }
    if (command.kind == CommandKind::Defuse) {
        if (!state.bomb.planted || state.round.phase != RoundPhase::Live) {
            return;
        }
        if (player.team != static_cast<uint8_t>(Team::CT)) {
            return;
        }
        state.bomb.defusing = true;
    }
}

void StepBomb(WorldState& state, const Content& content, EventList& events) {
    if (!state.bomb.planted) {
        return;
    }
    if (state.bomb.defusing) {
        const bool kit = state.players[0].has_defuser;
        const uint16_t required = kit ? content.round.defuse_kit_ticks : content.round.defuse_ticks;
        ++state.bomb.defuse_progress;
        state.bomb.defusing = false;
        if (state.bomb.defuse_progress >= required) {
            state.bomb.defused = true;
            state.bomb.planted = false;
            EndRound(state, content, static_cast<uint8_t>(Team::CT), events);
            events.Push({EventKind::BombDefused, 0, 0, 0});
            return;
        }
    } else {
        state.bomb.defuse_progress = 0;
    }
    if (state.bomb.timer_ticks > 0) {
        --state.bomb.timer_ticks;
    }
    if (state.bomb.timer_ticks == 0) {
        state.bomb.exploded = true;
        state.bomb.planted = false;
        events.Push({EventKind::BombExploded, 0, 0, 0});
        EndRound(state, content, static_cast<uint8_t>(Team::T), events);
    }
}

} // namespace recoil
