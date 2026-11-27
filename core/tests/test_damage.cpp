#include <array>
#include <cmath>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/content/weapon_table.h"
#include "recoil/sim/ballistics.h"
#include "recoil/sim/damage.h"
#include "recoil/sim/step.h"

namespace {

using recoil::Command;
using recoil::CommandKind;
using recoil::ComputeDamage;
using recoil::Content;
using recoil::DamageInput;
using recoil::DamageResult;
using recoil::DefaultContent;
using recoil::HitGroup;
using recoil::HitGroupMultiplier;
using recoil::KillAwardFor;
using recoil::RoundPhase;
using recoil::Step;
using recoil::Team;
using recoil::WeaponId;
using recoil::WorldState;

DamageInput Ak(HitGroup group, float distance = 0.0f) {
    DamageInput in{};
    in.base = 36;
    in.range_modifier = 0.98f;
    in.range_unit = 500.0f;
    in.armor_ratio = 0.775f;
    in.group = group;
    in.distance = distance;
    return in;
}

TEST_CASE("DMG-01 公式：AK 无甲 胸36/腹45/腿27/头144") {
    CHECK_EQ(ComputeDamage(Ak(HitGroup::Chest)).applied, 36);
    CHECK_EQ(ComputeDamage(Ak(HitGroup::Stomach)).applied, 45);
    CHECK_EQ(ComputeDamage(Ak(HitGroup::Legs)).applied, 27);
    CHECK_EQ(ComputeDamage(Ak(HitGroup::Head)).applied, 144);

    // 有甲：胸 36×0.775≈27.9→28；头（无头盔）仍 144。
    DamageInput armor = Ak(HitGroup::Chest);
    armor.armor = 100;
    CHECK_EQ(ComputeDamage(armor).applied, 28);
    DamageInput head_armor = Ak(HitGroup::Head);
    head_armor.armor = 100;
    CHECK_EQ(ComputeDamage(head_armor).applied, 144);
}

TEST_CASE("DMG-02 头甲：AK 一枪致死，M4A4 不致死") {
    DamageInput ak = Ak(HitGroup::Head);
    ak.armor = 100;
    ak.helmet = true;
    CHECK_EQ(ComputeDamage(ak).applied, 112);
    CHECK(ComputeDamage(ak).applied >= 100);

    DamageInput m4{};
    m4.base = 33;
    m4.armor_ratio = 0.75f;
    m4.group = HitGroup::Head;
    m4.armor = 100;
    m4.helmet = true;
    const uint16_t m4_damage = ComputeDamage(m4).applied;
    CHECK_EQ(m4_damage, 99);
    CHECK(m4_damage < 100);
}

TEST_CASE("DMG-03 腿免甲：有甲与无甲腿伤相同") {
    DamageInput bare = Ak(HitGroup::Legs);
    DamageInput armored = bare;
    armored.armor = 100;
    armored.helmet = true;
    CHECK_EQ(ComputeDamage(bare).applied, 27);
    CHECK_EQ(ComputeDamage(armored).applied, 27);
    CHECK_FALSE(ComputeDamage(armored).armor_absorbed);
}

TEST_CASE("DMG-04 衰减：0.98^(d/500) 随距离递减") {
    const uint16_t at_0 = ComputeDamage(Ak(HitGroup::Chest, 0.0f)).applied;
    const uint16_t at_30m = ComputeDamage(Ak(HitGroup::Chest, 30.0f * 39.37f)).applied;
    const uint16_t at_60m = ComputeDamage(Ak(HitGroup::Chest, 60.0f * 39.37f)).applied;
    CHECK_EQ(at_0, 36);
    CHECK_EQ(at_30m, 34); // 0.98^(1181/500)=0.9536 → 34.3
    CHECK_EQ(at_60m, 33); // 0.98^(2562/500)=0.9091 → 32.7
    CHECK(at_30m < at_0);
    CHECK(at_60m < at_30m);
}

TEST_CASE("DMG-05 穿透：1 层 ×0.75、2 层 ×0.5625") {
    DamageInput one = Ak(HitGroup::Chest);
    one.penetration_scale = 0.75f;
    CHECK_EQ(ComputeDamage(one).applied, 27);
    DamageInput two = Ak(HitGroup::Chest);
    two.penetration_scale = 0.5625f;
    CHECK_EQ(ComputeDamage(two).applied, 20);
    DamageInput blocked = two;
    blocked.penetration_scale = 0.0f; // 混凝土：不可穿
    CHECK_EQ(ComputeDamage(blocked).applied, 0);
}

TEST_CASE("护甲损耗基于 raw 且不超过剩余护甲") {
    DamageInput in = Ak(HitGroup::Chest);
    in.armor = 100;
    const DamageResult result = ComputeDamage(in);
    CHECK(result.armor_absorbed);
    // 36 × (1-0.775) × 0.5 = 4.05 → 4
    CHECK_EQ(result.armor_loss, 4);

    in.armor = 2; // 只剩 2 点甲
    CHECK_EQ(ComputeDamage(in).armor_loss, 2);
}

TEST_CASE("部位倍率查表") {
    CHECK_EQ(HitGroupMultiplier(HitGroup::Head), 4.0f);
    CHECK_EQ(HitGroupMultiplier(HitGroup::Stomach), 1.25f);
    CHECK_EQ(HitGroupMultiplier(HitGroup::Legs), 0.75f);
    CHECK_EQ(HitGroupMultiplier(HitGroup::Chest), 1.0f);
}

TEST_CASE("DMG-10 击杀奖励按武器类别：步枪300 / 狙击100 / 手枪300") {
    CHECK_EQ(KillAwardFor(WeaponId::AK47), 300);
    CHECK_EQ(KillAwardFor(WeaponId::AWP), 100);
    CHECK_EQ(KillAwardFor(WeaponId::Glock), 300);
    CHECK_EQ(KillAwardFor(WeaponId::USP), 300);
}

} // namespace

namespace {

WorldState Duel() {
    WorldState state;
    state.player_count = 3;
    state.players[0].alive = true;
    state.players[0].team = static_cast<uint8_t>(Team::T);
    state.players[0].health = 100;
    state.players[0].money = 800;
    state.players[0].on_ground = true;
    state.players[0].position = {0.0f, 0.0f, 0.0f};
    state.players[1].alive = true;
    state.players[1].team = static_cast<uint8_t>(Team::T); // 队友挡在射线上
    state.players[1].health = 100;
    state.players[1].on_ground = true;
    state.players[1].position = {0.0f, 100.0f, 0.0f};
    state.players[2].alive = true;
    state.players[2].team = static_cast<uint8_t>(Team::CT);
    state.players[2].health = 100;
    state.players[2].on_ground = true;
    state.players[2].position = {0.0f, 400.0f, 0.0f};
    state.round.phase = RoundPhase::Live;
    state.round.phase_ticks = 100;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        state.players[i].weapon.mag = 30;
    }
    return state;
}

Command Fire() {
    Command command;
    command.kind = CommandKind::Fire;
    command.player = 0;
    command.yaw_degrees = 90.0f;
    command.pitch_degrees = 0.0f;
    return command;
}

} // namespace

TEST_CASE("DMG-06 友伤关闭：队友挡弹道但双方都不掉血") {
    WorldState state = Duel();
    Content content = DefaultContent();
    content.ballistics_enabled = false;
    content.friendly_fire = false;
    const std::array<Command, 1> commands{Fire()};
    const auto result = Step(state, content, commands);
    CHECK_EQ(result.state.players[1].health, 100); // 队友挡枪但不受伤
    CHECK_EQ(result.state.players[2].health, 100); // 子弹被队友挡住
}

TEST_CASE("DMG-06 友伤开启：队友挡弹道且受伤") {
    WorldState state = Duel();
    Content content = DefaultContent();
    content.ballistics_enabled = false;
    content.friendly_fire = true;
    const std::array<Command, 1> commands{Fire()};
    const auto result = Step(state, content, commands);
    CHECK(result.state.players[1].health < 100);
    CHECK_EQ(result.state.players[2].health, 100);
    CHECK_EQ(result.state.players[0].kills, 0); // 误杀队友不计击杀
}
