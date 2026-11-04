#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/content/weapon_table.h"
#include "recoil/sim/round.h"
#include "recoil/sim/step.h"

namespace {

using recoil::Command;
using recoil::CommandKind;
using recoil::Content;
using recoil::DefaultContent;
using recoil::EventKind;
using recoil::EventList;
using recoil::FindWeaponRow;
using recoil::HashState;
using recoil::PlayerState;
using recoil::RoundPhase;
using recoil::Step;
using recoil::StepResult;
using recoil::Team;
using recoil::WeaponId;
using recoil::WeaponRow;
using recoil::WorldState;

const WeaponRow& Row(WeaponId id) {
    const WeaponRow* row = FindWeaponRow(id);
    return *row;
}

// 玩家索引 0 = T、索引 1 = CT；distance 为两人水平距离（沿 +Y）。
WorldState Duel(uint16_t target_health = 100, float distance = 100.0f) {
    WorldState state;
    state.player_count = 2;
    PlayerState& shooter = state.players[0];
    shooter.alive = true;
    shooter.health = 100;
    shooter.team = static_cast<uint8_t>(Team::T);
    shooter.on_ground = true;
    shooter.position = {0.0f, 0.0f, 0.0f};
    PlayerState& target = state.players[1];
    target.alive = true;
    target.health = target_health;
    target.team = static_cast<uint8_t>(Team::CT);
    target.on_ground = true;
    target.position = {0.0f, distance, 0.0f};
    state.round.phase = RoundPhase::Live;
    state.round.phase_ticks = 1000;
    return state;
}

// 把武器装进主武器槽并补满弹匣/备弹；再补一把副武器（可选）。
void GiveArms(WorldState& state, uint8_t player, WeaponId primary,
              WeaponId sidearm = WeaponId::None) {
    PlayerState& owner = state.players[player];
    const WeaponRow& main = Row(primary);
    owner.primary_id = static_cast<uint8_t>(primary);
    owner.weapon.mag = main.def.magazine;
    owner.weapon.reserve = main.def.reserve;
    if (sidearm == WeaponId::None) {
        owner.active_slot = 0;
        return;
    }
    const WeaponRow& side = Row(sidearm);
    owner.secondary_id = static_cast<uint8_t>(sidearm);
    owner.sidearm.mag = side.def.magazine;
    owner.sidearm.reserve = side.def.reserve;
    owner.active_slot = 0;
}

Command Fire(uint8_t player, float yaw = 90.0f, float pitch = 0.0f) {
    Command command;
    command.kind = CommandKind::Fire;
    command.player = player;
    command.yaw_degrees = yaw;
    command.pitch_degrees = pitch;
    return command;
}

Command ReloadCommand(uint8_t player) {
    Command command;
    command.kind = CommandKind::Reload;
    command.player = player;
    return command;
}

Command SwitchSlot(uint8_t player, uint8_t slot) {
    Command command;
    command.kind = CommandKind::SwitchWeapon;
    command.player = player;
    command.slot = slot;
    return command;
}

Command BuyWeaponCommand(uint8_t player, WeaponId weapon) {
    Command command;
    command.kind = CommandKind::BuyWeapon;
    command.player = player;
    command.weapon_id = weapon;
    return command;
}

StepResult StepOne(const WorldState& state, const std::array<Command, 1>& commands,
                   const Content& content = DefaultContent()) {
    return Step(state, content, commands);
}

StepResult StepIdle(const WorldState& state, int ticks, const Content& content = DefaultContent()) {
    WorldState current = state;
    StepResult result;
    for (int i = 0; i < ticks; ++i) {
        result = Step(current, content, {});
        current = result.state;
    }
    result.state = current;
    return result;
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

} // namespace

TEST_CASE("武器表：AK-47/M4A4/AWP 伤害与弹匣对齐 CS:GO") {
    CHECK(Row(WeaponId::AK47).def.damage == 36);
    CHECK(Row(WeaponId::M4A4).def.damage == 33);
    CHECK(Row(WeaponId::AWP).def.damage == 115);
    CHECK(Row(WeaponId::AK47).def.magazine == 30);
    CHECK(Row(WeaponId::M4A4).def.magazine == 30);
    CHECK(Row(WeaponId::AWP).def.magazine == 10);
    CHECK(Row(WeaponId::AWP).def.reserve == 30);
    CHECK(Row(WeaponId::AK47).def.reserve == 90);
    CHECK(Row(WeaponId::AK47).price == 2700);
    CHECK(Row(WeaponId::M4A4).price == 3100);
    CHECK(Row(WeaponId::AWP).price == 4750);
}

TEST_CASE("武器表：护甲穿透/距离衰减/射速/换弹/倍镜按武器") {
    CHECK(Row(WeaponId::AK47).def.armor_ratio == doctest::Approx(0.775f));
    CHECK(Row(WeaponId::M4A4).def.armor_ratio == doctest::Approx(0.70f));
    CHECK(Row(WeaponId::AWP).def.armor_ratio == doctest::Approx(0.975f));
    CHECK(Row(WeaponId::AK47).def.range_modifier == doctest::Approx(0.98f));
    CHECK(Row(WeaponId::M4A4).def.range_modifier == doctest::Approx(0.97f));
    CHECK(Row(WeaponId::AWP).def.range_modifier == doctest::Approx(0.99f));
    CHECK(Row(WeaponId::AK47).def.fire_interval_ticks == 6);
    CHECK(Row(WeaponId::AWP).def.fire_interval_ticks == 93);
    CHECK(Row(WeaponId::AK47).def.reload_ticks == 160);
    CHECK(Row(WeaponId::AWP).def.reload_ticks == 237);
    CHECK(Row(WeaponId::AWP).def.scope_magnification == doctest::Approx(4.0f));
    CHECK(Row(WeaponId::AK47).def.scope_magnification == doctest::Approx(1.0f));
    CHECK(recoil::WeaponName(WeaponId::AWP) == std::string("AWP"));
    CHECK(recoil::WeaponPrice(WeaponId::USP) == 200);
}

TEST_CASE("胸部位命中：36/33/115 按当前武器取基础伤害") {
    const Content content = DefaultContent();
    for (const WeaponId id : {WeaponId::AK47, WeaponId::M4A4, WeaponId::AWP}) {
        WorldState state = Duel();
        GiveArms(state, 0, id);
        const std::array<Command, 1> commands{Fire(0)};
        const StepResult result = StepOne(state, commands, content);
        const uint16_t expected = static_cast<uint16_t>(std::min<int>(100, Row(id).def.damage));
        CHECK(result.state.players[1].health == 100 - expected);
        CHECK(result.state.players[0].weapon.mag == Row(id).def.magazine - 1);
    }
}

TEST_CASE("头部命中 4 倍：AWP 一枪致死，AK 无盔也致死") {
    const Content content = DefaultContent();
    for (const WeaponId id : {WeaponId::AK47, WeaponId::M4A4, WeaponId::AWP}) {
        WorldState state = Duel(100, 300.0f);
        GiveArms(state, 0, id);
        const std::array<Command, 1> commands{Fire(0, 90.0f, 0.8f)};
        const StepResult result = StepOne(state, commands, content);
        CHECK(result.state.players[1].health == 0);
        CHECK(result.state.players[1].alive == false);
    }
}

TEST_CASE("戴盔后头部按 2 倍：AK 一枪不死") {
    WorldState state = Duel(100, 300.0f);
    GiveArms(state, 0, WeaponId::AK47);
    state.players[1].helmet = true;
    const std::array<Command, 1> commands{Fire(0, 90.0f, 0.8f)};

    const StepResult result = StepOne(state, commands);

    CHECK(result.state.players[1].health == 29);
    CHECK(result.state.players[1].alive);
}

TEST_CASE("腿部命中 0.75 倍：AK 73 / M4 76 / AWP 14 剩余") {
    const Content content = DefaultContent();
    WorldState ak = Duel(100, 300.0f);
    GiveArms(ak, 0, WeaponId::AK47);
    const std::array<Command, 1> leg{Fire(0, 90.0f, -8.0f)};
    CHECK(StepOne(ak, leg, content).state.players[1].health == 73);

    WorldState m4 = Duel(100, 300.0f);
    GiveArms(m4, 0, WeaponId::M4A4);
    CHECK(StepOne(m4, leg, content).state.players[1].health == 76);

    WorldState awp = Duel(100, 300.0f);
    GiveArms(awp, 0, WeaponId::AWP);
    CHECK(StepOne(awp, leg, content).state.players[1].health == 14);
}

TEST_CASE("护甲按武器 armor_ratio 减伤：AK 28 / M4 23 / AWP 112") {
    const Content content = DefaultContent();
    WorldState ak = Duel();
    GiveArms(ak, 0, WeaponId::AK47);
    ak.players[1].armor = 100;
    const std::array<Command, 1> body{Fire(0)};
    CHECK(StepOne(ak, body, content).state.players[1].health == 72);

    WorldState m4 = Duel();
    GiveArms(m4, 0, WeaponId::M4A4);
    m4.players[1].armor = 100;
    CHECK(StepOne(m4, body, content).state.players[1].health == 77);

    WorldState awp = Duel(200);
    GiveArms(awp, 0, WeaponId::AWP);
    awp.players[1].armor = 100;
    CHECK(StepOne(awp, body, content).state.players[1].health == 88);
}

TEST_CASE("距离衰减按武器：1000u 处 AK 35 / M4 31 / AWP 113") {
    const Content content = DefaultContent();
    WorldState ak = Duel(100, 1000.0f);
    GiveArms(ak, 0, WeaponId::AK47);
    const std::array<Command, 1> body{Fire(0)};
    CHECK(StepOne(ak, body, content).state.players[1].health == 65);

    WorldState m4 = Duel(100, 1000.0f);
    GiveArms(m4, 0, WeaponId::M4A4);
    CHECK(StepOne(m4, body, content).state.players[1].health == 69);

    WorldState awp = Duel(200, 1000.0f);
    GiveArms(awp, 0, WeaponId::AWP);
    CHECK(StepOne(awp, body, content).state.players[1].health == 87);
}

TEST_CASE("射击冷却按武器取 fire_interval_ticks") {
    const Content content = DefaultContent();
    WorldState ak = Duel();
    GiveArms(ak, 0, WeaponId::AK47);
    const std::array<Command, 1> fire{Fire(0)};
    CHECK(StepOne(ak, fire, content).state.players[0].weapon.cooldown_ticks == 6);

    WorldState awp = Duel();
    GiveArms(awp, 0, WeaponId::AWP);
    CHECK(StepOne(awp, fire, content).state.players[0].weapon.cooldown_ticks == 93);
    const StepResult fired = StepOne(awp, fire, content); // 首发：10 → 9，冷却 93
    CHECK(fired.state.players[0].weapon.mag == 9);
    // 冷却剩 2 tick 时仍被拒绝（弹匣不变）；再过 1 tick 后 Step 内先减到 0 即可开火。
    const StepResult at_91 = StepIdle(fired.state, 91, content);
    CHECK(at_91.state.players[0].weapon.cooldown_ticks == 2);
    CHECK(StepOne(at_91.state, fire, content).state.players[0].weapon.mag == 9);
    const StepResult at_92 = StepIdle(fired.state, 92, content);
    CHECK(at_92.state.players[0].weapon.cooldown_ticks == 1);
    CHECK(StepOne(at_92.state, fire, content).state.players[0].weapon.mag == 8);
}

TEST_CASE("换弹时长与弹匣容量按武器：AWP 237 tick 且补到 10 发") {
    WorldState state = Duel();
    GiveArms(state, 0, WeaponId::AWP);
    state.players[0].weapon.mag = 9;
    const std::array<Command, 1> reload{ReloadCommand(0)};

    StepResult result = StepOne(state, reload);
    CHECK(result.state.players[0].weapon.reloading);
    CHECK(result.state.players[0].weapon.reload_ticks_left == 237);

    result = StepIdle(result.state, 237);
    CHECK(result.state.players[0].weapon.reloading == false);
    CHECK(result.state.players[0].weapon.mag == 10);
    CHECK(result.state.players[0].weapon.reserve == 29);
}
