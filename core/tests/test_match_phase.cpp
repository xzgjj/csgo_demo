#include <array>
#include <cmath>
#include <cstdint>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/content/rules_table.gen.h"
#include "recoil/sim/match.h"
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

TEST_CASE("MTC-02 阶段机：Warmup → Freeze → Live → RoundEnd → 下一回合") {
    Content content = DefaultContent();
    WorldState state = MakeMatch(ModeId::Comp5v5);
    EventList events;
    for (uint16_t i = 0; i < state.match.warmup_ticks; ++i) {
        TickMatch(state, content, events);
    }
    CHECK_EQ(state.round.phase, RoundPhase::Freeze);
    CHECK_EQ(state.round.number, 1);
    for (uint16_t i = 0; i < state.match.freeze_ticks; ++i) {
        TickMatch(state, content, events);
    }
    CHECK_EQ(state.round.phase, RoundPhase::Live);
    CHECK_EQ(state.round.phase_ticks, state.match.round_ticks);
    CHECK_EQ(state.round.number, 1);
}

TEST_CASE("买窗关闭：买窗结束发事件并置 buy_closed") {
    Content content = DefaultContent();
    WorldState state = MakeMatch(ModeId::Comp5v5);
    EventList events;
    state.round.phase = RoundPhase::Freeze;
    state.round.phase_ticks = 0;
    state.round.buy_ticks_left = 3;
    TickMatch(state, content, events);
    CHECK_EQ(state.round.phase, RoundPhase::Live);
    CHECK_FALSE(state.round.buy_closed);
    for (int i = 0; i < 3; ++i) {
        TickMatch(state, content, events);
    }
    CHECK(state.round.buy_closed);
    CHECK_FALSE(CanBuyInMatch(state));
    bool seen = false;
    for (int i = 0; i < events.count; ++i) {
        seen = seen || events.items[i].kind == recoil::EventKind::BuyWindowClosed;
    }
    CHECK(seen);
}

TEST_CASE("MTC-07 行为许可矩阵：四种阶段的允许/拒绝") {
    WorldState state = MakeMatch(ModeId::Comp5v5);
    state.round.phase = RoundPhase::Warmup;
    CHECK(IsActionAllowed(state, Action::Move));
    CHECK(IsActionAllowed(state, Action::Fire));
    CHECK(IsActionAllowed(state, Action::Buy));
    CHECK(IsActionAllowed(state, Action::Respawn));
    CHECK(IsActionAllowed(state, Action::Vote));
    CHECK_FALSE(IsActionAllowed(state, Action::Plant));

    state.round.phase = RoundPhase::Freeze;
    CHECK_FALSE(IsActionAllowed(state, Action::Move));
    CHECK_FALSE(IsActionAllowed(state, Action::Fire));
    CHECK(IsActionAllowed(state, Action::Buy));
    CHECK(IsActionAllowed(state, Action::DropWeapon));
    CHECK_FALSE(IsActionAllowed(state, Action::Pickup));
    CHECK_FALSE(IsActionAllowed(state, Action::SwitchView));

    state.round.phase = RoundPhase::Live;
    CHECK(IsActionAllowed(state, Action::Move));
    CHECK(IsActionAllowed(state, Action::Fire));
    CHECK(IsActionAllowed(state, Action::Plant));
    CHECK(IsActionAllowed(state, Action::Defuse));
    CHECK(IsActionAllowed(state, Action::SwitchView));
    CHECK_FALSE(IsActionAllowed(state, Action::Buy));
    CHECK_FALSE(IsActionAllowed(state, Action::Respawn));

    state.round.phase = RoundPhase::RoundEnd;
    CHECK_FALSE(IsActionAllowed(state, Action::Move));
    CHECK(IsActionAllowed(state, Action::SwitchView));
    CHECK(IsActionAllowed(state, Action::Vote));

    state.round.phase = RoundPhase::MatchEnd;
    CHECK_FALSE(IsActionAllowed(state, Action::Vote));
    CHECK(IsActionAllowed(state, Action::SwitchView));
}

TEST_CASE("MTC-08 异常：全灭/同 tick 拆包/时间到/Freeze 死亡重开") {
    // 9：CT 全灭但包未下 → T 立即胜。
    WorldState ct_wiped = MakeMatch(ModeId::Comp5v5);
    EnterLive(ct_wiped);
    KillTeam(ct_wiped, kSideCt);
    TickMatch(ct_wiped, Content{}, Events());
    CHECK_EQ(ct_wiped.round.phase, RoundPhase::RoundEnd);
    CHECK_EQ(ct_wiped.round.winner, kSideT);

    // 8：T 全灭但包已下 → 不判，CT 仍需拆包。
    WorldState t_wiped = MakeMatch(ModeId::Comp5v5);
    EnterLive(t_wiped);
    t_wiped.bomb.planted = true;
    KillTeam(t_wiped, kSideT);
    TickMatch(t_wiped, Content{}, Events());
    CHECK_EQ(t_wiped.round.phase, RoundPhase::Live);

    // 2：拆包与引爆同 tick → 拆包优先（CT 胜）。
    WorldState both = MakeMatch(ModeId::Comp5v5);
    EnterLive(both);
    both.bomb.defused = true;
    both.bomb.exploded = true;
    TickMatch(both, Content{}, Events());
    CHECK_EQ(both.round.winner, kSideCt);
    CHECK_EQ(both.round.end_reason, static_cast<uint8_t>(RoundEndReason::BombDefused));

    // 4：时间到且包未下 → CT 胜。
    WorldState timeout = MakeMatch(ModeId::Comp5v5);
    EnterLive(timeout, 1);
    TickMatch(timeout, Content{}, Events());
    TickMatch(timeout, Content{}, Events());
    CHECK_EQ(timeout.round.phase, RoundPhase::RoundEnd);
    CHECK_EQ(timeout.round.winner, kSideCt);

    // 10：Freeze 内死亡 → DeadRound 重开，金钱不变。
    WorldState dead = MakeMatch(ModeId::Comp5v5);
    dead.round.phase = RoundPhase::Freeze;
    dead.players[0].money = 1234;
    MatchEndRound(dead, Content{}, 0, RoundEndReason::DeadRound, Events());
    CHECK_EQ(dead.round.phase, RoundPhase::Freeze);
    CHECK_EQ(dead.players[0].money, 1234);
}

TEST_CASE("暂停：只在 Freeze 生效、次数用尽拒绝、恢复回原阶段") {
    Content content = DefaultContent();
    WorldState state = MakeMatch(ModeId::Comp5v5);
    state.round.phase = RoundPhase::Live;
    CHECK_FALSE(RequestTimeout(state, 0));

    state.round.phase = RoundPhase::Freeze;
    CHECK(RequestTimeout(state, 0));
    CHECK_EQ(state.round.phase, RoundPhase::Timeout);
    CHECK_EQ(state.round.timeout_left[0], 2);
    CHECK_FALSE(RequestTimeout(state, 0)); // 暂停中不可叠加

    EndTimeout(state, Events());
    CHECK_EQ(state.round.phase, RoundPhase::Freeze);

    state.round.timeout_left[0] = 0;
    CHECK_FALSE(RequestTimeout(state, 0));
}

TEST_CASE("投降：达到门槛后立即 MatchEnd，比分保留") {
    Content content = DefaultContent();
    WorldState state = MakeMatch(ModeId::Comp5v5);
    state.round.score_team = {{6, 2}};
    state.round.number = 9;
    CHECK_FALSE(VoteSurrender(state, content, 0, Events()));
    state.round.number = 10;
    CHECK(VoteSurrender(state, content, 0, Events()));
    CHECK_EQ(state.round.phase, RoundPhase::MatchEnd);
    CHECK_EQ(state.round.score_team[0], 6);
    CHECK_EQ(state.round.score_team[1], 2);

    // 3v3 门槛第 6 回合；1v1 任意回合。
    WorldState squad = MakeMatch(ModeId::Squad3v3);
    squad.round.number = 6;
    CHECK(VoteSurrender(squad, content, 1, Events()));
}
