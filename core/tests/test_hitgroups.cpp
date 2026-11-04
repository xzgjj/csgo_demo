#include <array>
#include <cstdint>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/sim/step.h"

namespace {

using recoil::Command;
using recoil::CommandKind;
using recoil::Content;
using recoil::DefaultContent;
using recoil::RoundPhase;
using recoil::Step;
using recoil::StepResult;
using recoil::Team;
using recoil::WorldState;

WorldState MakeDuel() {
    WorldState state;
    state.player_count = 2;
    for (int i = 0; i < 2; ++i) {
        auto& player = state.players[static_cast<size_t>(i)];
        player.alive = true;
        player.health = 100;
        player.weapon.mag = 30;
        player.weapon.reserve = 90;
        player.team = static_cast<uint8_t>(i == 0 ? Team::T : Team::CT);
        player.position = {0.0f, static_cast<float>(i) * 500.0f, 0.0f};
    }
    return state;
}

Command FirePitch(uint8_t player, float yaw, float pitch) {
    Command command;
    command.kind = CommandKind::Fire;
    command.player = player;
    command.yaw_degrees = yaw;
    command.pitch_degrees = pitch;
    return command;
}

TEST_CASE("头部命中按 4 倍伤害，一枪击杀满血目标") {
    WorldState state = MakeDuel();
    state.players[1].position = {0.0f, 300.0f, 0.0f};
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FirePitch(0, 90.0f, 0.8f)};

    const StepResult result = Step(state, content, commands);

    CHECK(result.state.players[1].health == 0);
    CHECK(result.state.players[1].alive == false);
}

TEST_CASE("腿部命中按 0.75 倍伤害") {
    WorldState state = MakeDuel();
    state.players[1].position = {0.0f, 300.0f, 0.0f};
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FirePitch(0, 90.0f, -8.0f)};

    const StepResult result = Step(state, content, commands);

    CHECK(result.state.players[1].health == 73);
}

} // namespace

TEST_CASE("击杀队友不给击杀数与经济奖励") {
    WorldState state = MakeDuel();
    state.players[1].team = static_cast<uint8_t>(Team::T);
    state.players[1].position = {0.0f, 500.0f, 0.0f};
    state.players[1].health = 36;
    state.players[0].money = 800;
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FirePitch(0, 90.0f, 0.0f)};

    const StepResult result = Step(state, content, commands);

    CHECK(result.state.players[1].alive == false);
    CHECK(result.state.players[0].kills == 0);
    CHECK(result.state.players[0].money == 800);
}

TEST_CASE("比赛结束后不能再开火") {
    WorldState state = MakeDuel();
    state.round.phase = RoundPhase::MatchEnd;
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FirePitch(0, 90.0f, 0.0f)};

    const StepResult result = Step(state, content, commands);

    CHECK(result.state.players[0].weapon.mag == 30);
}
