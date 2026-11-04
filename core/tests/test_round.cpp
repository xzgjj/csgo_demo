#include <array>
#include <cstdint>
#include <span>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/sim/step.h"

namespace {

using recoil::BuyItem;
using recoil::Command;
using recoil::CommandKind;
using recoil::Content;
using recoil::DefaultContent;
using recoil::EventKind;
using recoil::EventList;
using recoil::RoundPhase;
using recoil::Step;
using recoil::StepResult;
using recoil::Team;
using recoil::WorldState;

WorldState MakeRound() {
    WorldState state;
    state.player_count = 2;
    state.players[0].alive = true;
    state.players[0].team = static_cast<uint8_t>(Team::T);
    state.players[0].health = 100;
    state.players[0].money = 800;
    state.players[0].on_ground = true;
    state.players[0].weapon.mag = 30;
    state.players[0].weapon.reserve = 90;
    state.players[1].alive = true;
    state.players[1].team = static_cast<uint8_t>(Team::CT);
    state.players[1].health = 100;
    state.players[1].money = 800;
    state.players[1].on_ground = true;
    state.players[1].weapon.mag = 30;
    state.players[1].weapon.reserve = 90;
    state.players[1].position = {400.0f, 0.0f, 0.0f};
    state.round.phase = RoundPhase::Freeze;
    state.round.phase_ticks = 2;
    return state;
}

Command FireAt() {
    Command command;
    command.kind = CommandKind::Fire;
    command.player = 0;
    command.yaw_degrees = 0.0f; // 靶子在 +X 方向
    return command;
}

Command BuyArmor() {
    Command command;
    command.kind = CommandKind::Buy;
    command.player = 0;
    command.item = BuyItem::Armor;
    return command;
}

int CountEvents(const EventList& events, EventKind kind) {
    int total = 0;
    for (int i = 0; i < events.count; ++i) {
        if (events.items[i].kind == kind) {
            ++total;
        }
    }
    return total;
}

} // namespace

TEST_CASE("冻结时间结束后进入进行中") {
    WorldState state = MakeRound();
    const Content content = DefaultContent();

    StepResult result = Step(state, content, {});
    CHECK(result.state.round.phase == RoundPhase::Freeze);
    result = Step(result.state, content, {});
    CHECK(result.state.round.phase == RoundPhase::Live);
    CHECK(CountEvents(result.events, EventKind::RoundLive) == 1);
}

TEST_CASE("击杀最后一名敌人结束回合并结算经济") {
    WorldState state = MakeRound();
    state.round.phase = RoundPhase::Live;
    state.players[1].health = 36;
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FireAt()};

    const StepResult result = Step(state, content, commands);

    CHECK(result.state.round.phase == RoundPhase::RoundEnd);
    CHECK(result.state.round.winner == static_cast<uint8_t>(Team::T));
    CHECK(result.state.round.score_t == 1);
    CHECK(result.state.players[0].money == 800 + 300 + 3250);
    CHECK(result.state.players[1].money == 800 + 1400);
    CHECK(CountEvents(result.events, EventKind::RoundEnded) == 1);
}

TEST_CASE("冻结期购买护甲扣钱并生效") {
    WorldState state = MakeRound();
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{BuyArmor()};

    const StepResult result = Step(state, content, commands);

    CHECK(result.state.players[0].money == 150);
    CHECK(result.state.players[0].armor == 100);
    CHECK(CountEvents(result.events, EventKind::BuySucceeded) == 1);
}

TEST_CASE("非冻结期购买被拒绝") {
    WorldState state = MakeRound();
    state.round.phase = RoundPhase::Live;
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{BuyArmor()};

    const StepResult result = Step(state, content, commands);

    CHECK(result.state.players[0].money == 800);
    CHECK(result.state.players[0].armor == 0);
    CHECK(CountEvents(result.events, EventKind::BuyFailed) == 1);
}

TEST_CASE("回合结束后重置并进入下一回合冻结期") {
    WorldState state = MakeRound();
    state.round.phase = RoundPhase::RoundEnd;
    state.round.phase_ticks = 2;
    state.players[0].health = 10;
    state.players[0].weapon.mag = 1;
    const Content content = DefaultContent();

    StepResult result = Step(state, content, {});
    result = Step(result.state, content, {});

    CHECK(result.state.round.phase == RoundPhase::Freeze);
    CHECK(result.state.round.number == 2);
    CHECK(result.state.players[0].health == 100);
    CHECK(result.state.players[0].weapon.mag == 30);
    CHECK(CountEvents(result.events, EventKind::RoundReset) == 1);
}

TEST_CASE("进行中时间耗尽由 CT 获胜") {
    WorldState state = MakeRound();
    state.round.phase = RoundPhase::Live;
    state.round.phase_ticks = 2;
    const Content content = DefaultContent();

    StepResult result = Step(state, content, {});
    result = Step(result.state, content, {});

    CHECK(result.state.round.phase == RoundPhase::RoundEnd);
    CHECK(result.state.round.winner == static_cast<uint8_t>(Team::CT));
    CHECK(result.state.round.score_ct == 1);
}

TEST_CASE("回合重置清理烟雾/投掷物/致盲") {
    WorldState state = MakeRound();
    state.round.phase = RoundPhase::RoundEnd;
    state.round.phase_ticks = 1;
    state.smoke.active = true;
    state.smoke.ticks_left = 100;
    state.grenades[0].active = true;
    state.players[0].blind_ticks = 50;
    const Content content = DefaultContent();

    const auto result = Step(state, content, {});

    CHECK(result.state.smoke.active == false);
    CHECK(result.state.grenades[0].active == false);
    CHECK(result.state.players[0].blind_ticks == 0);
}

TEST_CASE("回合重置恢复护甲/头盔/C4 初始状态") {
    WorldState state = MakeRound();
    state.round.phase = RoundPhase::RoundEnd;
    state.round.phase_ticks = 1;
    state.players[0].armor = 100;
    state.players[0].helmet = true;
    state.bomb.planted = true;
    state.bomb.timer_ticks = 1000;
    const Content content = DefaultContent();

    const auto result = Step(state, content, {});

    CHECK(result.state.players[0].armor == 0);
    CHECK(result.state.players[0].helmet == false);
    CHECK(result.state.bomb.planted == false);
}

TEST_CASE("回合重置不会把 C4 留在错误玩家身上") {
    WorldState state = MakeRound();
    state.round.phase = RoundPhase::RoundEnd;
    state.round.phase_ticks = 1;
    state.players[1].has_bomb = true;
    const Content content = DefaultContent();

    const auto result = Step(state, content, {});

    CHECK(result.state.players[0].has_bomb);
    CHECK(result.state.players[1].has_bomb == false);
}
