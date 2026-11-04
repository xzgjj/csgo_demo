#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/sim/step.h"

namespace {

using recoil::Command;
using recoil::CommandKind;
using recoil::Content;
using recoil::DefaultContent;
using recoil::Event;
using recoil::EventKind;
using recoil::EventList;
using recoil::HashState;
using recoil::PlayerState;
using recoil::Step;
using recoil::StepResult;
using recoil::WorldState;

WorldState MakeDuel() {
    WorldState state;
    state.player_count = 2;
    for (int i = 0; i < 2; ++i) {
        PlayerState& player = state.players[static_cast<size_t>(i)];
        player.alive = true;
        player.health = 100;
        player.weapon.mag = 30;
        player.weapon.reserve = 90;
        player.position = {0.0f, static_cast<float>(i) * 500.0f, 0.0f};
    }
    return state;
}

Command FireCommand(uint8_t player, float yaw = 90.0f) {
    Command command;
    command.kind = CommandKind::Fire;
    command.player = player;
    command.yaw_degrees = yaw;
    return command;
}

Command ReloadCommand(uint8_t player) {
    Command command;
    command.kind = CommandKind::Reload;
    command.player = player;
    return command;
}

StepResult StepOne(const WorldState& state, const Content& content,
                   std::span<const Command> commands) {
    return Step(state, content, commands);
}

int CountEvents(const EventList& events, EventKind kind) {
    int count = 0;
    for (int i = 0; i < events.count; ++i) {
        if (events.items[i].kind == kind) {
            ++count;
        }
    }
    return count;
}

bool HasEvent(const EventList& events, EventKind kind) {
    return CountEvents(events, kind) > 0;
}

} // namespace

TEST_CASE("开火后弹匣恰好少一发子弹") {
    // 准备
    WorldState state = MakeDuel();
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FireCommand(0)};

    // 执行
    const StepResult result = StepOne(state, content, commands);

    // 检查
    CHECK(result.state.players[0].weapon.mag == 29);
    CHECK(result.state.players[1].health == 100 - 36);
    CHECK(CountEvents(result.events, EventKind::WeaponFired) == 1);
    CHECK(CountEvents(result.events, EventKind::DamageApplied) == 1);
}

TEST_CASE("空仓不能开火，只产生干火事件") {
    WorldState state = MakeDuel();
    state.players[0].weapon.mag = 0;
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FireCommand(0)};

    const StepResult result = StepOne(state, content, commands);

    CHECK(result.state.players[0].weapon.mag == 0);
    CHECK(result.state.players[1].health == 100);
    CHECK(HasEvent(result.events, EventKind::WeaponDryFire));
    CHECK(CountEvents(result.events, EventKind::WeaponFired) == 0);
    CHECK(CountEvents(result.events, EventKind::DamageApplied) == 0);
}

TEST_CASE("目标死亡后不再受到伤害") {
    WorldState state = MakeDuel();
    state.players[1].alive = false;
    state.players[1].health = 0;
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FireCommand(0)};

    const StepResult result = StepOne(state, content, commands);

    CHECK(result.state.players[1].health == 0);
    CHECK(result.state.players[1].alive == false);
    CHECK(CountEvents(result.events, EventKind::DamageApplied) == 0);
    CHECK(CountEvents(result.events, EventKind::PlayerDied) == 0);
}

TEST_CASE("射击冷却期间不能再次开火") {
    const Content content = DefaultContent();
    const std::array<Command, 1> fire{FireCommand(0)};
    const std::array<Command, 0> idle{};

    StepResult result = StepOne(MakeDuel(), content, fire);
    CHECK(result.state.players[0].weapon.mag == 29);

    const StepResult blocked = StepOne(result.state, content, fire);
    CHECK(blocked.state.players[0].weapon.mag == 29);
    CHECK(HasEvent(blocked.events, EventKind::CommandRejected));

    WorldState state = blocked.state;
    for (int i = 0; i < 10 && state.players[0].weapon.cooldown_ticks > 0; ++i) {
        state = StepOne(state, content, idle).state;
    }
    const StepResult allowed = StepOne(state, content, fire);
    CHECK(allowed.state.players[0].weapon.mag == 28);
    CHECK(CountEvents(allowed.events, EventKind::WeaponFired) == 1);
}

TEST_CASE("换弹完成后弹匣回满且备弹扣减") {
    WorldState state = MakeDuel();
    state.players[0].weapon.mag = 5;
    const Content content = DefaultContent();
    const std::array<Command, 1> reload{ReloadCommand(0)};
    const std::array<Command, 0> idle{};

    StepResult result = StepOne(state, content, reload);
    CHECK(result.state.players[0].weapon.reloading);
    CHECK(HasEvent(result.events, EventKind::ReloadStarted));

    for (int i = 0; i < 200 && result.state.players[0].weapon.reloading; ++i) {
        result = StepOne(result.state, content, idle);
    }

    CHECK(result.state.players[0].weapon.mag == 30);
    CHECK(result.state.players[0].weapon.reserve == 65);
    CHECK(result.state.players[0].weapon.reloading == false);
    CHECK(HasEvent(result.events, EventKind::ReloadFinished));
}

TEST_CASE("换弹过程中不能开火") {
    WorldState state = MakeDuel();
    state.players[0].weapon.mag = 8;
    const Content content = DefaultContent();
    const std::array<Command, 1> reload{ReloadCommand(0)};

    const StepResult reloading = StepOne(state, content, reload);
    const std::array<Command, 1> fire{FireCommand(0)};
    const StepResult result = StepOne(reloading.state, content, fire);

    CHECK(result.state.players[0].weapon.mag == 8);
    CHECK(HasEvent(result.events, EventKind::CommandRejected));
    CHECK(CountEvents(result.events, EventKind::WeaponFired) == 0);
}

TEST_CASE("护甲按比例吸收伤害") {
    WorldState state = MakeDuel();
    state.players[1].armor = 100;
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FireCommand(0)};

    const StepResult result = StepOne(state, content, commands);

    CHECK(result.state.players[1].health == 72);
    CHECK(result.state.players[1].armor < 100);
    CHECK(result.state.players[1].health > (100 - 36));
}

TEST_CASE("死亡玩家不能开火") {
    WorldState state = MakeDuel();
    state.players[0].alive = false;
    state.players[0].health = 0;
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FireCommand(0)};

    const StepResult result = StepOne(state, content, commands);

    CHECK(result.state.players[0].weapon.mag == 30);
    CHECK(result.state.players[1].health == 100);
    CHECK(CountEvents(result.events, EventKind::WeaponFired) == 0);
}

TEST_CASE("瞄准方向偏离时子弹不命中") {
    WorldState state = MakeDuel();
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FireCommand(0, 0.0f)};

    const StepResult result = StepOne(state, content, commands);

    CHECK(result.state.players[1].health == 100);
    CHECK(HasEvent(result.events, EventKind::ShotMissed));
    CHECK(CountEvents(result.events, EventKind::DamageApplied) == 0);
}

TEST_CASE("非法玩家编号被拒绝而不是崩溃") {
    WorldState state = MakeDuel();
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FireCommand(5)};

    const StepResult result = StepOne(state, content, commands);

    CHECK(result.state.players[0].weapon.mag == 30);
    CHECK(HasEvent(result.events, EventKind::CommandRejected));
}

TEST_CASE("状态哈希对相同状态稳定、对变化敏感") {
    const WorldState state = MakeDuel();
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FireCommand(0)};

    CHECK(HashState(state) == HashState(state));

    const StepResult fired = StepOne(state, content, commands);
    CHECK(HashState(fired.state) != HashState(state));
}

TEST_CASE("相同输入产生完全相同的结果（确定性）") {
    const WorldState state = MakeDuel();
    const Content content = DefaultContent();
    const std::array<Command, 1> commands{FireCommand(0)};

    const StepResult a = StepOne(state, content, commands);
    const StepResult b = StepOne(state, content, commands);

    CHECK(HashState(a.state) == HashState(b.state));
    CHECK(a.events.count == b.events.count);
}
