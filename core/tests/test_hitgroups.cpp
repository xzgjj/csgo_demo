#include <array>
#include <cstdint>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/sim/hitbox.h"
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

TEST_CASE("友军不开火伤害：队友不死亡也不给奖励（friendly fire off）") {
    WorldState state = MakeDuel();
    state.players[1].team = static_cast<uint8_t>(Team::T);
    state.players[1].position = {0.0f, 500.0f, 0.0f};
    state.players[1].health = 36;
    state.players[0].money = 800;
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FirePitch(0, 90.0f, 0.0f)};

    const StepResult result = Step(state, content, commands);

    CHECK(result.state.players[1].alive == true);
    CHECK(result.state.players[1].health == 36);
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

TEST_CASE("比赛结束后比分与回合不再变化") {
    WorldState state = MakeDuel();
    state.round.phase = RoundPhase::MatchEnd;
    state.round.score_t = 1;
    state.round.score_ct = 0;
    const Content content = DefaultContent();
    const std::array<Command, 0> commands{};

    auto result = Step(state, content, commands);
    for (int i = 0; i < 50; ++i) {
        result = Step(result.state, content, commands);
    }

    CHECK(result.state.round.score_t == 1);
    CHECK(result.state.round.score_ct == 0);
    CHECK(result.state.round.phase == RoundPhase::MatchEnd);
}

TEST_CASE("命中盒：站立胸部命中、蹲伏同高度射线落空") {
    WorldState state = MakeDuel();
    recoil::PlayerState& target = state.players[1];
    target.position = {0.0f, 300.0f, 0.0f};
    target.view_yaw = 0.0f;

    const recoil::PlayerHit standing =
        recoil::TracePlayerHitboxes(target, {0.0f, 0.0f, 60.0f}, {0.0f, 1.0f, 0.0f}, 1000.0f);
    CHECK(standing.hit);
    CHECK(standing.group == recoil::HitGroup::Chest);

    target.ducking = true;
    const recoil::PlayerHit ducking =
        recoil::TracePlayerHitboxes(target, {0.0f, 0.0f, 60.0f}, {0.0f, 1.0f, 0.0f}, 1000.0f);
    CHECK_FALSE(ducking.hit);
}

TEST_CASE("命中盒随视角偏航旋转：侧面进入点随深度变化") {
    WorldState state = MakeDuel();
    recoil::PlayerState& target = state.players[1];
    target.position = {0.0f, 0.0f, 0.0f};
    target.view_yaw = 0.0f;

    const recoil::PlayerHit front =
        recoil::TracePlayerHitboxes(target, {-300.0f, 0.0f, 56.0f}, {1.0f, 0.0f, 0.0f}, 1000.0f);
    CHECK(front.hit);
    CHECK(front.point.x == doctest::Approx(-16.0f));

    target.view_yaw = 90.0f;
    const recoil::PlayerHit side =
        recoil::TracePlayerHitboxes(target, {-300.0f, 0.0f, 56.0f}, {1.0f, 0.0f, 0.0f}, 1000.0f);
    CHECK(side.hit);
    CHECK(side.point.x == doctest::Approx(-10.0f));
}

TEST_CASE("蹲伏时头部高度上限从 72 降到 54") {
    WorldState state = MakeDuel();
    recoil::PlayerState& target = state.players[1];
    target.position = {0.0f, 0.0f, 0.0f};
    target.view_yaw = 0.0f;
    target.ducking = true;

    const recoil::PlayerHit head =
        recoil::TracePlayerHitboxes(target, {-300.0f, 0.0f, 51.0f}, {1.0f, 0.0f, 0.0f}, 1000.0f);
    CHECK(head.hit);
    CHECK(head.group == recoil::HitGroup::Head);

    const recoil::PlayerHit above =
        recoil::TracePlayerHitboxes(target, {-300.0f, 0.0f, 60.0f}, {1.0f, 0.0f, 0.0f}, 1000.0f);
    CHECK_FALSE(above.hit);
}

TEST_CASE("材质穿透：玻璃后仍受伤，混凝土完全挡住") {
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FirePitch(0, 90.0f, 0.0f)};

    WorldState plain = MakeDuel();
    plain.players[1].position = {0.0f, 300.0f, 0.0f};
    const StepResult plain_result = Step(plain, content, commands);

    WorldState glass = MakeDuel();
    glass.players[1].position = {0.0f, 300.0f, 0.0f};
    recoil::MapDef glass_map;
    glass_map.wall_count = 1;
    glass_map.walls[0] = recoil::Wall{{-50.0f, 100.0f, 0.0f}, {50.0f, 110.0f, 72.0f}};
    glass_map.walls[0].surface = recoil::Surface::Glass;
    const StepResult glass_result = Step(glass, content, commands, &glass_map);

    WorldState concrete = MakeDuel();
    concrete.players[1].position = {0.0f, 300.0f, 0.0f};
    recoil::MapDef concrete_map;
    concrete_map.wall_count = 1;
    concrete_map.walls[0] = recoil::Wall{{-50.0f, 100.0f, 0.0f}, {50.0f, 110.0f, 72.0f}};
    const StepResult concrete_result = Step(concrete, content, commands, &concrete_map);

    CHECK(plain_result.state.players[1].health < 100);
    CHECK(glass_result.state.players[1].health > plain_result.state.players[1].health);
    CHECK(concrete_result.state.players[1].health == 100);
}

TEST_CASE("材质穿透：两层玻璃后伤害继续衰减，第三层挡住") {
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FirePitch(0, 90.0f, 0.0f)};

    WorldState two = MakeDuel();
    two.players[1].position = {0.0f, 300.0f, 0.0f};
    recoil::MapDef two_map;
    two_map.wall_count = 2;
    two_map.walls[0] = recoil::Wall{{-50.0f, 100.0f, 0.0f}, {50.0f, 110.0f, 72.0f}};
    two_map.walls[0].surface = recoil::Surface::Glass;
    two_map.walls[1] = recoil::Wall{{-50.0f, 200.0f, 0.0f}, {50.0f, 210.0f, 72.0f}};
    two_map.walls[1].surface = recoil::Surface::Glass;
    const StepResult two_result = Step(two, content, commands, &two_map);

    WorldState three = MakeDuel();
    three.players[1].position = {0.0f, 300.0f, 0.0f};
    recoil::MapDef three_map = two_map;
    three_map.walls[2] = recoil::Wall{{-50.0f, 260.0f, 0.0f}, {50.0f, 270.0f, 72.0f}};
    three_map.walls[2].surface = recoil::Surface::Glass;
    three_map.wall_count = 3;
    const StepResult three_result = Step(three, content, commands, &three_map);

    CHECK(two_result.state.players[1].health > 0);
    CHECK(two_result.state.players[1].health < 90);
    CHECK(three_result.state.players[1].health == 100);
}
