#include "recoil/ai/reaction.h"

#include "recoil/ai/vision.h"

namespace recoil {
namespace {

// 与 NearestVisibleEnemy 同序同义，但把结果写进本 tick 的 VisibilityMemo：
// 后面的 BotDecide / 战术层读同一份结论，同一对玩家每 tick 只 trace 一次。
int NearestVisibleEnemyMemo(WorldState& state, const MapDef* map, uint8_t viewer) {
    const PlayerState& me = state.players[viewer];
    int best = -1;
    float best_dist = 1.0e30f;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        if (i == viewer) {
            continue;
        }
        const PlayerState& other = state.players[i];
        if (!other.alive || other.team == me.team) {
            continue;
        }
        const float dist = Length(other.position - me.position);
        if (dist >= best_dist) {
            continue;
        }
        if (!SeesMemo(state, map, viewer, i)) {
            continue;
        }
        best_dist = dist;
        best = i;
    }
    return best;
}

} // namespace

void UpdateReactions(WorldState& state, const MapDef* map) {
    // 每 tick 一次感知：先复位备忘录，再逐人取最近可见敌人。
    state.visibility.tick = kVisibilityInvalidTick;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        PlayerState& player = state.players[i];
        if (!player.alive) {
            player.reaction_target = kNoReactionTarget;
            player.reaction_ticks = 0;
            continue;
        }
        const int target = NearestVisibleEnemyMemo(state, map, i);
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
