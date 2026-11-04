#include <array>
#include <cstddef>
#include <cstdint>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/content/weapon_table.h"
#include "recoil/sim/event.h"
#include "recoil/sim/round.h"
#include "recoil/sim/step.h"

namespace {

using recoil::Command;
using recoil::CommandKind;
using recoil::Content;
using recoil::DefaultContent;
using recoil::EventKind;
using recoil::FindWeaponRow;
using recoil::GroundWeapon;
using recoil::HashState;
using recoil::PlayerState;
using recoil::RoundPhase;
using recoil::Step;
using recoil::StepResult;
using recoil::Team;
using recoil::Vec3;
using recoil::WeaponId;
using recoil::WorldState;

WorldState Duel() {
    WorldState state;
    state.player_count = 2;
    PlayerState& shooter = state.players[0];
    shooter.alive = true;
    shooter.health = 100;
    shooter.team = static_cast<uint8_t>(Team::T);
    shooter.on_ground = true;
    shooter.position = {0.0f, 0.0f, 0.0f};
    PlayerState& victim = state.players[1];
    victim.alive = true;
    victim.health = 100;
    victim.team = static_cast<uint8_t>(Team::CT);
    victim.on_ground = true;
    victim.position = {0.0f, 100.0f, 0.0f};
    state.round.phase = RoundPhase::Live;
    state.round.phase_ticks = 1000;
    return state;
}

void GiveArms(WorldState& state, uint8_t player, WeaponId id, uint16_t mag = 0,
              uint16_t reserve = 0) {
    const recoil::WeaponRow* row = FindWeaponRow(id);
    PlayerState& owner = state.players[player];
    owner.primary_id = static_cast<uint8_t>(id);
    owner.weapon.mag = mag != 0 ? mag : row->def.magazine;
    owner.weapon.reserve = reserve != 0 ? reserve : row->def.reserve;
    owner.weapon.reloading = false;
    owner.weapon.reload_ticks_left = 0;
    owner.weapon.cooldown_ticks = 0;
    owner.active_slot = 0;
}

Command Fire() {
    Command command;
    command.kind = CommandKind::Fire;
    command.player = 0;
    command.yaw_degrees = 90.0f;
    return command;
}

Command Pickup(uint8_t player = 0) {
    Command command;
    command.kind = CommandKind::Pickup;
    command.player = player;
    return command;
}

StepResult StepOne(const WorldState& state, const Command& command,
                   const Content& content = DefaultContent()) {
    const std::array<Command, 1> commands{command};
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

int CountEvents(const recoil::EventList& events, EventKind kind) {
    int count = 0;
    for (int i = 0; i < events.count; ++i) {
        if (events.items[i].kind == kind) {
            ++count;
        }
    }
    return count;
}

int FirstEventValue(const recoil::EventList& events, EventKind kind) {
    for (int i = 0; i < events.count; ++i) {
        if (events.items[i].kind == kind) {
            return events.items[i].value;
        }
    }
    return -999;
}

const recoil::WeaponRow& Row(WeaponId id) {
    return *FindWeaponRow(id);
}

void PutGroundDrop(WorldState& state, int slot, WeaponId id, const Vec3& position, uint16_t mag) {
    GroundWeapon& drop = state.ground_weapons[static_cast<size_t>(slot)];
    drop.active = true;
    drop.weapon_id = static_cast<uint8_t>(id);
    drop.position = position;
    drop.mag = mag;
    drop.reserve = Row(id).def.reserve;
}

int GroundCount(const WorldState& state) {
    int count = 0;
    for (const GroundWeapon& drop : state.ground_weapons) {
        count += drop.active ? 1 : 0;
    }
    return count;
}

} // namespace

TEST_CASE("玩家被击杀时原地掉落主武器并保留剩余弹药") {
    WorldState state = Duel();
    GiveArms(state, 0, WeaponId::AK47);
    GiveArms(state, 1, WeaponId::M4A4, 17, 53);
    state.players[1].health = 36;
    state.players[1].position = {10.0f, 120.0f, 0.0f};

    const StepResult result = StepOne(state, Fire());

    REQUIRE(result.state.players[1].alive == false);
    CHECK(GroundCount(result.state) == 1);
    const GroundWeapon& drop = result.state.ground_weapons[0];
    CHECK(drop.active);
    CHECK(drop.weapon_id == static_cast<uint8_t>(WeaponId::M4A4));
    CHECK(drop.position.x == doctest::Approx(10.0f));
    CHECK(drop.position.y == doctest::Approx(120.0f));
    CHECK(drop.mag == 17);
    CHECK(drop.reserve == 53);
    CHECK(result.state.players[1].primary_id == 0); // 主武器已离手
    CHECK(CountEvents(result.events, EventKind::WeaponDropped) == 1);
}

TEST_CASE("无主武器的玩家死亡不产生掉落") {
    WorldState state = Duel();
    GiveArms(state, 0, WeaponId::AK47);
    state.players[1].health = 36;

    const StepResult result = StepOne(state, Fire());

    CHECK(result.state.players[1].alive == false);
    CHECK(GroundCount(result.state) == 0);
    CHECK(CountEvents(result.events, EventKind::WeaponDropped) == 0);
}

TEST_CASE("靠近掉落物按 Pickup 拾取：主武器换成掉落武器且掉落物消失") {
    WorldState state = Duel();
    GiveArms(state, 0, WeaponId::AK47);
    GiveArms(state, 1, WeaponId::M4A4, 9, 31);
    state.players[1].health = 36;
    const StepResult killed = StepOne(state, Fire());
    REQUIRE(GroundCount(killed.state) == 1);

    WorldState nearby = killed.state;
    nearby.players[0].position = nearby.ground_weapons[0].position;
    nearby.players[0].weapon.cooldown_ticks = 0;
    const StepResult picked = StepOne(nearby, Pickup());

    CHECK(picked.state.players[0].primary_id == static_cast<uint8_t>(WeaponId::M4A4));
    CHECK(picked.state.players[0].weapon.mag == 9);
    CHECK(picked.state.players[0].weapon.reserve == 31);
    CHECK(GroundCount(picked.state) == 0);
    CHECK(CountEvents(picked.events, EventKind::WeaponPickedUp) == 1);
    CHECK(FirstEventValue(picked.events, EventKind::WeaponPickedUp) ==
          static_cast<int>(WeaponId::M4A4));
}

TEST_CASE("距离超过拾取半径的掉落物不可拾取") {
    WorldState state = Duel();
    GiveArms(state, 0, WeaponId::AK47);
    PutGroundDrop(state, 0, WeaponId::AWP, {300.0f, 0.0f, 0.0f}, 4);

    const StepResult result = StepOne(state, Pickup());

    CHECK(result.state.players[0].primary_id == static_cast<uint8_t>(WeaponId::AK47));
    CHECK(GroundCount(result.state) == 1);
    CHECK(CountEvents(result.events, EventKind::WeaponPickedUp) == 0);
    CHECK(result.events.items[0].kind == EventKind::CommandRejected);
}

TEST_CASE("多件掉落物拾取最近的一件") {
    WorldState state = Duel();
    GiveArms(state, 0, WeaponId::AK47);
    PutGroundDrop(state, 0, WeaponId::AWP, {40.0f, 0.0f, 0.0f}, 4);
    PutGroundDrop(state, 1, WeaponId::M4A4, {16.0f, 0.0f, 0.0f}, 20);

    const StepResult result = StepOne(state, Pickup());

    CHECK(result.state.players[0].primary_id == static_cast<uint8_t>(WeaponId::M4A4));
    CHECK(result.state.ground_weapons[0].active); // 远的 AWP 仍在原地
    CHECK(result.state.ground_weapons[1].active == false);
}

TEST_CASE("掉落与拾取确定性：同一序列两次运行哈希一致") {
    const auto run = []() {
        WorldState state = Duel();
        GiveArms(state, 0, WeaponId::AK47);
        GiveArms(state, 1, WeaponId::M4A4, 9, 31);
        state.players[1].health = 36;
        StepResult result = StepOne(state, Fire());
        result = StepIdle(result.state, 6);
        WorldState nearby = result.state;
        nearby.players[0].position = nearby.ground_weapons[0].position;
        result = StepOne(nearby, Pickup());
        result = StepIdle(result.state, 3);
        return HashState(result.state);
    };

    CHECK(run() == run());
}

TEST_CASE("状态哈希覆盖地面掉落武器") {
    WorldState base = Duel();
    PutGroundDrop(base, 0, WeaponId::AWP, {1.0f, 2.0f, 3.0f}, 4);
    WorldState inactive = base;
    inactive.ground_weapons[0].active = false;
    WorldState other_id = base;
    other_id.ground_weapons[0].weapon_id = static_cast<uint8_t>(WeaponId::M4A4);
    WorldState other_pos = base;
    other_pos.ground_weapons[0].position = {9.0f, 2.0f, 3.0f};
    WorldState other_ammo = base;
    other_ammo.ground_weapons[0].mag = 1;
    WorldState moved = base;
    moved.ground_weapons[1].active = true;

    CHECK(HashState(base) == HashState(base));
    CHECK(HashState(base) != HashState(inactive));
    CHECK(HashState(base) != HashState(other_id));
    CHECK(HashState(base) != HashState(other_pos));
    CHECK(HashState(base) != HashState(other_ammo));
    CHECK(HashState(base) != HashState(moved));
}

TEST_CASE("回合重置清空地面掉落武器") {
    WorldState state = Duel();
    PutGroundDrop(state, 0, WeaponId::AWP, {0.0f, 0.0f, 0.0f}, 4);
    state.round.phase = RoundPhase::RoundEnd;
    state.round.phase_ticks = 1;

    const StepResult result = Step(StepIdle(state, 0).state, DefaultContent(), {});
    const StepResult next = Step(result.state, DefaultContent(), {});
    // 第一次 Step 递减 end_ticks，第二次触发 ResetRound。
    CHECK(next.state.round.phase == RoundPhase::Freeze);
    CHECK(GroundCount(next.state) == 0);
}

TEST_CASE("地面掉落武器数量不超过 kMaxDrops，且拾取只消耗一件") {
    WorldState state = Duel();
    GiveArms(state, 0, WeaponId::AK47);
    for (int i = 0; i < recoil::kMaxDrops; ++i) {
        PutGroundDrop(state, i, WeaponId::AWP, {0.0f, 0.0f, 0.0f}, 4);
    }
    // 已满：再掉落一件不改变数量。
    WorldState victim = Duel();
    GiveArms(victim, 1, WeaponId::M4A4);
    for (int i = 0; i < recoil::kMaxDrops; ++i) {
        PutGroundDrop(victim, i, WeaponId::AWP, {0.0f, 0.0f, 0.0f}, 4);
    }
    victim.players[1].health = 36;
    const StepResult killed = StepOne(victim, Fire());
    CHECK(GroundCount(killed.state) == recoil::kMaxDrops);

    const StepResult picked = StepOne(state, Pickup());
    CHECK(GroundCount(picked.state) == recoil::kMaxDrops - 1);
}
