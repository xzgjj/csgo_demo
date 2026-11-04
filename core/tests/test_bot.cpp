#include "doctest/doctest.h"
#include "recoil/ai/bot.h"
#include "recoil/content/content.h"

namespace {

using recoil::BotDecide;
using recoil::CommandKind;
using recoil::DefaultContent;
using recoil::Team;
using recoil::WorldState;

WorldState Duel() {
    WorldState state;
    state.player_count = 2;
    state.players[0].alive = true;
    state.players[0].team = static_cast<uint8_t>(Team::T);
    state.players[0].health = 100;
    state.players[1].alive = true;
    state.players[1].team = static_cast<uint8_t>(Team::CT);
    state.players[1].health = 100;
    state.players[1].position = {0.0f, 500.0f, 0.0f};
    return state;
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
    state.smoke.active = true;
    state.smoke.position = {0.0f, 250.0f, 0.0f};
    state.smoke.radius = 144.0f;
    state.smoke.ticks_left = 100;
    const auto command = BotDecide(state, DefaultContent(), 0);
    CHECK(command.kind != CommandKind::Fire);
}

TEST_CASE("Bot 死亡或没有敌人时停止") {
    WorldState state = Duel();
    state.players[0].alive = false;
    CHECK(BotDecide(state, DefaultContent(), 0).kind == CommandKind::Stop);
}
