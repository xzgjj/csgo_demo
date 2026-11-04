#include <array>
#include <cstdint>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/sim/step.h"

namespace {

using recoil::Command;
using recoil::CommandKind;
using recoil::Content;
using recoil::DefaultContent;
using recoil::EventKind;
using recoil::EventList;
using recoil::GrenadeKind;
using recoil::RoundPhase;
using recoil::Step;
using recoil::StepResult;
using recoil::Team;
using recoil::WorldState;

WorldState MakeArena() {
    WorldState state;
    state.player_count = 2;
    state.players[0].alive = true;
    state.players[0].team = static_cast<uint8_t>(Team::T);
    state.players[0].health = 100;
    state.players[0].money = 800;
    state.players[0].on_ground = true;
    state.players[0].weapon.mag = 30;
    state.players[0].weapon.reserve = 90;
    state.players[1].alive = true;
    state.players[1].team = static_cast<uint8_t>(Team::CT);
    state.players[1].health = 100;
    state.players[1].money = 800;
    state.players[1].on_ground = true;
    state.players[1].weapon.mag = 30;
    state.players[1].weapon.reserve = 90;
    state.players[1].position = {0.0f, 500.0f, 0.0f};
    state.round.phase = RoundPhase::Live;
    state.round.phase_ticks = 1000;
    return state;
}

Command Throw(GrenadeKind kind, float yaw, float pitch) {
    Command command;
    command.kind = CommandKind::Throw;
    command.player = 0;
    command.yaw_degrees = yaw;
    command.pitch_degrees = pitch;
    command.grenade = kind;
    return command;
}

Command FireAt(float yaw) {
    Command command;
    command.kind = CommandKind::Fire;
    command.player = 0;
    command.yaw_degrees = yaw;
    return command;
}

int CountEvents(const EventList& events, EventKind kind) {
    int total = 0;
    for (int i = 0; i < events.count; ++i) {
        if (events.items[i].kind == kind) {
            ++total;
        }
    }
    return total;
}

} // namespace

TEST_CASE("投掷 HE 后引信结束产生爆炸并造成伤害") {
    WorldState state = MakeArena();
    const Content content = DefaultContent();
    const std::array<Command, 1> throw_cmd{Throw(GrenadeKind::HE, 90.0f, 10.0f)};

    StepResult result = Step(state, content, throw_cmd);
    CHECK(CountEvents(result.events, EventKind::GrenadeThrown) == 1);
    bool exploded = false;
    for (int i = 0; i < 200 && !exploded; ++i) {
        result = Step(result.state, content, {});
        exploded = CountEvents(result.events, EventKind::GrenadeExploded) == 1;
    }
    CHECK(exploded);
    CHECK(result.state.players[1].health < 100);
}

TEST_CASE("HE 超出半径不造成伤害") {
    WorldState state = MakeArena();
    state.players[1].position = {0.0f, 1200.0f, 0.0f};
    const Content content = DefaultContent();
    const std::array<Command, 1> throw_cmd{Throw(GrenadeKind::HE, 90.0f, 0.0f)};

    StepResult result = Step(state, content, throw_cmd);
    for (int i = 0; i < 200; ++i) {
        result = Step(result.state, content, {});
    }

    CHECK(result.state.players[1].health == 100);
}

TEST_CASE("烟雾阻挡射线，射击无法命中") {
    WorldState state = MakeArena();
    state.smoke.active = true;
    state.smoke.position = {0.0f, 250.0f, 0.0f};
    state.smoke.radius = 144.0f;
    state.smoke.ticks_left = 600;
    const Content content = DefaultContent();
    const std::array<Command, 1> fire{FireAt(90.0f)};

    const StepResult result = Step(state, content, fire);

    CHECK(result.state.players[1].health == 100);
    CHECK(CountEvents(result.events, EventKind::ShotMissed) == 1);
}

TEST_CASE("闪光弹使范围内玩家致盲") {
    WorldState state = MakeArena();
    state.players[1].position = {0.0f, 400.0f, 0.0f};
    const Content content = DefaultContent();
    const std::array<Command, 1> throw_cmd{Throw(GrenadeKind::Flash, 90.0f, 0.0f)};

    StepResult result = Step(state, content, throw_cmd);
    for (int i = 0; i < 200 && result.state.players[1].blind_ticks == 0; ++i) {
        result = Step(result.state, content, {});
    }

    CHECK(result.state.players[1].blind_ticks > 0);
}

TEST_CASE("烟雾弹落地生成烟区并按时消散") {
    WorldState state = MakeArena();
    const Content content = DefaultContent();
    state.grenades[0].active = true;
    state.grenades[0].kind = static_cast<uint8_t>(GrenadeKind::Smoke);
    state.grenades[0].owner = 0;
    state.grenades[0].position = {0.0f, 250.0f, 0.0f};
    state.grenades[0].velocity = {};
    state.grenades[0].fuse_ticks = 1;

    StepResult result = Step(state, content, {});
    CHECK(result.state.smoke.active);
    CHECK(CountEvents(result.events, EventKind::SmokeStarted) == 1);
}

TEST_CASE("HE 伤害受护甲吸收") {
    WorldState state = MakeArena();
    state.players[1].armor = 100;
    state.players[1].position = {0.0f, 60.0f, 0.0f};
    state.grenades[0].active = true;
    state.grenades[0].kind = static_cast<uint8_t>(GrenadeKind::HE);
    state.grenades[0].owner = 0;
    state.grenades[0].position = {0.0f, 60.0f, 0.0f};
    state.grenades[0].velocity = {};
    state.grenades[0].fuse_ticks = 1;
    const Content content = DefaultContent();

    const StepResult result = Step(state, content, {});

    CHECK(result.state.players[1].health == 100 - 44);
}
