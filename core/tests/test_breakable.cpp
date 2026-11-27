#include "doctest/doctest.h"

#include "recoil/sim/step.h"
#include "recoil/world/breakable.h"
#include "recoil/world/map.h"
#include "recoil/world/trace.h"

namespace {

recoil::Wall MakeWall(float x0, float z0, float x1, float z1, uint16_t hp,
                      recoil::Surface surface = recoil::Surface::Wood) {
    recoil::Wall wall;
    wall.min = {x0, -100.0f, z0};
    wall.max = {x1, 100.0f, z1};
    wall.hp = hp;
    wall.surface = surface;
    return wall;
}

bool SeesWall(const recoil::MapDef& map) {
    const recoil::TraceHit hit =
        recoil::TraceRay(map, {-300.0f, 0.0f, 64.0f}, {300.0f, 0.0f, 64.0f});
    return hit.hit;
}

} // namespace

TEST_CASE("可破坏：玻璃 20 HP 一枪打碎并移除碰撞盒") {
    recoil::MapDef map;
    const recoil::MapError error =
        recoil::AddWall(map, MakeWall(0.0f, 0.0f, 20.0f, 200.0f, 20, recoil::Surface::Glass));
    REQUIRE(error == recoil::MapError::None);
    REQUIRE(SeesWall(map));
    recoil::BreakableReport report = recoil::DamageWall(map, 0, 20);
    CHECK(report.result == recoil::BreakableResult::Destroyed);
    CHECK(map.wall_count == 0);
    CHECK_FALSE(SeesWall(map));
}

TEST_CASE("可破坏：木箱 200 HP 分两枪扣血，第二枪归零移除") {
    recoil::MapDef map;
    REQUIRE(recoil::AddWall(map, MakeWall(0.0f, 0.0f, 20.0f, 200.0f, 200)) ==
            recoil::MapError::None);
    recoil::BreakableReport first = recoil::DamageWall(map, 0, 100);
    CHECK(first.result == recoil::BreakableResult::Damaged);
    CHECK(first.remaining_hp == 100);
    CHECK(map.wall_count == 1);
    recoil::BreakableReport second = recoil::DamageWall(map, 0, 100);
    CHECK(second.result == recoil::BreakableResult::Destroyed);
    CHECK(map.wall_count == 0);
}

TEST_CASE("可破坏：木门 400 HP 伤害不足只扣血") {
    recoil::MapDef map;
    REQUIRE(recoil::AddWall(map, MakeWall(0.0f, 0.0f, 20.0f, 200.0f, 400)) ==
            recoil::MapError::None);
    recoil::BreakableReport report = recoil::DamageWall(map, 0, 36);
    CHECK(report.result == recoil::BreakableResult::Damaged);
    CHECK(report.remaining_hp == 364);
    CHECK(map.wall_count == 1);
}

TEST_CASE("可破坏：HP=0 的金属不可破坏，扣血不改地图") {
    recoil::MapDef map;
    REQUIRE(recoil::AddWall(map, MakeWall(0.0f, 0.0f, 20.0f, 20.0f, 0, recoil::Surface::Metal)) ==
            recoil::MapError::None);
    recoil::BreakableReport report = recoil::DamageWall(map, 0, 999);
    CHECK(report.result == recoil::BreakableResult::Indestructible);
    CHECK(map.wall_count == 1);
    CHECK(recoil::IsBreakable(map, 0) == false);
}

TEST_CASE("可破坏：非法索引与非法伤害不改地图") {
    recoil::MapDef map;
    REQUIRE(recoil::AddWall(map, MakeWall(0.0f, 0.0f, 20.0f, 200.0f, 200)) ==
            recoil::MapError::None);
    CHECK(recoil::DamageWall(map, -1, 10).result == recoil::BreakableResult::InvalidIndex);
    CHECK(recoil::DamageWall(map, 1, 10).result == recoil::BreakableResult::InvalidIndex);
    CHECK(recoil::DamageWall(map, 0, 0).result == recoil::BreakableResult::InvalidDamage);
    CHECK(recoil::DamageWall(map, 0, -5).result == recoil::BreakableResult::InvalidDamage);
    CHECK(map.wall_count == 1);
    CHECK(map.wall_hp[0] == 200);
}

TEST_CASE("可破坏：Step 射击命中玻璃扣血并移除，事件 WallDamaged") {
    recoil::MapDef map;
    REQUIRE(recoil::AddWall(map, MakeWall(200.0f, 0.0f, 220.0f, 200.0f, 20,
                                          recoil::Surface::Glass)) == recoil::MapError::None);
    recoil::WorldState state;
    state.player_count = 1;
    state.round.phase = recoil::RoundPhase::Live;
    state.players[0].alive = true;
    state.players[0].team = static_cast<uint8_t>(recoil::Team::T);
    state.players[0].health = 100;
    state.players[0].position = {0.0f, 0.0f, 0.0f};
    state.players[0].weapon.mag = 30;
    state.players[0].weapon.reserve = 90;
    recoil::Command fire;
    fire.kind = recoil::CommandKind::Fire;
    fire.player = 0;
    fire.yaw_degrees = 0.0f;
    fire.pitch_degrees = 0.0f;
    const recoil::Command commands[] = {fire};
    recoil::Content content;
    const recoil::StepResult result =
        recoil::Step(state, content, std::span<const recoil::Command>(commands), &map, &map);
    CHECK(map.wall_count == 0);
    bool damaged = false;
    for (int i = 0; i < result.events.count; ++i) {
        if (result.events.items[i].kind == recoil::EventKind::WallDamaged) {
            damaged = true;
        }
    }
    CHECK(damaged);
    CHECK_FALSE(SeesWall(map));
}

TEST_CASE("可破坏：Step 无 mutable_map 时只发 ShotMissed，不改地图") {
    recoil::MapDef map;
    REQUIRE(recoil::AddWall(map, MakeWall(200.0f, 0.0f, 220.0f, 200.0f, 20,
                                          recoil::Surface::Glass)) == recoil::MapError::None);
    recoil::WorldState state;
    state.player_count = 1;
    state.round.phase = recoil::RoundPhase::Live;
    state.players[0].alive = true;
    state.players[0].weapon.mag = 30;
    recoil::Command fire;
    fire.kind = recoil::CommandKind::Fire;
    fire.player = 0;
    const recoil::Command commands[] = {fire};
    recoil::Content content;
    const recoil::StepResult result =
        recoil::Step(state, content, std::span<const recoil::Command>(commands), &map);
    CHECK(map.wall_count == 1);
    bool missed = false;
    for (int i = 0; i < result.events.count; ++i) {
        if (result.events.items[i].kind == recoil::EventKind::ShotMissed) {
            missed = true;
        }
    }
    CHECK(missed);
}

TEST_CASE("可破坏：尾块 swap-erase 后 HP 跟随搬移") {
    recoil::MapDef map;
    REQUIRE(recoil::AddWall(map, MakeWall(0.0f, 0.0f, 20.0f, 200.0f, 100)) ==
            recoil::MapError::None);
    REQUIRE(recoil::AddWall(map, MakeWall(400.0f, 0.0f, 420.0f, 200.0f, 300)) ==
            recoil::MapError::None);
    recoil::DamageWall(map, 0, 100);
    CHECK(map.wall_count == 1);
    CHECK(map.walls[0].min.x == doctest::Approx(400.0f));
    CHECK(map.wall_hp[0] == 300);
    CHECK(recoil::IsBreakable(map, 0));
}
