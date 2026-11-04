#include <cmath>

#include "doctest/doctest.h"
#include "recoil/ai/bot.h"
#include "recoil/ai/nav.h"
#include "recoil/content/content.h"
#include "recoil/world/map.h"

namespace {

using recoil::BotDecide;
using recoil::CommandKind;
using recoil::DefaultContent;
using recoil::MapDef;
using recoil::NavPath;
using recoil::PlayerState;
using recoil::Team;
using recoil::Vec3;
using recoil::Wall;
using recoil::WorldState;

constexpr float kPi = 3.14159265358979323846f;

void PlacePlayer(WorldState& state, uint8_t index, Team team, const Vec3& position) {
    PlayerState& player = state.players[index];
    player.alive = true;
    player.health = 100;
    player.team = static_cast<uint8_t>(team);
    player.position = position;
}

WorldState Duel() {
    WorldState state;
    state.player_count = 2;
    PlacePlayer(state, 0, Team::T, {0.0f, 0.0f, 0.0f});
    PlacePlayer(state, 1, Team::CT, {0.0f, 500.0f, 0.0f});
    return state;
}

void ActivateSmoke(WorldState& state, const Vec3& position) {
    state.smoke.active = true;
    state.smoke.position = position;
    state.smoke.radius = 144.0f;
    state.smoke.ticks_left = 100;
}

float YawDegrees(const Vec3& from, const Vec3& to) {
    const Vec3 d = to - from;
    return std::atan2(d.y, d.x) * (180.0f / kPi);
}

} // namespace

TEST_CASE("Bot 看到敌人时开火并朝向敌人") {
    const WorldState state = Duel();
    const auto command = BotDecide(state, DefaultContent(), 0);
    CHECK(command.kind == CommandKind::Fire);
    CHECK(command.yaw_degrees == doctest::Approx(90.0).epsilon(0.01));
}

TEST_CASE("Bot 被烟雾遮挡时不开火") {
    WorldState state = Duel();
    ActivateSmoke(state, {0.0f, 250.0f, 0.0f});
    const auto command = BotDecide(state, DefaultContent(), 0);
    CHECK(command.kind == CommandKind::Move);
    CHECK(command.move_forward > 0.0f);
}

TEST_CASE("Bot 死亡或没有敌人时停止") {
    WorldState state = Duel();
    state.players[0].alive = false;
    CHECK(BotDecide(state, DefaultContent(), 0).kind == CommandKind::Stop);
}

TEST_CASE("没有存活敌人时 Bot 停止") {
    WorldState state = Duel();
    state.players[1].alive = false;
    CHECK(BotDecide(state, DefaultContent(), 0).kind == CommandKind::Stop);
}

TEST_CASE("最近敌人被烟雾遮挡时改打可见的较远敌人") {
    WorldState state;
    state.player_count = 3;
    PlacePlayer(state, 0, Team::T, {0.0f, 0.0f, 0.0f});
    PlacePlayer(state, 1, Team::CT, {0.0f, 300.0f, 0.0f});
    PlacePlayer(state, 2, Team::CT, {0.0f, -400.0f, 0.0f});
    ActivateSmoke(state, {0.0f, 150.0f, 0.0f});

    const auto command = BotDecide(state, DefaultContent(), 0);

    CHECK(command.kind == CommandKind::Fire);
    CHECK(command.yaw_degrees == doctest::Approx(-90.0).epsilon(0.01));
}

TEST_CASE("无可见敌人时沿导航路径推进") {
    WorldState state = Duel();
    ActivateSmoke(state, {0.0f, 250.0f, 0.0f});
    MapDef map;
    map.walls[0] = Wall{{-96.0f, 128.0f, 0.0f}, {96.0f, 192.0f, 256.0f}};
    map.wall_count = 1;
    const Vec3 from = state.players[0].position;
    const Vec3 to = state.players[1].position;
    const auto path = NavPath(map, from, to);
    REQUIRE(!path.empty());

    const auto command = BotDecide(state, DefaultContent(), 0, &map);

    CHECK(command.kind == CommandKind::Move);
    CHECK(command.move_forward == doctest::Approx(1.0f));
    CHECK(command.yaw_degrees == doctest::Approx(YawDegrees(from, path.front())).epsilon(0.001));
    CHECK(std::fabs(command.yaw_degrees - 90.0f) > 1.0f);
}

TEST_CASE("导航不可达时仍朝敌人方向推进") {
    WorldState state = Duel();
    ActivateSmoke(state, {0.0f, 250.0f, 0.0f});
    MapDef map;
    map.walls[0] = Wall{{-64.0f, 448.0f, 0.0f}, {0.0f, 512.0f, 256.0f}};
    map.walls[1] = Wall{{64.0f, 448.0f, 0.0f}, {128.0f, 512.0f, 256.0f}};
    map.walls[2] = Wall{{0.0f, 384.0f, 0.0f}, {64.0f, 448.0f, 256.0f}};
    map.walls[3] = Wall{{0.0f, 512.0f, 0.0f}, {64.0f, 576.0f, 256.0f}};
    map.wall_count = 4;
    const Vec3 from = state.players[0].position;
    const Vec3 to = state.players[1].position;
    REQUIRE(NavPath(map, from, to).empty());

    const auto command = BotDecide(state, DefaultContent(), 0, &map);

    CHECK(command.kind == CommandKind::Move);
    CHECK(command.move_forward == doctest::Approx(1.0f));
    CHECK(command.yaw_degrees == doctest::Approx(90.0).epsilon(0.01));
}

TEST_CASE("Bot 被墙体遮挡时不开火而是推进") {
    const WorldState state = Duel();
    MapDef map;
    map.wall_count = 1;
    map.walls[0] = Wall{{-64.0f, 200.0f, 0.0f}, {64.0f, 232.0f, 128.0f}};

    const auto command = BotDecide(state, DefaultContent(), 0, &map);

    CHECK(command.kind == CommandKind::Move);
    CHECK(command.move_forward > 0.0f);
}
