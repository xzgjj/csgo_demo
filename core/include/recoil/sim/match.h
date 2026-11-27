#pragma once

#include <cstdint>

#include "recoil/content/content.h"
#include "recoil/sim/event.h"
#include "recoil/sim/match_types.h"
#include "recoil/sim/state.h"

namespace recoil {

// 赛制错误码（SPEC-03 §3.13）。BeginMatch 系列返回它，Ok 之外的每个分支都有单测。
enum class MatchError : uint8_t {
    Ok = 0,
    ModeInvalid = 1,        // E01：模式越界/配置缺失
    RosterMismatch = 2,     // E02：玩家数 != 2*team_size
    SiteConflict = 3,       // E03：包点数与模式不符
    OvertimeCap = 4,        // E04：加时次数超上限
    TimeoutUnavailable = 5, // E05：次数用尽 / 不在 Freeze
    BuyClosed = 6,          // E06：买窗已关
    ActionNotAllowed = 7,   // E07：阶段行为许可拒绝
};

// 开始一场按 rules 配置的对局：校验模式/人数/包点，初始化 team_id、side、比分与初始经济，
// 进入 Warmup。旧调用方仍可用 round.h 的 BeginMatch（等价于 enabled=false 的 5v5）。
MatchError BeginMatchWithRules(WorldState& state, const MatchRules& rules, EventList& events);

// 阶段推进（幂等）：Warmup→Freeze、Freeze→Live、买窗关闭、Live
// 计时、RoundEnd→下一回合/半场/加时/结算。 不负责击杀判定，交火结果由 Step/EndRound 处理。
void TickMatch(WorldState& state, const Content& content, EventList& events);

// 回合结算：记分（按 team_id）、发钱（胜/负阶梯/个人奖励）、连败、半场/加时/终局判定。
// 幂等：phase 不是 Live/Warmup 时直接返回（红线 4）。
void MatchEndRound(WorldState& state, const Content& content, uint8_t winner_side,
                   RoundEndReason reason, EventList& events);

// 原子换边（§3.6.3 八步）：side/player.team、出生映射、经济、连败、掉落，比分与 team_id 不变。
void SwapSides(WorldState& state, const Content& content, EventList& events);

// 进入下一个加时：比分清零、经济 ot_money、连败重置；超过 overtime_cap 返回 false（E04）。
bool StartOvertime(WorldState& state, const Content& content, EventList& events);

// 阶段行为许可（10×10 矩阵）。服务端每处输入入口调用一次。
bool IsActionAllowed(const WorldState& state, Action action);

// 战术暂停：只有 Freeze 阶段、次数未用尽、本队请求成功；进入 Timeout 并冻结计时。
bool RequestTimeout(WorldState& state, uint8_t team_id);

// 恢复暂停：计时到或手动恢复，回到 resume_phase。
void EndTimeout(WorldState& state, EventList& events);

// 投降投票：单机下玩家 1 票即通过（人机自动同意），判对方胜并进 MatchEnd。
bool VoteSurrender(WorldState& state, const Content& content, uint8_t team_id, EventList& events);

// 小工具：当前阶段是否处于“可以买”的窗口（Freeze 前 buy_ticks，或 Live 后 buy 宽限在旧逻辑）。
bool CanBuyInMatch(const WorldState& state);

// 按 team_id 取当前比分（HUD/结算页用）。
uint16_t ScoreOf(const WorldState& state, uint8_t team_id);

// 场上某队人数（含死亡），用于校验与人机补齐。
uint8_t RosterSize(const WorldState& state, uint8_t team_id);

} // namespace recoil
