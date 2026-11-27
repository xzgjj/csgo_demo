#include <array>
#include <cmath>
#include <cstddef>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/content/weapon_table.h"
#include "recoil/sim/ballistics.h"
#include "recoil/sim/event.h"
#include "recoil/sim/step.h"

namespace {

using recoil::BallisticsFor;
using recoil::Command;
using recoil::CommandKind;
using recoil::Content;
using recoil::DefaultContent;
using recoil::EventKind;
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
    target.position = {0.0f, 8000.0f, 0.0f}; // 远到打不中，避免影响弹道状态
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

// 按 6 tick 一发（AK 冷却）连打 shots 发，返回最终状态。
WorldState FireShots(WorldState state, int shots) {
    const Content content = DefaultContent();
    for (int i = 0; i < shots; ++i) {
        const std::array<Command, 1> commands{FireCommand()};
        const StepResult fired = Step(state, content, commands);
        state = fired.state;
        for (int idle = 0; idle < 5; ++idle) {
            const StepResult skipped = Step(state, content, {});
            state = skipped.state;
        }
    }
    return state;
}

WorldState Idle(WorldState state, int ticks) {
    const Content content = DefaultContent();
    for (int i = 0; i < ticks; ++i) {
        const StepResult result = Step(state, content, {});
        state = result.state;
    }
    return state;
}

} // namespace

TEST_CASE("BAL-04 停火 0.4 s 内 inaccuracy 回到 base 10% 内") {
    WorldState state = FireShots(LiveDuel(), 10);
    const float base = BallisticsFor(WeaponId::AK47).inaccuracy.base;
    REQUIRE(state.players[0].inaccuracy > base * 3.0f); // 先确认确实累积了

    state = Idle(state, 26); // 26/64 ≈ 0.406 s
    CHECK(state.players[0].inaccuracy <= base * 0.1f);
}

TEST_CASE("BAL-04 两段恢复：initial 速率大于 final（先快后慢）") {
    const recoil::BallisticsDef& ak = BallisticsFor(WeaponId::AK47);
    CHECK(ak.inaccuracy.recovery_initial > ak.inaccuracy.recovery_final);

    // 同一 inaccuracy，分别推进 1 tick：停火延迟后的第 1 s 段 vs 0.3 s 段。
    PlayerState early;
    early.inaccuracy = 2.0f;
    early.last_fire_tick = 0;
    const float before_early = early.inaccuracy;
    recoil::TickBallistics(early, ak, 8); // 0.125 s：已在恢复窗内、initial 段
    const float early_drop = before_early - early.inaccuracy;
    PlayerState late;
    late.inaccuracy = 2.0f;
    late.last_fire_tick = 0;
    const float before_late = late.inaccuracy;
    recoil::TickBallistics(late, ak, 20); // 0.3125 s：final 段
    const float late_drop = before_late - late.inaccuracy;

    CHECK(early_drop > 0.0f);
    CHECK(late_drop > 0.0f);
    CHECK(early_drop > late_drop);
}

TEST_CASE("BAL-04 连发窗口内不恢复，射速越快扩散越累积") {
    const recoil::BallisticsDef& ak = BallisticsFor(WeaponId::AK47);
    PlayerState player;
    player.inaccuracy = 1.0f;
    player.last_fire_tick = 0;
    // 第 6 tick（射击间隔内）不恢复。
    recoil::TickBallistics(player, ak, 6);
    CHECK(player.inaccuracy == doctest::Approx(1.0f));
    // 超过恢复延迟（0.11 s ≈ 8 tick）后开始恢复。
    recoil::TickBallistics(player, ak, 9);
    CHECK(player.inaccuracy < 1.0f);
}

TEST_CASE("BAL-05 停火 1 s 后 shot_index 归零") {
    WorldState state = FireShots(LiveDuel(), 3);
    CHECK(state.players[0].shot_index == 3);
    state = Idle(state, 64); // 1 s
    CHECK(state.players[0].shot_index == 0);
}

TEST_CASE("BAL-05 复位后下一发使用 pattern[0]") {
    WorldState state = FireShots(LiveDuel(), 3);
    state = Idle(state, 64);
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FireCommand()};
    const StepResult fired = Step(state, content, commands);
    REQUIRE(fired.events.count > 0);
    recoil::Event weapon_fired{};
    bool found = false;
    for (int i = 0; i < fired.events.count; ++i) {
        if (fired.events.items[i].kind == EventKind::WeaponFired) {
            weapon_fired = fired.events.items[i];
            found = true;
        }
    }
    REQUIRE(found);
    CHECK(weapon_fired.shot_index == 0);
    // 第 1 发只有锥内随机，图案 pitch 为 0（|偏移| 远小于第 3 发的 2.7°）。
    CHECK(std::fabs(weapon_fired.f0) <= 0.5f);
    CHECK(fired.state.players[0].shot_index == 1);
}

TEST_CASE("BAL-04 点射（短连发）比长扫射恢复更快回到可接受扩散") {
    const Content content = DefaultContent();
    const float base = BallisticsFor(WeaponId::AK47).inaccuracy.base;
    WorldState burst = FireShots(LiveDuel(), 3);
    WorldState spray = FireShots(LiveDuel(), 12);
    burst = Idle(burst, 16); // 0.25 s
    spray = Idle(spray, 16);
    CHECK(burst.players[0].inaccuracy <= base);
    CHECK(spray.players[0].inaccuracy >= burst.players[0].inaccuracy);
    CHECK(content.ballistics_enabled);
}

TEST_CASE("BAL-04 关闭弹道开关时状态不被污染（回滚路径）") {
    Content content = DefaultContent();
    content.ballistics_enabled = false;
    WorldState state = LiveDuel();
    const std::array<Command, 1> commands{FireCommand()};
    const StepResult fired = Step(state, content, commands);
    CHECK(fired.state.players[0].shot_index == 0);
    CHECK(fired.state.players[0].inaccuracy == doctest::Approx(0.0f));
    CHECK(fired.state.players[0].recoil_pitch == doctest::Approx(0.0f));
}
