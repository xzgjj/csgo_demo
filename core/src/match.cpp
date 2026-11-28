#include "recoil/sim/match.h"

#include <algorithm>
#include <utility>

#include "recoil/sim/round.h"

namespace recoil {
namespace {

constexpr uint8_t kNoTeam = 0xFF;

// 队伍身份 → 当前阵营；越界按 0 处理，避免脏索引。
uint8_t SideOfTeam(const WorldState& state, uint8_t team_id) {
    return state.round.side[team_id < 2 ? team_id : 0];
}

// 当前阵营 → 队伍身份（side 唯一，必有一解）。
uint8_t TeamOfSide(const WorldState& state, uint8_t side) {
    return state.round.side[1] == side ? 1 : 0;
}

// 兼容字段同步：score_t/score_ct 始终是“当前侧”的镜像，真源是 score_team。
void SyncScores(WorldState& state) {
    RoundState& round = state.round;
    round.score_t = round.score_team[TeamOfSide(state, static_cast<uint8_t>(Team::T))];
    round.score_ct = round.score_team[TeamOfSide(state, static_cast<uint8_t>(Team::CT))];
}

// 发钱并截断到赛制上限（超出部分丢弃，§3.6.4）。
void Pay(PlayerState& player, uint32_t amount, const MatchRules& rules) {
    const uint32_t total = static_cast<uint32_t>(player.money) + amount;
    player.money = static_cast<uint16_t>(std::min<uint32_t>(total, rules.money_cap));
}

uint8_t AliveOnSide(const WorldState& state, uint8_t side) {
    uint8_t alive = 0;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        if (state.players[i].alive && state.players[i].team == side) {
            ++alive;
        }
    }
    return alive;
}

// 进入半场：换边 + 180 s 间歇（§3.6.3）。
void EnterHalftime(WorldState& state, const Content& content, EventList& events) {
    SwapSides(state, content, events);
    state.round.half = 2;
    state.round.phase = RoundPhase::Halftime;
    state.round.phase_ticks = state.match.halftime_ticks;
    events.Push({EventKind::RoundHalfStarted, 0, 0, state.round.half});
}

// RoundEnd 计时结束后的阶段选择：终局 > 半场 > 加时 > 下一回合。
void AdvanceAfterRoundEnd(WorldState& state, const Content& content, EventList& events) {
    RoundState& round = state.round;
    const MatchRules& rules = state.match;
    const auto& score = round.score_team;
    if (score[0] >= rules.win_score || score[1] >= rules.win_score) {
        round.phase = RoundPhase::MatchEnd;
        events.Push({EventKind::MatchEnded, round.winner, 0, round.number});
        return;
    }
    // 常规时间：第 swap_round() 回合结束后进半场，比分不清零。
    if (round.overtime == 0 && round.half == 1 && round.number >= rules.swap_round()) {
        EnterHalftime(state, content, events);
        return;
    }
    // 常规时间打平到“差一分” → 进加时；无加时的模式直接判平。
    if (round.overtime == 0 && score[0] == score[1] &&
        score[0] == static_cast<uint16_t>(rules.win_score - 1)) {
        if (!StartOvertime(state, content, events)) {
            round.phase = RoundPhase::MatchEnd;
            events.Push({EventKind::MatchDrawn, 0, 0, round.number});
        }
        return;
    }
    // 加时：先到 ot_win_score 结束；每半 ot_rounds_per_half 回合换边；再 3–3 进下一个 OT。
    if (round.overtime > 0) {
        if (score[0] >= rules.ot_win_score || score[1] >= rules.ot_win_score) {
            round.phase = RoundPhase::MatchEnd;
            events.Push({EventKind::MatchEnded, round.winner, 0, round.number});
            return;
        }
        if (round.ot_half == 1 && round.ot_rounds_in_half >= rules.ot_rounds_per_half) {
            SwapSides(state, content, events);
            round.ot_half = 2;
            round.ot_rounds_in_half = 0;
            round.phase = RoundPhase::Halftime;
            round.phase_ticks = rules.halftime_ticks;
            events.Push({EventKind::RoundHalfStarted, 0, 0, round.half});
            return;
        }
        if (round.ot_half == 2 && score[0] == score[1] &&
            score[0] == static_cast<uint16_t>(rules.ot_rounds_per_half)) {
            if (!StartOvertime(state, content, events)) {
                round.phase = RoundPhase::MatchEnd;
                events.Push({EventKind::MatchDrawn, 0, 0, round.number});
            }
            return;
        }
        ++round.ot_rounds_in_half;
    }
    StartRound(state, content, events);
}

} // namespace

MatchError BeginMatchWithRules(WorldState& state, const MatchRules& rules, EventList& events) {
    // 入参快照：调用方可能直接传 state.match 自身，而下方要 state = WorldState{}；
    // 沿用引用会读到重置后的默认规则（enabled=false），静默丢掉赛制配置。
    const MatchRules rules_snapshot = rules;
    const int mode = static_cast<int>(rules_snapshot.mode);
    if (!rules_snapshot.enabled || mode < 0 || mode > 2 || rules_snapshot.team_size == 0 ||
        rules_snapshot.team_size > 5) {
        return MatchError::ModeInvalid;
    }
    const bool comp = rules_snapshot.mode == ModeId::Comp5v5;
    if ((comp && rules_snapshot.site_count != 2) || (!comp && rules_snapshot.site_count != 1)) {
        return MatchError::SiteConflict;
    }
    const uint8_t roster = static_cast<uint8_t>(rules_snapshot.team_size * 2);
    if (state.player_count == 0) {
        state.player_count = roster;
    } else if (state.player_count != roster) {
        return MatchError::RosterMismatch;
    }
    const uint8_t count = state.player_count;
    const Vec3 spawn_t = state.spawn_t;
    const Vec3 spawn_ct = state.spawn_ct;
    const bool spawn_from_map = state.spawn_from_map;
    state = WorldState{};
    state.player_count = count;
    state.spawn_t = spawn_t;
    state.spawn_ct = spawn_ct;
    state.spawn_from_map = spawn_from_map;
    state.match = rules_snapshot;
    state.round.side = {{static_cast<uint8_t>(Team::T), static_cast<uint8_t>(Team::CT)}};
    state.round.score_team = {{0, 0}};
    state.round.loss_streak = {{1, 1}};
    state.round.timeout_left = {{rules_snapshot.timeout_count, rules_snapshot.timeout_count}};
    state.round.half = 1;
    state.round.number = 0;
    state.round.halves_done = false;
    for (uint8_t i = 0; i < count; ++i) {
        PlayerState& player = state.players[i];
        player.team_id = static_cast<uint8_t>(i < rules_snapshot.team_size ? 0 : 1);
        player.team = SideOfTeam(state, player.team_id);
        player.alive = true;
        player.health = 100;
        player.money = static_cast<uint16_t>(rules_snapshot.money_start);
        player.kills = 0;
    }
    StartRound(state, Content{}, events, false);
    state.round.number = 1;
    state.round.phase = RoundPhase::Warmup;
    state.round.phase_ticks = rules_snapshot.warmup_ticks;
    events.Push({EventKind::MatchStarted, 0, 0, 0});
    SyncScores(state);
    return MatchError::Ok;
}

void TickMatch(WorldState& state, const Content& content, EventList& events) {
    RoundState& round = state.round;
    const MatchRules& rules = state.match;
    if (!rules.enabled) {
        return;
    }
    // 买窗在 Freeze 与 Live 初期同时走表（与 §3.6.1 时间轴一致）。
    if ((round.phase == RoundPhase::Freeze || round.phase == RoundPhase::Live) &&
        !round.buy_closed) {
        if (round.buy_ticks_left > 0) {
            --round.buy_ticks_left;
        }
        if (round.buy_ticks_left == 0) {
            round.buy_closed = true;
            events.Push({EventKind::BuyWindowClosed, 0, 0, round.number});
        }
    }
    switch (round.phase) {
    case RoundPhase::Warmup:
        if (round.phase_ticks > 0) {
            --round.phase_ticks;
        }
        if (round.phase_ticks == 0) {
            StartRound(state, content, events, false);
            events.Push({EventKind::RoundLive, 0, 0, round.number});
        }
        return;
    case RoundPhase::Freeze:
        if (round.phase_ticks > 0) {
            --round.phase_ticks;
        }
        if (round.phase_ticks == 0) {
            round.phase = RoundPhase::Live;
            round.phase_ticks = round.round_ticks_left;
            events.Push({EventKind::RoundLive, 0, 0, round.number});
        }
        return;
    case RoundPhase::Live: {
        const uint8_t side_t = static_cast<uint8_t>(Team::T);
        const uint8_t side_ct = static_cast<uint8_t>(Team::CT);
        // 优先级 1/2：拆包与引爆（同 tick 拆包优先）。
        if (state.bomb.defused) {
            MatchEndRound(state, content, side_ct, RoundEndReason::BombDefused, events);
            return;
        }
        if (state.bomb.exploded) {
            MatchEndRound(state, content, side_t, RoundEndReason::BombExploded, events);
            return;
        }
        const uint8_t alive_t = AliveOnSide(state, side_t);
        const uint8_t alive_ct = AliveOnSide(state, side_ct);
        // 优先级 3/5：CT 全灭 → T 胜；T 全灭且包未下 → CT 胜；包已下则不判（异常 8）。
        if (alive_ct == 0) {
            MatchEndRound(state, content, side_t, RoundEndReason::Elimination, events);
            return;
        }
        if (alive_t == 0 && !state.bomb.planted) {
            MatchEndRound(state, content, side_ct, RoundEndReason::Elimination, events);
            return;
        }
        // 优先级 4：回合计时到且包未下 → CT 胜；已下包交给 C4。
        if (!state.bomb.planted && round.round_ticks_left > 0) {
            --round.round_ticks_left;
        }
        round.phase_ticks = round.round_ticks_left;
        if (!state.bomb.planted && round.round_ticks_left == 0) {
            MatchEndRound(state, content, side_ct, RoundEndReason::TimeExpired, events);
        }
        return;
    }
    case RoundPhase::RoundEnd:
        if (round.phase_ticks > 0) {
            --round.phase_ticks;
        }
        if (round.phase_ticks == 0) {
            AdvanceAfterRoundEnd(state, content, events);
        }
        return;
    case RoundPhase::Halftime:
        if (round.phase_ticks > 0) {
            --round.phase_ticks;
        }
        if (round.phase_ticks == 0) {
            StartRound(state, content, events);
        }
        return;
    case RoundPhase::Timeout:
        if (round.timeout_ticks_left > 0) {
            --round.timeout_ticks_left;
        }
        if (round.timeout_ticks_left == 0) {
            EndTimeout(state, events);
        }
        return;
    case RoundPhase::MatchEnd:
    default:
        return;
    }
}

void MatchEndRound(WorldState& state, const Content& content, uint8_t winner_side,
                   RoundEndReason reason, EventList& events) {
    RoundState& round = state.round;
    const MatchRules& rules = state.match;
    if (reason == RoundEndReason::DeadRound) {
        if (round.phase == RoundPhase::Freeze || round.phase == RoundPhase::Warmup) {
            StartRound(state, content, events, false);
        }
        return;
    }
    // 幂等（红线 4）：只在 Live 结算一次；MatchEnd 后忽略任何重复结算。
    if (round.phase == RoundPhase::MatchEnd) {
        return;
    }
    // 投降可在任意阶段立即结算（异常 15），且不加分、保留当前比分（§3.6.2 优先级 6）。
    if (reason == RoundEndReason::Surrender) {
        round.end_reason = static_cast<uint8_t>(reason);
        round.winner = winner_side;
        round.phase = RoundPhase::MatchEnd;
        events.Push({EventKind::SurrenderPassed, winner_side, 0, round.number});
        events.Push({EventKind::MatchEnded, winner_side, 0, round.number});
        return;
    }
    if (round.phase != RoundPhase::Live) {
        return;
    }
    if (reason == RoundEndReason::Draw) {
        round.phase = RoundPhase::MatchEnd;
        round.end_reason = static_cast<uint8_t>(reason);
        events.Push({EventKind::MatchDrawn, 0, 0, round.number});
        return;
    }
    round.end_reason = static_cast<uint8_t>(reason);
    round.winner = winner_side;
    const uint8_t wteam = TeamOfSide(state, winner_side);
    const uint8_t lteam = static_cast<uint8_t>(1 - wteam);
    ++round.score_team[wteam];
    SyncScores(state);
    const uint16_t win_money =
        (reason == RoundEndReason::BombExploded || reason == RoundEndReason::BombDefused)
            ? rules.win_objective
            : rules.win_elim;
    const uint8_t loss_index = static_cast<uint8_t>(
        std::min<uint8_t>(round.loss_streak[lteam], static_cast<uint8_t>(5)) - 1);
    const bool plant_loss =
        state.bomb.planted && SideOfTeam(state, lteam) == static_cast<uint8_t>(Team::T);
    for (uint8_t i = 0; i < state.player_count; ++i) {
        PlayerState& player = state.players[i];
        if (player.team_id == wteam) {
            Pay(player, win_money, rules);
        } else {
            // 失败补偿阶梯 + 下包后仍输的额外补偿（§3.6.4）。
            Pay(player, rules.loss_ladder[loss_index], rules);
            if (plant_loss) {
                Pay(player, rules.plant_loss_bonus, rules);
            }
        }
    }
    // 连败：输 +1（上限 5），赢 -1（下限 1，禁止清零）。
    round.loss_streak[lteam] = static_cast<uint8_t>(std::min<uint8_t>(
        static_cast<uint8_t>(5), static_cast<uint8_t>(round.loss_streak[lteam] + 1)));
    round.loss_streak[wteam] = static_cast<uint8_t>(std::max<uint8_t>(
        static_cast<uint8_t>(1), static_cast<uint8_t>(round.loss_streak[wteam] - 1)));
    round.phase = RoundPhase::RoundEnd;
    round.phase_ticks = rules.round_end_ticks;
    events.Push({EventKind::RoundEnded, winner_side, 0, round.number});
}

void SwapSides(WorldState& state, const Content& content, EventList& events) {
    (void)content; // 出生点在下一次 StartRound 时按新 side 重新计算，这里只做状态翻转。
    RoundState& round = state.round;
    const MatchRules& rules = state.match;
    std::swap(round.side[0], round.side[1]);
    for (uint8_t i = 0; i < state.player_count; ++i) {
        PlayerState& player = state.players[i];
        player.team = SideOfTeam(state, player.team_id);
        player.money =
            static_cast<uint16_t>(round.overtime > 0 ? rules.ot_money : rules.money_start);
        player.has_bomb = false;
        player.has_defuser = false;
    }
    round.loss_streak = {{1, 1}};
    for (GroundWeapon& drop : state.ground_weapons) {
        drop = GroundWeapon{};
    }
    SyncScores(state);
    events.Push({EventKind::SideSwapped, round.side[0], round.side[1], round.half});
}

bool StartOvertime(WorldState& state, const Content& content, EventList& events) {
    RoundState& round = state.round;
    const MatchRules& rules = state.match;
    if (rules.overtime_cap == 0) {
        return false; // 1v1 无加时 → 判平
    }
    if (round.overtime >= rules.overtime_cap) {
        return false; // E04：上限后强制平局
    }
    ++round.overtime;
    round.ot_half = 1;
    round.ot_rounds_in_half = 0;
    round.half = 1;
    round.score_team = {{0, 0}};
    round.loss_streak = {{1, 1}};
    SyncScores(state);
    for (uint8_t i = 0; i < state.player_count; ++i) {
        state.players[i].money = static_cast<uint16_t>(rules.ot_money);
    }
    events.Push({EventKind::OvertimeStarted, round.overtime, 0, 0});
    StartRound(state, content, events);
    return true;
}

} // namespace recoil
