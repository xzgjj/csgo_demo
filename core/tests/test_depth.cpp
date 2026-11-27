#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "doctest/doctest.h"
#include "recoil/ai/economy_policy.h"
#include "recoil/content/content.h"
#include "recoil/content/weapon_table.h"
#include "recoil/sim/damage.h"
#include "recoil/sim/event.h"
#include "recoil/sim/step.h"

namespace {

using recoil::BuyState;
using recoil::ComputeDamage;
using recoil::DamageInput;
using recoil::FindWeaponRow;
using recoil::HitGroup;
using recoil::RecommendBuyState;
using recoil::WeaponId;
using recoil::WeaponRow;

struct TtkCase {
    WeaponId weapon;
    float distance;
    int target_shots;
};

int ShotsToKill(WeaponId id, float distance, uint16_t armor, bool helmet,
                HitGroup group = HitGroup::Chest) {
    const WeaponRow* row = FindWeaponRow(id);
    if (row == nullptr) {
        return 99;
    }
    DamageInput in{};
    in.base = row->def.damage;
    in.range_modifier = row->def.range_modifier;
    in.range_unit = row->def.range_modifier_unit;
    in.armor_ratio = row->def.armor_ratio;
    in.group = group;
    in.distance = distance;
    in.armor = armor;
    in.helmet = helmet;
    int shots = 0;
    while (shots < 30) {
        ++shots;
        const int applied = static_cast<int>(ComputeDamage(in).applied);
        if (applied * shots >= 100) {
            break;
        }
    }
    return shots;
}

} // namespace

TEST_CASE("DEP-01 TTK：AK/M4 胸甲击杀发数与目标偏差 ≤1") {
    const TtkCase cases[] = {
        {WeaponId::AK47, 5.0f, 3}, {WeaponId::AK47, 18.0f, 4}, {WeaponId::AK47, 32.0f, 5},
        {WeaponId::M4A4, 5.0f, 3}, {WeaponId::M4A4, 18.0f, 4}, {WeaponId::M4A4, 32.0f, 5},
        {WeaponId::AWP, 30.0f, 1},
    };
    FILE* csv = nullptr;
    const char* csv_path = std::getenv("RECOIL_TTK_CSV");
    if (csv_path != nullptr) {
        csv = std::fopen(csv_path, "w");
        if (csv != nullptr) {
            std::fprintf(csv, "weapon,distance,shots,target,delta\n");
        }
    }
    for (const TtkCase& item : cases) {
        const int shots = ShotsToKill(item.weapon, item.distance, 100, false);
        const int delta = std::abs(shots - item.target_shots);
        CHECK_LE(delta, 1);
        if (csv != nullptr) {
            const WeaponRow* row = FindWeaponRow(item.weapon);
            std::fprintf(csv, "%s,%.0f,%d,%d,%d\n", row == nullptr ? "?" : row->name, item.distance,
                         shots, item.target_shots, delta);
        }
    }
    if (csv != nullptr) {
        std::fclose(csv);
    }
    // 25–40 m：AK 头甲一枪致死；M4 不允许（保留武器定位差）。
    CHECK_EQ(ShotsToKill(WeaponId::AK47, 32.0f, 100, true, HitGroup::Head), 1);
    CHECK_GT(ShotsToKill(WeaponId::M4A4, 32.0f, 100, true, HitGroup::Head), 1);
}

TEST_CASE("DEP-03 经济：四状态阈值边界与推荐一致") {
    CHECK_EQ(RecommendBuyState(4000), BuyState::Full);
    CHECK_EQ(RecommendBuyState(3999), BuyState::Half);
    CHECK_EQ(RecommendBuyState(2500), BuyState::Half);
    CHECK_EQ(RecommendBuyState(2499), BuyState::Force);
    CHECK_EQ(RecommendBuyState(1500), BuyState::Force);
    CHECK_EQ(RecommendBuyState(1499), BuyState::Eco);
    CHECK_EQ(RecommendBuyState(0), BuyState::Eco);
    CHECK_EQ(recoil::BuyStateName(BuyState::Full), std::string("full"));
}

TEST_CASE("DEP-02 受击方向：相对受害者视角左右对称，仅实际掉血才发事件") {
    recoil::WorldState state;
    state.player_count = 2;
    for (int i = 0; i < 2; ++i) {
        recoil::PlayerState& p = state.players[static_cast<size_t>(i)];
        p.alive = true;
        p.health = 100;
        p.weapon.mag = 30;
        p.weapon.reserve = 90;
        p.team = static_cast<uint8_t>(i == 0 ? recoil::Team::T : recoil::Team::CT);
    }
    state.players[0].position = {0.0f, 0.0f, 0.0f};
    state.players[1].position = {100.0f, 0.0f, 0.0f};
    recoil::Command fire;
    fire.kind = recoil::CommandKind::Fire;
    fire.player = 0;
    fire.yaw_degrees = 0.0f;
    const recoil::Content content = recoil::DefaultContent();

    state.players[1].view_yaw = 90.0f; // 面朝 +y，射手在右手边 → 左侧 -90°
    recoil::StepResult result = recoil::Step(state, content, std::array<recoil::Command, 1>{fire});
    const recoil::Event* hit = nullptr;
    for (const recoil::Event& e : result.events.items) {
        if (e.kind == recoil::EventKind::PlayerHitTaken) {
            hit = &e;
            break;
        }
    }
    REQUIRE(hit != nullptr);
    CHECK_EQ(hit->source, 0);
    CHECK_EQ(hit->target, 1);
    CHECK(std::abs(std::abs(hit->f0) - 90.0f) < 0.5f);

    recoil::WorldState mirrored = state;
    mirrored.players[1].view_yaw = 270.0f;
    recoil::StepResult left = recoil::Step(mirrored, content, std::array<recoil::Command, 1>{fire});
    for (const recoil::Event& e : left.events.items) {
        if (e.kind == recoil::EventKind::PlayerHitTaken) {
            CHECK(std::abs(e.f0 + hit->f0) < 0.5f);
            break;
        }
    }

    // 目标已死不再掉血，也就不发受击事件。
    recoil::WorldState dead = state;
    dead.players[1].alive = false;
    dead.players[1].health = 0;
    recoil::StepResult none = recoil::Step(dead, content, std::array<recoil::Command, 1>{fire});
    for (const recoil::Event& e : none.events.items) {
        CHECK(e.kind != recoil::EventKind::PlayerHitTaken);
    }
}
