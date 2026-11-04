#include <array>
#include <cstddef>
#include <cstdint>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/sim/command.h"
#include "recoil/sim/event.h"
#include "recoil/sim/step.h"

namespace {

using recoil::BuyItem;
using recoil::Command;
using recoil::CommandKind;
using recoil::Content;
using recoil::DefaultContent;
using recoil::HashState;
using recoil::PlayerState;
using recoil::RoundPhase;
using recoil::Step;
using recoil::StepResult;
using recoil::Team;
using recoil::WorldState;

WorldState Duel() {
    WorldState state;
    state.player_count = 2;
    for (int i = 0; i < 2; ++i) {
        PlayerState& player = state.players[static_cast<size_t>(i)];
        player.alive = true;
        player.health = 100;
        player.on_ground = true;
        player.team = static_cast<uint8_t>(i == 0 ? Team::T : Team::CT);
        player.position = {0.0f, static_cast<float>(i) * 500.0f, 0.0f};
        player.weapon.mag = 30;
        player.weapon.reserve = 90;
    }
    state.round.phase = RoundPhase::Live;
    state.round.phase_ticks = 1000;
    return state;
}

Command Fire(float yaw = 90.0f) {
    Command command;
    command.kind = CommandKind::Fire;
    command.player = 0;
    command.yaw_degrees = yaw;
    return command;
}

Command Move(float yaw) {
    Command command;
    command.kind = CommandKind::Move;
    command.player = 0;
    command.yaw_degrees = yaw;
    command.move_forward = 1.0f;
    return command;
}

Command Buy(BuyItem item, uint8_t player = 0) {
    Command command;
    command.kind = CommandKind::Buy;
    command.player = player;
    command.item = item;
    return command;
}

StepResult StepOne(const WorldState& state, const Command& command,
                   const Content& content = DefaultContent()) {
    const std::array<Command, 1> commands{command};
    return Step(state, content, commands);
}

} // namespace

TEST_CASE("Fire 命令累计开火数并记录最后开火 tick") {
    const Content content = DefaultContent();
    const StepResult first = StepOne(Duel(), Fire(), content);

    CHECK(first.state.players[0].fire_count == 1);
    CHECK(first.state.players[0].last_fire_tick == static_cast<uint32_t>(first.state.tick));

    WorldState state = first.state;
    const std::array<Command, 0> idle{};
    for (int i = 0; i < 10; ++i) {
        state = Step(state, content, idle).state;
    }
    const StepResult second = StepOne(state, Fire(), content);

    CHECK(second.state.players[0].fire_count == 2);
    CHECK(second.state.players[0].last_fire_tick == static_cast<uint32_t>(second.state.tick));
}

TEST_CASE("未开火时开火计数与最后开火 tick 保持为 0") {
    WorldState state = Duel();
    const std::array<Command, 0> idle{};
    for (int i = 0; i < 20; ++i) {
        state = Step(state, DefaultContent(), idle).state;
    }

    CHECK(state.players[0].fire_count == 0);
    CHECK(state.players[0].last_fire_tick == 0);
}

TEST_CASE("开火与移动命令更新 view_yaw，Stop 不改变朝向") {
    const Content content = DefaultContent();
    const StepResult moved = StepOne(Duel(), Move(45.0f), content);
    CHECK(moved.state.players[0].view_yaw == doctest::Approx(45.0f));

    WorldState state = moved.state;
    const StepResult fired = StepOne(state, Fire(90.0f), content);
    CHECK(fired.state.players[0].view_yaw == doctest::Approx(90.0f));

    Command stop;
    stop.kind = CommandKind::Stop;
    stop.player = 0;
    const StepResult stopped = StepOne(fired.state, stop, content);
    CHECK(stopped.state.players[0].view_yaw == doctest::Approx(90.0f));
}

TEST_CASE("状态哈希覆盖 view_yaw 与开火计数") {
    WorldState base = Duel();
    WorldState yaw = base;
    yaw.players[0].view_yaw = 33.0f;
    WorldState count = base;
    count.players[0].fire_count = 3;
    WorldState last_tick = base;
    last_tick.players[0].last_fire_tick = 12345;

    CHECK(HashState(base) == HashState(base));
    CHECK(HashState(base) != HashState(yaw));
    CHECK(HashState(base) != HashState(count));
    CHECK(HashState(base) != HashState(last_tick));
}

TEST_CASE("护甲已满但未戴盔时仍可购买头盔（扣 1000）") {
    WorldState state = Duel();
    state.round.phase = RoundPhase::Freeze;
    state.round.phase_ticks = 100;
    state.players[0].money = 2000;
    state.players[0].armor = 100;
    state.players[0].helmet = false;

    const StepResult result = StepOne(state, Buy(BuyItem::Helmet));

    CHECK(result.state.players[0].helmet);
    CHECK(result.state.players[0].armor == 100);
    CHECK(result.state.players[0].money == 1000);
    CHECK(result.state.players[0].fire_count == 0);
}

TEST_CASE("未开火时玩家死亡不记录最后开火 tick") {
    WorldState state = Duel();
    state.players[0].health = 10; // 濒死：被对方一枪击杀
    state.players[1].position = state.players[0].position;
    Command fire = Fire(90.0f);
    fire.player = 1;
    fire.yaw_degrees = 270.0f;

    const StepResult result = StepOne(state, fire);

    CHECK(result.state.players[1].fire_count == 1);
    CHECK(result.state.players[1].last_fire_tick == static_cast<uint32_t>(result.state.tick));
    CHECK(result.state.players[0].fire_count == 0);
    CHECK(result.state.players[0].last_fire_tick == 0);
}
