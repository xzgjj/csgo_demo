#pragma once

#include <cstdint>

#include "recoil/sim/event.h"
#include "recoil/sim/state.h"

namespace recoil {

// 统一拒绝事件（命令被规则拒绝时使用）。
inline void PushReject(EventList& events, uint8_t player, RejectReason reason) {
    events.Push({EventKind::CommandRejected, player, 0, static_cast<int32_t>(reason)});
}

// 命令公共前置校验：玩家索引有效且存活；失败时写入拒绝事件。
inline bool EnsureAlivePlayer(const WorldState& state, uint8_t player, EventList& events) {
    if (player >= state.player_count) {
        PushReject(events, player, RejectReason::InvalidPlayer);
        return false;
    }
    if (!state.players[player].alive) {
        PushReject(events, player, RejectReason::Dead);
        return false;
    }
    return true;
}

} // namespace recoil
