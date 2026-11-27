#ifndef RECOIL_SRC_STEP_RELOAD_H
#define RECOIL_SRC_STEP_RELOAD_H

// 换弹阶段（仅在 step.cpp 的匿名命名空间内 include）。
// 拆出目的：step.cpp 稳定压在 CI 单文件 400 行上限内。

#include <algorithm>

#include "recoil/content/weapon_table.h"
#include "recoil/sim/ballistics.h"
#include "recoil/sim/reject.h"

inline void FinishReload(PlayerState& player, const WeaponDef& weapon, uint8_t source,
                         EventList& events) {
    const int capacity =
        std::max(0, static_cast<int>(weapon.magazine) - static_cast<int>(player.weapon.mag));
    const int take = std::min(static_cast<int>(player.weapon.reserve), capacity);
    player.weapon.mag = static_cast<uint16_t>(player.weapon.mag + take);
    player.weapon.reserve = static_cast<uint16_t>(player.weapon.reserve - take);
    player.weapon.reloading = false;
    player.weapon.reload_ticks_left = 0;
    events.Push({EventKind::ReloadFinished, source, 0, player.weapon.mag});
}

inline void ApplyTimers(WorldState& state, const Content& content, EventList& events) {
    for (uint8_t i = 0; i < state.player_count; ++i) {
        PlayerState& player = state.players[i];
        // 弹道恢复与连发复位：与冷却/换弹同处一个 tick 阶段，保证确定性顺序。
        TickBallistics(player, BallisticsFor(ActiveWeaponId(player)), state.tick);
        if (player.weapon.cooldown_ticks > 0) {
            --player.weapon.cooldown_ticks;
        }
        if (!player.weapon.reloading) {
            continue;
        }
        if (player.weapon.reload_ticks_left > 0) {
            --player.weapon.reload_ticks_left;
        }
        if (player.weapon.reload_ticks_left == 0) {
            FinishReload(player, ActiveWeaponDef(player, content), i, events);
        }
    }
}

inline void StartReload(WorldState& state, const Content& content, const Command& command,
                        EventList& events) {
    if (!EnsureAlivePlayer(state, command.player, events)) {
        return;
    }
    PlayerState& player = state.players[command.player];
    const WeaponDef weapon = ActiveWeaponDef(player, content);
    if (player.weapon.reloading) {
        PushReject(events, command.player, RejectReason::Reloading);
        return;
    }
    if (player.weapon.mag >= weapon.magazine) {
        PushReject(events, command.player, RejectReason::MagazineFull);
        return;
    }
    if (player.weapon.reserve == 0) {
        PushReject(events, command.player, RejectReason::NoReserve);
        return;
    }
    player.weapon.reloading = true;
    player.weapon.reload_ticks_left = weapon.reload_ticks;
    events.Push({EventKind::ReloadStarted, command.player, 0, player.weapon.reload_ticks_left});
}

#endif
