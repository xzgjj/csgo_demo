#include "recoil/ai/reaction.h"

#include "recoil/ai/vision.h"

namespace recoil {

void UpdateReactions(WorldState& state, const MapDef* map) {
    for (uint8_t i = 0; i < state.player_count; ++i) {
        PlayerState& player = state.players[i];
        if (!player.alive) {
            player.reaction_target = kNoReactionTarget;
            player.reaction_ticks = 0;
            continue;
        }
        const int target = NearestVisibleEnemy(state, map, i);
        if (target < 0) {
            player.reaction_target = kNoReactionTarget;
            player.reaction_ticks = 0;
            continue;
        }
        const uint8_t target_index = static_cast<uint8_t>(target);
        if (player.reaction_target == target_index) {
            if (player.reaction_ticks < kMaxReactionTicks) {
                ++player.reaction_ticks;
            }
            continue;
        }
        // 换目标：从 1 重新计时（本 tick 即为第一次看到）。
        player.reaction_target = target_index;
        player.reaction_ticks = 1;
    }
}

} // namespace recoil
