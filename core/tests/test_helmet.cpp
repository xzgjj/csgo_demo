#include <array>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/sim/step.h"

namespace {

using recoil::BuyItem;
using recoil::Command;
using recoil::CommandKind;
using recoil::Content;
using recoil::DefaultContent;
using recoil::RoundPhase;
using recoil::Step;
using recoil::StepResult;
using recoil::Team;
using recoil::WorldState;

WorldState Duel() {
    WorldState state;
    state.player_count = 2;
    state.players[0].alive = true;
    state.players[0].team = static_cast<uint8_t>(Team::T);
    state.players[0].health = 100;
    state.players[0].money = 2000;
    state.players[0].on_ground = true;
    state.players[0].weapon.mag = 30;
    state.players[1].alive = true;
    state.players[1].team = static_cast<uint8_t>(Team::CT);
    state.players[1].health = 100;
    state.players[1].on_ground = true;
    state.players[1].position = {0.0f, 300.0f, 0.0f};
    state.players[1].weapon.mag = 30;
    state.round.phase = RoundPhase::Freeze;
    state.round.phase_ticks = 100;
    return state;
}

Command Buy(BuyItem item) {
    Command command;
    command.kind = CommandKind::Buy;
    command.player = 0;
    command.item = item;
    return command;
}

Command FirePitch(float pitch) {
    Command command;
    command.kind = CommandKind::Fire;
    command.player = 0;
    command.yaw_degrees = 90.0f;
    command.pitch_degrees = pitch;
    return command;
}

} // namespace

TEST_CASE("购买头盔扣钱并生效") {
    WorldState state = Duel();
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{Buy(BuyItem::Helmet)};

    const StepResult result = Step(state, content, commands);

    CHECK(result.state.players[0].helmet);
    CHECK(result.state.players[0].armor == 100);
    CHECK(result.state.players[0].money == 2000 - 1000);
}

TEST_CASE("戴头盔时头部伤害从 4 倍降为 2 倍") {
    WorldState state = Duel();
    state.round.phase = RoundPhase::Live;
    state.players[1].helmet = true;
    state.players[1].armor = 100;
    state.players[1].health = 100;
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FirePitch(0.8f)};

    const StepResult result = Step(state, content, commands);

    CHECK(result.state.players[1].health > 0);
    CHECK(result.state.players[1].health < 50);
}
