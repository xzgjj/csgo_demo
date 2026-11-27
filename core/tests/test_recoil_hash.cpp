#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/sim/ballistics.h"
#include "recoil/sim/event.h"
#include "recoil/sim/step.h"

namespace {

using recoil::Command;
using recoil::CommandKind;
using recoil::Content;
using recoil::DefaultContent;
using recoil::EventKind;
using recoil::HashState;
using recoil::PlayerState;
using recoil::RoundPhase;
using recoil::Step;
using recoil::StepResult;
using recoil::Team;
using recoil::WeaponId;
using recoil::WorldState;

WorldState LiveDuel() {
    WorldState state;
    state.player_count = 2;
    PlayerState& shooter = state.players[0];
    shooter.alive = true;
    shooter.on_ground = true;
    shooter.health = 100;
    shooter.team = static_cast<uint8_t>(Team::T);
    shooter.primary_id = static_cast<uint8_t>(WeaponId::AK47);
    shooter.weapon.mag = 30;
    shooter.weapon.reserve = 90;
    PlayerState& target = state.players[1];
    target.alive = true;
    target.on_ground = true;
    target.health = 100;
    target.team = static_cast<uint8_t>(Team::CT);
    target.position = {0.0f, 6000.0f, 0.0f};
    state.round.phase = RoundPhase::Live;
    state.round.phase_ticks = 4000;
    return state;
}

Command FireCommand() {
    Command command;
    command.kind = CommandKind::Fire;
    command.player = 0;
    command.yaw_degrees = 90.0f;
    command.pitch_degrees = 0.0f;
    return command;
}

// 固定命令序列：每 6 tick 一发；返回最终状态（含空闲 tick 的恢复）。
WorldState RunPattern(int ticks) {
    const Content content = DefaultContent();
    WorldState state = LiveDuel();
    for (int tick = 0; tick < ticks; ++tick) {
        const std::array<Command, 1> commands{FireCommand()};
        const StepResult result =
            (tick % 6 == 0) ? Step(state, content, commands) : Step(state, content, {});
        state = result.state;
    }
    return state;
}

} // namespace

TEST_CASE("BAL-07 同种子 1000 tick 两轮回放哈希一致") {
    const WorldState first = RunPattern(1000);
    const WorldState second = RunPattern(1000);
    CHECK(HashState(first) == HashState(second));
    CHECK(first.tick == second.tick);
    CHECK(first.rng == second.rng);
}

TEST_CASE("BAL-07 弹道状态进入哈希：改 shot_index/recoil/inaccuracy 都会变哈希") {
    const WorldState base = LiveDuel();
    WorldState shot = base;
    shot.players[0].shot_index = 5;
    WorldState pitch = base;
    pitch.players[0].recoil_pitch = 2.5f;
    WorldState yaw = base;
    yaw.players[0].recoil_yaw = -1.25f;
    WorldState inaccuracy = base;
    inaccuracy.players[0].inaccuracy = 0.5f;

    CHECK(HashState(base) != HashState(shot));
    CHECK(HashState(base) != HashState(pitch));
    CHECK(HashState(base) != HashState(yaw));
    CHECK(HashState(base) != HashState(inaccuracy));
}

TEST_CASE("WeaponFired 事件携带图案偏移/扩散/连发序号") {
    const Content content = DefaultContent();
    WorldState state = LiveDuel();
    const std::array<Command, 1> commands{FireCommand()};
    const StepResult first = Step(state, content, commands);
    recoil::Event fired{};
    bool found = false;
    for (int i = 0; i < first.events.count; ++i) {
        if (first.events.items[i].kind == EventKind::WeaponFired) {
            fired = first.events.items[i];
            found = true;
        }
    }
    REQUIRE(found);
    CHECK(fired.aux == static_cast<uint8_t>(WeaponId::AK47));
    CHECK(fired.shot_index == 0);
    CHECK(fired.f2 ==
          doctest::Approx(recoil::BallisticsFor(WeaponId::AK47).inaccuracy.base).epsilon(0.01f));

    // 第二发（等冷却结束）应报告 shot_index=1，且 inaccuracy 已累加。
    WorldState next = first.state;
    for (int i = 0; i < 6; ++i) {
        next = Step(next, content, {}).state;
    }
    const StepResult second = Step(next, content, commands);
    recoil::Event second_fired{};
    found = false;
    for (int i = 0; i < second.events.count; ++i) {
        if (second.events.items[i].kind == EventKind::WeaponFired) {
            second_fired = second.events.items[i];
            found = true;
        }
    }
    REQUIRE(found);
    CHECK(second_fired.shot_index == 1);
    CHECK(second_fired.f2 > fired.f2);
    CHECK(std::fabs(second_fired.f0) > 0.5f); // 第 2 发已明显抬枪
}

TEST_CASE("同命令不同种子会产生不同弹着（随机只影响 diffusion）") {
    const Content content = DefaultContent();
    WorldState a = LiveDuel();
    WorldState b = LiveDuel();
    a.rng = 1ull;
    b.rng = 2ull;
    const std::array<Command, 1> commands{FireCommand()};
    const StepResult first = Step(a, content, commands);
    const StepResult second = Step(b, content, commands);
    recoil::Event first_fired{};
    recoil::Event second_fired{};
    for (int i = 0; i < first.events.count; ++i) {
        if (first.events.items[i].kind == EventKind::WeaponFired) {
            first_fired = first.events.items[i];
        }
    }
    for (int i = 0; i < second.events.count; ++i) {
        if (second.events.items[i].kind == EventKind::WeaponFired) {
            second_fired = second.events.items[i];
        }
    }
    CHECK(first.state.players[0].recoil_pitch ==
          doctest::Approx(second.state.players[0].recoil_pitch));
    // 图案固定，随机只改锥内偏量：两者事件 f0 不应完全相等。
    CHECK(first_fired.f0 != doctest::Approx(second_fired.f0).epsilon(0.0001f));
}
