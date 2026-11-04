#include <array>
#include <cstddef>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/sim/event.h"
#include "recoil/sim/step.h"
#include "recoil/world/map.h"

namespace {

using recoil::Command;
using recoil::CommandKind;
using recoil::Content;
using recoil::DefaultContent;
using recoil::EventKind;
using recoil::HashState;
using recoil::PlayerState;
using recoil::Step;
using recoil::StepResult;
using recoil::Team;
using recoil::Wall;
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
        player.team = static_cast<uint8_t>(i == 0 ? Team::T : Team::CT);
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

bool HasEvent(const recoil::EventList& events, EventKind kind) {
    for (int i = 0; i < events.count; ++i) {
        if (events.items[i].kind == kind) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("墙体挡住子弹：不造成伤害并记录 ShotMissed") {
    WorldState state = MakeDuel();
    recoil::MapDef map;
    map.wall_count = 1;
    map.walls[0] = Wall{{-64.0f, 200.0f, 0.0f}, {64.0f, 232.0f, 128.0f}};
    const std::array<Command, 1> commands{FireCommand(0)};
    const Content content = DefaultContent();

    const StepResult result = Step(state, content, commands, &map);

    CHECK(result.state.players[1].health == 100);
    CHECK(HasEvent(result.events, EventKind::ShotMissed));
}

TEST_CASE("状态哈希覆盖 C4 持有者/拆弹器与炸弹位置") {
    const WorldState base = MakeDuel();
    WorldState bomb_holder = base;
    bomb_holder.players[0].has_bomb = true;
    WorldState defuser = base;
    defuser.players[1].has_defuser = true;
    WorldState bomb_position = base;
    bomb_position.bomb.position = {12.0f, 34.0f, 6.0f};

    CHECK(HashState(base) != HashState(bomb_holder));
    CHECK(HashState(base) != HashState(defuser));
    CHECK(HashState(base) != HashState(bomb_position));
}
