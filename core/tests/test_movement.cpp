#include <array>
#include <cmath>
#include <cstdint>
#include <span>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/sim/step.h"
#include "recoil/world/map.h"

namespace {

using recoil::Command;
using recoil::CommandKind;
using recoil::Content;
using recoil::DefaultContent;
using recoil::HashState;
using recoil::MapDef;
using recoil::PlayerState;
using recoil::Step;
using recoil::StepResult;
using recoil::Wall;
using recoil::WorldState;

WorldState MakeMover() {
    WorldState state;
    state.player_count = 1;
    PlayerState& player = state.players[0];
    player.alive = true;
    player.health = 100;
    player.on_ground = true;
    player.weapon.mag = 30;
    player.weapon.reserve = 90;
    player.position = {0.0f, 0.0f, 0.0f};
    return state;
}

Command MoveCommand(float forward, float yaw = 0.0f, bool jump = false, bool duck = false) {
    Command command;
    command.kind = CommandKind::Move;
    command.player = 0;
    command.yaw_degrees = yaw;
    command.move_forward = forward;
    command.jump = jump;
    command.duck = duck;
    return command;
}

StepResult StepOnce(const WorldState& state, const Content& content, const Command& command,
                    const MapDef* map) {
    const std::array<Command, 1> commands{command};
    return Step(state, content, commands, map);
}

float HorizontalSpeed(const PlayerState& player) {
    return std::sqrt(player.velocity.x * player.velocity.x + player.velocity.y * player.velocity.y);
}

MapDef WallMap() {
    MapDef map;
    map.wall_count = 1;
    map.walls[0] = Wall{{100.0f, -50.0f, 0.0f}, {132.0f, 50.0f, 72.0f}};
    return map;
}

} // namespace

TEST_CASE("地面加速最终接近最大速度") {
    const Content content = DefaultContent();
    const MapDef* map = nullptr;
    WorldState state = MakeMover();
    const Command move = MoveCommand(1.0f);

    for (int i = 0; i < 128; ++i) {
        state = StepOnce(state, content, move, map).state;
    }

    CHECK(HorizontalSpeed(state.players[0]) > 249.0f);
    CHECK(HorizontalSpeed(state.players[0]) < 251.0f);
}

TEST_CASE("松开方向键后摩擦使玩家停下") {
    const Content content = DefaultContent();
    WorldState state = MakeMover();
    state.players[0].velocity.x = 250.0f;
    const Command stop_move = MoveCommand(0.0f);

    for (int i = 0; i < 200; ++i) {
        state = StepOnce(state, content, stop_move, nullptr).state;
    }

    CHECK(HorizontalSpeed(state.players[0]) < 1.0f);
}

TEST_CASE("跳跃离开地面并最终落地") {
    const Content content = DefaultContent();
    WorldState state = MakeMover();
    const Command jump = MoveCommand(0.0f, 0.0f, true);

    const StepResult jumped = StepOnce(state, content, jump, nullptr);
    CHECK(jumped.state.players[0].velocity.z > 300.0f);
    CHECK(jumped.state.players[0].position.z > 0.0f);
    CHECK(jumped.state.players[0].on_ground == false);

    WorldState falling = jumped.state;
    int ticks = 0;
    while (!falling.players[0].on_ground && ticks < 200) {
        falling = StepOnce(falling, content, MoveCommand(0.0f), nullptr).state;
        ++ticks;
    }
    CHECK(ticks < 200);
    CHECK(falling.players[0].position.z == doctest::Approx(0.0f).epsilon(0.001));
}

TEST_CASE("撞墙后停在墙前且不穿墙") {
    const Content content = DefaultContent();
    const MapDef map = WallMap();
    WorldState state = MakeMover();

    for (int i = 0; i < 128; ++i) {
        state = StepOnce(state, content, MoveCommand(1.0f), &map).state;
    }

    CHECK(state.players[0].position.x <= 84.5f);
    CHECK(state.players[0].position.x >= 83.0f);
    CHECK(state.players[0].velocity.x == doctest::Approx(0.0f).epsilon(0.001));
}

TEST_CASE("高速移动通过子步进避免穿墙") {
    const Content content = DefaultContent();
    const MapDef map = WallMap();
    WorldState state = MakeMover();
    state.players[0].velocity.x = 3000.0f;
    const Command no_move = MoveCommand(0.0f);

    const StepResult result = StepOnce(state, content, no_move, &map);

    CHECK(result.state.players[0].position.x + 16.0f <= 100.5f);
}

TEST_CASE("下蹲时速度被限制在蹲速") {
    const Content content = DefaultContent();
    WorldState state = MakeMover();
    const Command duck_move = MoveCommand(1.0f, 0.0f, false, true);

    for (int i = 0; i < 128; ++i) {
        state = StepOnce(state, content, duck_move, nullptr).state;
    }

    CHECK(HorizontalSpeed(state.players[0]) <= 86.0f);
    CHECK(HorizontalSpeed(state.players[0]) > 84.0f);
}

TEST_CASE("移动会改变状态哈希") {
    const Content content = DefaultContent();
    const WorldState state = MakeMover();
    const StepResult moved = StepOnce(state, content, MoveCommand(1.0f), nullptr);

    CHECK(HashState(moved.state) != HashState(state));
}

TEST_CASE("低矮空间下不能站起") {
    // 准备：天花板下沿 55u，站立高度 72u 会碰撞，蹲高 54u 可通过。
    MapDef map;
    map.wall_count = 1;
    map.walls[0] = Wall{{-100.0f, -100.0f, 55.0f}, {100.0f, 100.0f, 200.0f}};
    WorldState state = MakeMover();
    state.players[0].ducking = true;
    const Content content = DefaultContent();
    const Command stand = MoveCommand(0.0f, 0.0f, false, false);

    // 执行
    const StepResult result = StepOnce(state, content, stand, &map);

    // 检查：仍保持蹲下，不能站起
    CHECK(result.state.players[0].ducking == true);
}

TEST_CASE("开阔空间可以站起") {
    WorldState state = MakeMover();
    state.players[0].ducking = true;
    const Content content = DefaultContent();
    const Command stand = MoveCommand(0.0f, 0.0f, false, false);

    const StepResult result = StepOnce(state, content, stand, nullptr);

    CHECK(result.state.players[0].ducking == false);
}

TEST_CASE("状态哈希覆盖移动意图与跳跃队列") {
    const WorldState base = MakeMover();
    WorldState with_wish = base;
    with_wish.players[0].wish_forward = 1.0f;
    WorldState with_jump = base;
    with_jump.players[0].jump_queued = true;

    CHECK(HashState(base) != HashState(with_wish));
    CHECK(HashState(base) != HashState(with_jump));
}

TEST_CASE("32u 台阶自动 step-up 上台阶") {
    const Content content = DefaultContent();
    MapDef map;
    map.wall_count = 1;
    map.walls[0] = Wall{{40.0f, -96.0f, 0.0f}, {400.0f, 96.0f, 32.0f}};
    WorldState state = MakeMover();
    state.players[0].position = {-20.0f, 0.0f, 0.0f};

    for (int i = 0; i < 64; ++i) {
        state = StepOnce(state, content, MoveCommand(1.0f), &map).state;
    }

    CHECK(state.players[0].position.z == doctest::Approx(32.0f).epsilon(0.001));
    CHECK(state.players[0].position.x > 40.0f);
}

TEST_CASE("超过 step_max 的高台不能直接 step-up") {
    const Content content = DefaultContent();
    MapDef map;
    map.wall_count = 1;
    map.walls[0] = Wall{{40.0f, -96.0f, 0.0f}, {400.0f, 96.0f, 64.0f}};
    WorldState state = MakeMover();
    state.players[0].position = {-20.0f, 0.0f, 0.0f};

    for (int i = 0; i < 64; ++i) {
        state = StepOnce(state, content, MoveCommand(1.0f), &map).state;
    }

    CHECK(state.players[0].position.z == doctest::Approx(0.0f).epsilon(0.001));
    CHECK(state.players[0].position.x <= 24.5f);
}

TEST_CASE("从空中落到平台顶面并落地") {
    const Content content = DefaultContent();
    MapDef map;
    map.wall_count = 1;
    map.walls[0] = Wall{{0.0f, -96.0f, 0.0f}, {200.0f, 96.0f, 64.0f}};
    WorldState state = MakeMover();
    state.players[0].position = {60.0f, 0.0f, 120.0f};
    state.players[0].on_ground = false;

    for (int i = 0; i < 80; ++i) {
        state = StepOnce(state, content, MoveCommand(0.0f), &map).state;
    }

    CHECK(state.players[0].position.z == doctest::Approx(64.0f).epsilon(0.001));
    CHECK(state.players[0].on_ground == true);
}

TEST_CASE("走出平台边缘后会坠落回地面") {
    const Content content = DefaultContent();
    MapDef map;
    map.wall_count = 1;
    map.walls[0] = Wall{{-100.0f, -100.0f, 0.0f}, {100.0f, 100.0f, 64.0f}};
    WorldState state = MakeMover();
    state.players[0].position = {0.0f, 0.0f, 64.0f};

    for (int i = 0; i < 200; ++i) {
        state = StepOnce(state, content, MoveCommand(1.0f), &map).state;
    }

    CHECK(state.players[0].position.x > 116.0f);
    CHECK(state.players[0].position.z == doctest::Approx(0.0f).epsilon(0.001));
}
