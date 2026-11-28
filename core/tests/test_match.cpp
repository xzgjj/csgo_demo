#include <array>
#include <cmath>
#include <cstdint>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/content/rules_table.gen.h"
#include "recoil/sim/match.h"
#include "recoil/sim/mode.h"
#include "recoil/sim/round.h"

namespace {

using recoil::Action;
using recoil::BeginMatchWithRules;
using recoil::CanBuyInMatch;
using recoil::Content;
using recoil::DefaultContent;
using recoil::EndTimeout;
using recoil::EventList;
using recoil::IsActionAllowed;
using recoil::MatchEndRound;
using recoil::MatchError;
using recoil::MatchRules;
using recoil::ModeId;
using recoil::RequestTimeout;
using recoil::RoundEndReason;
using recoil::RoundPhase;
using recoil::ScoreOf;
using recoil::StartOvertime;
using recoil::SwapSides;
using recoil::Team;
using recoil::TickMatch;
using recoil::VoteSurrender;
using recoil::WorldState;
using recoil::content::MatchRulesFor;

constexpr uint8_t kSideT = static_cast<uint8_t>(Team::T);
constexpr uint8_t kSideCt = static_cast<uint8_t>(Team::CT);

// 测试用事件收集器：每次取用前清空，避免在断言辅助函数里传临时对象。
EventList& Events() {
    static EventList events;
    events = EventList{};
    return events;
}

WorldState MakeMatch(ModeId mode, MatchError* error = nullptr) {
    WorldState state;
    EventList events;
    const MatchError result = BeginMatchWithRules(state, MatchRulesFor(mode), events);
    if (error != nullptr) {
        *error = result;
    }
    return state;
}

// 跳过 Warmup/Freeze，直接进入 Live 并清空回合计时，便于单点断言结算逻辑。
void EnterLive(WorldState& state, uint16_t round_ticks = 1000) {
    state.round.phase = RoundPhase::Live;
    state.round.phase_ticks = round_ticks;
    state.round.round_ticks_left = round_ticks;
    state.round.buy_closed = true;
}

void KillTeam(WorldState& state, uint8_t side) {
    for (uint8_t i = 0; i < state.player_count; ++i) {
        if (state.players[i].team == side) {
            state.players[i].alive = false;
            state.players[i].health = 0;
        }
    }
}

void SettleRound(WorldState& state, uint8_t winner_side, RoundEndReason reason) {
    EnterLive(state);
    MatchEndRound(state, Content{}, winner_side, reason, Events());
}

} // namespace

TEST_CASE("MTC-01 模式矩阵：人数/包点/回合数/换边点/资金上限") {
    const MatchRules& duel = MatchRulesFor(ModeId::Duel1v1);
    CHECK(duel.enabled);
    CHECK_EQ(duel.team_size, 1);
    CHECK_EQ(duel.site_count, 1);
    CHECK_EQ(duel.max_rounds, 10);
    CHECK_EQ(duel.win_score, 6);
    CHECK_EQ(duel.swap_round(), 5);
    CHECK_EQ(duel.money_cap, 8000u);
    CHECK_EQ(duel.overtime_cap, 0);

    const MatchRules& squad = MatchRulesFor(ModeId::Squad3v3);
    CHECK_EQ(squad.team_size, 3);
    CHECK_EQ(squad.site_count, 1);
    CHECK_EQ(squad.max_rounds, 16);
    CHECK_EQ(squad.win_score, 9);
    CHECK_EQ(squad.swap_round(), 8);
    CHECK_EQ(squad.money_cap, 12000u);
    CHECK_EQ(squad.ot_win_score, 4);

    const MatchRules& comp = MatchRulesFor(ModeId::Comp5v5);
    CHECK_EQ(comp.team_size, 5);
    CHECK_EQ(comp.site_count, 2);
    CHECK_EQ(comp.max_rounds, 24);
    CHECK_EQ(comp.win_score, 13);
    CHECK_EQ(comp.swap_round(), 12);
    CHECK_EQ(comp.money_cap, 16000u);
    CHECK_EQ(comp.round_ticks, 115 * 64);
    CHECK_EQ(comp.freeze_ticks, 15 * 64);
    CHECK_EQ(comp.c4_ticks, 40 * 64);
}

TEST_CASE("E01/E02/E03：模式非法、人数不符、包点冲突") {
    MatchRules bad = MatchRulesFor(ModeId::Comp5v5);
    bad.enabled = false;
    WorldState state;
    CHECK_EQ(BeginMatchWithRules(state, bad, Events()), MatchError::ModeInvalid);

    MatchRules site = MatchRulesFor(ModeId::Comp5v5);
    site.site_count = 1; // 5v5 必须双点
    WorldState state2;
    CHECK_EQ(BeginMatchWithRules(state2, site, Events()), MatchError::SiteConflict);

    WorldState state3;
    state3.player_count = 9; // 5v5 需要 10 人
    CHECK_EQ(BeginMatchWithRules(state3, MatchRulesFor(ModeId::Comp5v5), Events()),
             MatchError::RosterMismatch);

    WorldState ok;
    CHECK_EQ(BeginMatchWithRules(ok, MatchRulesFor(ModeId::Comp5v5), Events()), MatchError::Ok);
    CHECK_EQ(ok.player_count, 10);
    CHECK_EQ(ok.round.phase, RoundPhase::Warmup);
}

TEST_CASE("MTC-04 经济：胜/负奖励、上限截断、下包后仍输补偿") {
    WorldState state = MakeMatch(ModeId::Comp5v5);
    for (uint8_t i = 0; i < state.player_count; ++i) {
        state.players[i].money = 800;
    }
    SettleRound(state, kSideT, RoundEndReason::Elimination);
    CHECK_EQ(state.players[0].money, 4050); // 800 + 3250
    CHECK_EQ(state.players[9].money, 2200); // 800 + 1400（首败第一档）
    CHECK_EQ(ScoreOf(state, 0), 1);

    // 目标奖励（引爆）高于歼灭：3500。
    WorldState bombs = MakeMatch(ModeId::Comp5v5);
    for (uint8_t i = 0; i < bombs.player_count; ++i) {
        bombs.players[i].money = 1000;
    }
    SettleRound(bombs, kSideT, RoundEndReason::BombExploded);
    CHECK_EQ(bombs.players[0].money, 4500);
    CHECK_EQ(bombs.players[9].money, 2400);

    // 上限截断：15500 + 3250 → 16000。
    WorldState cap = MakeMatch(ModeId::Comp5v5);
    cap.players[0].money = 15500;
    SettleRound(cap, kSideT, RoundEndReason::Elimination);
    CHECK_EQ(cap.players[0].money, 16000);

    // T 下包后仍输：失败补偿 1400 + 600。
    WorldState plant = MakeMatch(ModeId::Comp5v5);
    plant.bomb.planted = true;
    for (uint8_t i = 0; i < plant.player_count; ++i) {
        plant.players[i].money = 800;
    }
    SettleRound(plant, kSideCt, RoundEndReason::BombDefused);
    CHECK_EQ(plant.players[0].money, 2800); // 800 + 1400 + 600
    CHECK_EQ(plant.players[9].money, 4300); // 800 + 3500（拆包成功属目标奖励）
}

TEST_CASE("MTC-05 连败阶梯：1400/1900/2400/2900/3400，赢一局降一档") {
    WorldState state = MakeMatch(ModeId::Comp5v5);
    for (uint8_t i = 0; i < state.player_count; ++i) {
        state.players[i].money = 0;
    }
    const std::array<uint16_t, 5> expect{{1400, 1900, 2400, 2900, 3400}};
    uint16_t prev = 0;
    for (size_t i = 0; i < expect.size(); ++i) {
        SettleRound(state, kSideT, RoundEndReason::Elimination);
        CHECK_EQ(state.players[9].money, static_cast<uint16_t>(prev + expect[i]));
        prev = state.players[9].money;
        CHECK_EQ(state.round.loss_streak[1], static_cast<uint8_t>(i + 2 > 5 ? 5 : i + 2));
    }
    // 赢一局：降一档但不回头给钱，后续再输按第 5 档期望（loss_streak 5 → 4）。
    WorldState win = MakeMatch(ModeId::Comp5v5);
    win.round.loss_streak = {{1, 5}};
    win.players[9].money = 0;
    SettleRound(win, kSideCt, RoundEndReason::Elimination);
    CHECK_EQ(win.round.loss_streak[1], 4);
    CHECK_EQ(win.players[9].money, 3250);
}

TEST_CASE("MTC-03 换边：side/team 翻转、经济重置、比分归属不变") {
    Content content = DefaultContent();
    WorldState state = MakeMatch(ModeId::Comp5v5);
    state.round.score_team = {{8, 4}};
    state.round.number = 12;
    EventList events;
    SwapSides(state, content, events);
    CHECK_EQ(state.round.side[0], kSideCt);
    CHECK_EQ(state.round.side[1], kSideT);
    CHECK_EQ(state.players[0].team, kSideCt);
    CHECK_EQ(state.players[9].team, kSideT);
    CHECK_EQ(state.round.score_team[0], 8);
    CHECK_EQ(state.round.score_team[1], 4);
    CHECK_EQ(state.round.score_t, 4); // T 现在由 team 1 承担
    CHECK_EQ(state.round.score_ct, 8);
    CHECK_EQ(state.round.loss_streak[0], 1);
    CHECK_EQ(state.round.loss_streak[1], 1);
    CHECK_EQ(state.players[0].money, 800);
    bool swapped = false;
    for (int i = 0; i < events.count; ++i) {
        swapped = swapped || events.items[i].kind == recoil::EventKind::SideSwapped;
    }
    CHECK(swapped);
}

TEST_CASE("半场：第 12 回合结束进 Halftime 并已完成换边") {
    Content content = DefaultContent();
    WorldState state = MakeMatch(ModeId::Comp5v5);
    state.round.score_team = {{7, 5}};
    state.round.number = 12;
    SettleRound(state, kSideT, RoundEndReason::Elimination);
    state.round.phase_ticks = 1;
    TickMatch(state, content, Events());
    CHECK_EQ(state.round.phase, RoundPhase::Halftime);
    CHECK_EQ(state.round.half, 2);
    CHECK_EQ(state.round.side[0], kSideCt);
}

TEST_CASE("MTC-06 加时：12–12 进 OT、先到 4 结束、$10,000、上限平局") {
    Content content = DefaultContent();
    WorldState state = MakeMatch(ModeId::Comp5v5);
    state.round.score_team = {{12, 12}};
    state.round.number = 24;
    state.round.half = 2;
    state.round.phase = RoundPhase::RoundEnd;
    state.round.phase_ticks = 1;
    TickMatch(state, content, Events());
    CHECK_EQ(state.round.overtime, 1);
    CHECK_EQ(state.round.score_team[0], 0);
    CHECK_EQ(state.round.score_team[1], 0);
    CHECK_EQ(state.players[0].money, 10000);
    CHECK_EQ(state.round.phase, RoundPhase::Freeze);

    // 3–0 再赢一局 → 4 → MatchEnd。
    state.round.score_team = {{3, 0}};
    SettleRound(state, kSideT, RoundEndReason::Elimination);
    state.round.phase_ticks = 1;
    TickMatch(state, content, Events());
    CHECK_EQ(state.round.phase, RoundPhase::MatchEnd);

    // 上限：overtime 已经到 cap 时 StartOvertime 返回 false。
    WorldState cap = MakeMatch(ModeId::Comp5v5);
    cap.round.overtime = cap.match.overtime_cap;
    CHECK_FALSE(StartOvertime(cap, content, Events()));

    // 1v1 无加时 → 5–5 判平。
    WorldState duel = MakeMatch(ModeId::Duel1v1);
    duel.round.score_team = {{5, 5}};
    CHECK_FALSE(StartOvertime(duel, content, Events()));
}

TEST_CASE("红线 4：EndRound 幂等，不重复记分与发钱") {
    WorldState state = MakeMatch(ModeId::Comp5v5);
    state.players[0].money = 800;
    SettleRound(state, kSideT, RoundEndReason::Elimination);
    const uint16_t after_first = state.players[0].money;
    const uint16_t score_first = state.round.score_team[0];
    MatchEndRound(state, Content{}, kSideT, RoundEndReason::Elimination, Events());
    CHECK_EQ(state.players[0].money, after_first);
    CHECK_EQ(state.round.score_team[0], score_first);
}

// 模式预置（mode.h）：开关的唯一映射 + 语义谓词 + 一致性自检。
TEST_CASE("模式预置只写开关，三种组合与谓词一致") {
    using recoil::ApplyModePreset;
    using recoil::IsCompetitive;
    using recoil::IsLegacyDemo;
    using recoil::IsTraining;
    using recoil::PlayMode;

    WorldState state;
    Content content = DefaultContent();

    ApplyModePreset(state, content, PlayMode::kLegacyDemo);
    CHECK_FALSE(state.match.enabled);
    CHECK_FALSE(content.buy.enabled);
    CHECK(IsLegacyDemo(state, content));
    CHECK_FALSE(IsTraining(state, content));

    ApplyModePreset(state, content, PlayMode::kTraining);
    CHECK_FALSE(state.match.enabled);
    CHECK(content.buy.enabled);
    CHECK(IsTraining(state, content));
    CHECK_FALSE(IsLegacyDemo(state, content));
    CHECK_FALSE(IsCompetitive(state));

    ApplyModePreset(state, content, PlayMode::kCompetitive, ModeId::Squad3v3);
    CHECK(state.match.enabled);
    CHECK(content.buy.enabled);
    CHECK(IsCompetitive(state));
    CHECK(state.match.mode == ModeId::Squad3v3);
    CHECK_EQ(state.match.team_size, 3);
    CHECK_EQ(state.match.win_score, 9); // Squad3v3 按 rules.json：win_score=9
}

TEST_CASE("competitive 预置与 BeginMatchWithRules 自洽，不会 RosterMismatch") {
    using recoil::ApplyModePreset;
    using recoil::PlayMode;

    WorldState state;
    Content content = DefaultContent();
    ApplyModePreset(state, content, PlayMode::kCompetitive, ModeId::Comp5v5);
    state.player_count = 10;
    EventList events;
    const MatchError error = BeginMatchWithRules(state, state.match, events);
    CHECK(error == MatchError::Ok);
    CHECK(state.match.enabled);
    CHECK_EQ(state.player_count, 10);
}

TEST_CASE("谓词逐项等价旧开关（四种布尔组合穷举）") {
    using recoil::ApplyModePreset;
    using recoil::IsCompetitive;
    using recoil::IsLegacyDemo;
    using recoil::IsTraining;
    using recoil::PlayMode;

    for (int mask = 0; mask < 4; ++mask) {
        WorldState state;
        Content content = DefaultContent();
        state.match.enabled = (mask & 1) != 0;
        content.buy.enabled = (mask & 2) != 0;
        CHECK(IsCompetitive(state) == state.match.enabled);
        CHECK(recoil::CatalogBuyOn(content) == content.buy.enabled);
        CHECK(IsTraining(state, content) == (!state.match.enabled && content.buy.enabled));
        CHECK(IsLegacyDemo(state, content) == (!state.match.enabled && !content.buy.enabled));
    }
    // 预置应用不得改变旧的默认态（DefaultContent 目录购买为关）。
    Content content = DefaultContent();
    WorldState state;
    ApplyModePreset(state, content, PlayMode::kLegacyDemo);
    CHECK_FALSE(content.buy.enabled);
}
