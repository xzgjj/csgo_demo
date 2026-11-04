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
        AwardMoney(player, content.round.plant_reward, content.round);
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
        state.bomb.defuser = command.player;
    }
}

void StepBomb(WorldState& state, const Content& content, EventList& events) {
    if (!state.bomb.planted) {
        return;
    }
    if (state.bomb.defusing) {
        const uint8_t defuser = state.bomb.defuser;
        const bool kit = defuser < state.player_count && state.players[defuser].has_defuser;
        const uint16_t required = kit ? content.round.defuse_kit_ticks : content.round.defuse_ticks;
        ++state.bomb.defuse_progress;
        state.bomb.defusing = false;
        if (state.bomb.defuse_progress >= required) {
            state.bomb.defused = true;
            state.bomb.planted = false;
            const uint8_t defuser = state.bomb.defuser;
            if (defuser < state.player_count) {
                AwardMoney(state.players[defuser], content.round.defuse_reward, content.round);
            }
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
