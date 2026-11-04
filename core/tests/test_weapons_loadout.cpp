#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

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

TEST_CASE("双武器槽：切枪使用副武器数值并各自保留弹匣") {
    WorldState state = Duel();
    GiveArms(state, 0, WeaponId::AK47, WeaponId::Glock);
    const Content content = DefaultContent();
    const std::array<Command, 1> fire{Fire(0)};

    const StepResult first = StepOne(state, fire, content);
    CHECK(first.state.players[1].health == 64); // AK 36
    CHECK(first.state.players[0].weapon.mag == 29);

    const std::array<Command, 1> to_sidearm{SwitchSlot(0, 1)};
    const StepResult switched = StepOne(first.state, to_sidearm, content);
    CHECK(switched.state.players[0].active_slot == 1);
    CHECK(switched.state.players[0].weapon.mag == 20);  // Glock 满弹匣
    CHECK(switched.state.players[0].sidearm.mag == 29); // AK 停火保留
    CHECK(CountEvents(switched.events, EventKind::WeaponSwitched) == 1);

    const StepResult glock_shot = StepOne(switched.state, fire, content);
    CHECK(glock_shot.state.players[1].health == 64 - 29); // Glock 30 → 距离衰减 29

    const std::array<Command, 1> to_primary{SwitchSlot(0, 0)};
    const StepResult back = StepOne(glock_shot.state, to_primary, content);
    CHECK(back.state.players[0].active_slot == 0);
    CHECK(back.state.players[0].weapon.mag == 29);
}

TEST_CASE("切枪：非法槽位被拒绝，换弹中切枪打断换弹") {
    WorldState state = Duel();
    GiveArms(state, 0, WeaponId::AK47, WeaponId::Glock);
    state.players[0].weapon.mag = 5;
    state.players[0].weapon.reloading = true;
    state.players[0].weapon.reload_ticks_left = 100;
    const std::array<Command, 1> bad{SwitchSlot(0, 7)};

    const StepResult rejected = StepOne(state, bad);
    CHECK(rejected.state.players[0].active_slot == 0);
    CHECK(rejected.state.players[0].weapon.reloading);
    CHECK(rejected.events.items[0].value ==
          static_cast<int32_t>(recoil::RejectReason::InvalidSlot));

    const std::array<Command, 1> good{SwitchSlot(0, 1)};
    const StepResult switched = StepOne(state, good);
    CHECK(switched.state.players[0].active_slot == 1);
    CHECK(switched.state.players[0].weapon.reloading == false);
    CHECK(switched.state.players[0].weapon.reload_ticks_left == 0);
}

TEST_CASE("购买武器：冻结期扣钱并进对应槽，进行中被拒绝") {
    WorldState state = Duel();
    state.players[0].money = 8000;
    state.round.phase = RoundPhase::Freeze;
    state.round.phase_ticks = 100;
    const std::array<Command, 1> buy_m4{BuyWeaponCommand(0, WeaponId::M4A4)};

    const StepResult bought = StepOne(state, buy_m4);
    CHECK(bought.state.players[0].primary_id == static_cast<uint8_t>(WeaponId::M4A4));
    CHECK(bought.state.players[0].weapon.mag == 30);
    CHECK(bought.state.players[0].weapon.reserve == 90);
    CHECK(bought.state.players[0].money == 8000 - 3100);
    CHECK(CountEvents(bought.events, EventKind::BuySucceeded) == 1);

    const std::array<Command, 1> buy_glock{BuyWeaponCommand(0, WeaponId::Glock)};
    const StepResult sidearm_bought = StepOne(bought.state, buy_glock);
    CHECK(sidearm_bought.state.players[0].secondary_id == static_cast<uint8_t>(WeaponId::Glock));
    CHECK(sidearm_bought.state.players[0].money == 8000 - 3100 - 200);

    WorldState live = state;
    live.round.phase = RoundPhase::Live;
    const StepResult rejected = StepOne(live, buy_m4);
    CHECK(rejected.state.players[0].primary_id == 0);
    CHECK(CountEvents(rejected.events, EventKind::BuyFailed) == 1);

    WorldState poor = state;
    poor.players[0].money = 100;
    const std::array<Command, 1> buy_awp{BuyWeaponCommand(0, WeaponId::AWP)};
    CHECK(CountEvents(StepOne(poor, buy_awp).events, EventKind::BuyFailed) == 1);
}

TEST_CASE("BeginMatch 给 T 发 AK-47、CT 发 M4A4（含副武器手枪）") {
    WorldState state;
    EventList events;
    recoil::BeginMatch(state, DefaultContent(), events);

    for (uint8_t i = 0; i < state.player_count; ++i) {
        const PlayerState& player = state.players[i];
        const bool is_t = player.team == static_cast<uint8_t>(Team::T);
        CHECK(player.primary_id == static_cast<uint8_t>(is_t ? WeaponId::AK47 : WeaponId::M4A4));
        CHECK(player.secondary_id == static_cast<uint8_t>(is_t ? WeaponId::Glock : WeaponId::USP));
        CHECK(player.weapon.mag == 30);
        CHECK(player.weapon.reserve == 90);
        CHECK(player.active_slot == 0);
    }
}

TEST_CASE("回合重置按队伍恢复默认武器并补满弹匣") {
    WorldState state;
    EventList events;
    recoil::BeginMatch(state, DefaultContent(), events);
    state.round.phase = RoundPhase::RoundEnd;
    state.round.phase_ticks = 1;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        state.players[i].weapon.mag = 0;
        state.players[i].weapon.reserve = 0;
        state.players[i].active_slot = 1;
    }

    const StepResult result = Step(state, DefaultContent(), {});

    CHECK(result.state.round.phase == RoundPhase::Freeze);
    for (uint8_t i = 0; i < result.state.player_count; ++i) {
        const PlayerState& player = result.state.players[i];
        const bool is_t = player.team == static_cast<uint8_t>(Team::T);
        CHECK(player.primary_id == static_cast<uint8_t>(is_t ? WeaponId::AK47 : WeaponId::M4A4));
        CHECK(player.active_slot == 0);
        CHECK(player.weapon.mag == 30);
        CHECK(player.weapon.reserve == 90);
    }
}

TEST_CASE("状态哈希覆盖武器 id、槽位与副武器状态") {
    WorldState base = Duel();
    GiveArms(base, 0, WeaponId::AK47, WeaponId::Glock);
    WorldState other_primary = base;
    other_primary.players[0].primary_id = static_cast<uint8_t>(WeaponId::M4A4);
    WorldState other_sidearm = base;
    other_sidearm.players[0].secondary_id = static_cast<uint8_t>(WeaponId::USP);
    WorldState other_slot = base;
    other_slot.players[0].active_slot = 1;
    WorldState other_mag = base;
    other_mag.players[0].sidearm.mag = 3;

    CHECK(HashState(base) == HashState(base));
    CHECK(HashState(base) != HashState(other_primary));
    CHECK(HashState(base) != HashState(other_sidearm));
    CHECK(HashState(base) != HashState(other_slot));
    CHECK(HashState(base) != HashState(other_mag));
}

TEST_CASE("WeaponId::None 回退 Content::rifle（旧状态兼容）") {
    WorldState state = Duel();
    state.players[0].weapon.mag = 30;
    state.players[0].weapon.reserve = 90;
    CHECK(state.players[0].primary_id == 0);
    const std::array<Command, 1> commands{Fire(0)};

    const StepResult result = StepOne(state, commands);

    CHECK(result.state.players[1].health == 100 - 36);
    CHECK(result.state.players[0].weapon.mag == 29);
}

TEST_CASE("没有 Fire 命令时血量不随时间变化（伤害只由命中产生）") {
    WorldState state = Duel();
    GiveArms(state, 0, WeaponId::AK47);
    GiveArms(state, 1, WeaponId::M4A4);

    const StepResult result = StepIdle(state, 100);

    CHECK(result.state.players[0].health == 100);
    CHECK(result.state.players[1].health == 100);
    CHECK(result.state.tick == state.tick + 100);
    CHECK(result.state.players[1].armor == state.players[1].armor);
}
