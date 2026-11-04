#include <array>

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
using recoil::Team;
using recoil::WorldState;

WorldState MakeBombWorld() {
    WorldState state;
    state.player_count = 2;
    state.players[0].alive = true;
    state.players[0].team = static_cast<uint8_t>(Team::T);
    state.players[0].health = 100;
    state.players[0].on_ground = true;
    state.players[0].has_bomb = true;
    state.players[0].weapon.mag = 30;
    state.players[0].weapon.reserve = 90;
    state.players[1].alive = true;
    state.players[1].team = static_cast<uint8_t>(Team::CT);
    state.players[1].health = 100;
    state.players[1].on_ground = true;
    state.players[1].position = {0.0f, 120.0f, 0.0f};
    state.players[1].weapon.mag = 30;
    state.players[1].weapon.reserve = 90;
    state.round.phase = RoundPhase::Live;
    state.round.phase_ticks = 1000;
    return state;
}

Command Cmd(CommandKind kind, uint8_t player = 0) {
    Command command;
    command.kind = kind;
    command.player = player;
    return command;
}

} // namespace

TEST_CASE("T 携带 C4 在回合中可埋包") {
    WorldState state = MakeBombWorld();
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{Cmd(CommandKind::Plant)};

    const auto result = Step(state, content, commands);

    CHECK(result.state.bomb.planted);
    CHECK(result.state.bomb.timer_ticks > 0);
    CHECK(result.state.players[0].has_bomb == false);
}

TEST_CASE("C4 倒计时结束爆炸并判 T 胜") {
    WorldState state = MakeBombWorld();
    const Content content = DefaultContent();
    state.bomb.planted = true;
    state.bomb.timer_ticks = 2;

    auto result = Step(state, content, {});
    result = Step(result.state, content, {});

    CHECK(result.state.round.phase == RoundPhase::RoundEnd);
    CHECK(result.state.round.winner == static_cast<uint8_t>(Team::T));
}

TEST_CASE("CT 持续拆包成功并判 CT 胜") {
    WorldState state = MakeBombWorld();
    state.bomb.planted = true;
    state.bomb.timer_ticks = 1000;
    Content content = DefaultContent();
    content.round.defuse_ticks = 2;
    const std::array<Command, 1> defuse{Cmd(CommandKind::Defuse, 1)};

    auto result = Step(state, content, defuse);
    result = Step(result.state, content, defuse);

    CHECK(result.state.bomb.defused);
    CHECK(result.state.round.phase == RoundPhase::RoundEnd);
    CHECK(result.state.round.winner == static_cast<uint8_t>(Team::CT));
}

TEST_CASE("非携带者不能埋包、非 CT 不能拆包") {
    WorldState state = MakeBombWorld();
    state.players[0].has_bomb = false;
    const Content content = DefaultContent();
    const std::array<Command, 1> plant{Cmd(CommandKind::Plant)};

    const auto planted = Step(state, content, plant);
    CHECK(planted.state.bomb.planted == false);

    WorldState planted_state = MakeBombWorld();
    planted_state.bomb.planted = true;
    planted_state.bomb.timer_ticks = 1000;
    const std::array<Command, 1> defuse_by_t{Cmd(CommandKind::Defuse, 0)};
    const auto defused = Step(planted_state, content, defuse_by_t);
    CHECK(defused.state.bomb.defuse_progress == 0);
}

TEST_CASE("中断拆包会重置进度") {
    WorldState state = MakeBombWorld();
    state.bomb.planted = true;
    state.bomb.timer_ticks = 1000;
    const Content content = DefaultContent();
    const std::array<Command, 1> defuse{Cmd(CommandKind::Defuse, 1)};

    auto result = Step(state, content, defuse);
    CHECK(result.state.bomb.defuse_progress == 1);
    result = Step(result.state, content, {});
    CHECK(result.state.bomb.defuse_progress == 0);
}

TEST_CASE("C4 携带者死亡时失去携带状态") {
    WorldState state = MakeBombWorld();
    state.players[0].health = 30;
    state.players[1].position = {0.0f, 0.0f, 0.0f};
    state.players[1].weapon.mag = 30;
    state.round.phase = RoundPhase::Live;
    Command fire;
    fire.kind = CommandKind::Fire;
    fire.player = 1;
    fire.yaw_degrees = 270.0f;
    const std::array<Command, 1> commands{fire};

    const auto result = Step(state, DefaultContent(), commands);

    CHECK(result.state.players[0].alive == false);
    CHECK(result.state.players[0].has_bomb == false);
}

TEST_CASE("拆弹器计时按实际拆包者生效") {
    WorldState state = MakeBombWorld();
    state.bomb.planted = true;
    state.bomb.timer_ticks = 1000;
    state.players[1].has_defuser = true;
    Content content = DefaultContent();
    content.round.defuse_ticks = 4;
    content.round.defuse_kit_ticks = 2;
    const std::array<Command, 1> defuse{Cmd(CommandKind::Defuse, 1)};

    auto result = Step(state, content, defuse);
    result = Step(result.state, content, defuse);

    CHECK(result.state.bomb.defused);
}
