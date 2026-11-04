#include <cstdint>

#include "doctest/doctest.h"
#include "recoil/ai/vision.h"
#include "recoil/sim/state.h"
#include "recoil/world/map.h"

namespace {

using recoil::HasLineOfSight;
using recoil::MapDef;
using recoil::NearestVisibleEnemy;
using recoil::PlayerState;
using recoil::SmokeZone;
using recoil::Team;
using recoil::Vec3;
using recoil::Wall;
using recoil::WorldState;

void PlacePlayer(WorldState& state, uint8_t index, Team team, const Vec3& position) {
    PlayerState& player = state.players[index];
    player.alive = true;
    player.health = 100;
    player.team = static_cast<uint8_t>(team);
    player.position = position;
}

WorldState Duel() {
    WorldState state;
    state.player_count = 2;
    PlacePlayer(state, 0, Team::T, {0.0f, 0.0f, 0.0f});
    PlacePlayer(state, 1, Team::CT, {0.0f, 500.0f, 0.0f});
    return state;
}

// 横在两人之间的墙（y 200..232，挡住 eye→chest 线段）。
MapDef WallBetween() {
    MapDef map;
    map.walls[0] = Wall{{-64.0f, 200.0f, 0.0f}, {64.0f, 232.0f, 128.0f}};
    map.wall_count = 1;
    return map;
}

void ActivateSmoke(WorldState& state, const Vec3& position) {
    state.smoke = SmokeZone{};
    state.smoke.active = true;
    state.smoke.position = position;
    state.smoke.radius = 144.0f;
    state.smoke.ticks_left = 100;
}

} // namespace

TEST_CASE("无障碍时两名敌人互相可见") {
    const WorldState state = Duel();

    CHECK(HasLineOfSight(state, nullptr, 0, 1));
    CHECK(HasLineOfSight(state, nullptr, 1, 0));
    CHECK(NearestVisibleEnemy(state, nullptr, 0) == 1);
    CHECK(NearestVisibleEnemy(state, nullptr, 1) == 0);
}

TEST_CASE("墙体挡住视线时不可见") {
    const WorldState state = Duel();
    const MapDef map = WallBetween();

    CHECK(HasLineOfSight(state, &map, 0, 1) == false);
    CHECK(HasLineOfSight(state, &map, 1, 0) == false);
    CHECK(NearestVisibleEnemy(state, &map, 0) == -1);
}

TEST_CASE("烟雾挡住视线时不可见") {
    WorldState state = Duel();
    ActivateSmoke(state, {0.0f, 250.0f, 0.0f});

    CHECK(HasLineOfSight(state, nullptr, 0, 1) == false);
    CHECK(NearestVisibleEnemy(state, nullptr, 0) == -1);
}

TEST_CASE("自己、阵亡目标与越界目标都不可见") {
    WorldState state = Duel();

    CHECK(HasLineOfSight(state, nullptr, 0, 0) == false);
    CHECK(HasLineOfSight(state, nullptr, 0, 2) == false);
    state.players[1].alive = false;
    CHECK(HasLineOfSight(state, nullptr, 0, 1) == false);
    CHECK(HasLineOfSight(state, nullptr, 2, 0) == false);
    CHECK(NearestVisibleEnemy(state, nullptr, 0) == -1);
}

TEST_CASE("最近敌人被墙挡住时改选可见的较远敌人") {
    WorldState state;
    state.player_count = 3;
    PlacePlayer(state, 0, Team::T, {0.0f, 0.0f, 0.0f});
    PlacePlayer(state, 1, Team::CT, {0.0f, 300.0f, 0.0f});
    PlacePlayer(state, 2, Team::CT, {0.0f, -400.0f, 0.0f});
    MapDef map;
    map.walls[0] = Wall{{-64.0f, 120.0f, 0.0f}, {64.0f, 152.0f, 128.0f}};
    map.wall_count = 1;

    CHECK(HasLineOfSight(state, &map, 0, 1) == false);
    CHECK(HasLineOfSight(state, &map, 0, 2));
    CHECK(NearestVisibleEnemy(state, &map, 0) == 2);
}

TEST_CASE("队友也满足几何可见（阵营过滤由调用方决定）") {
    WorldState state = Duel();
    state.players[1].team = static_cast<uint8_t>(Team::T);

    CHECK(HasLineOfSight(state, nullptr, 0, 1));
    CHECK(NearestVisibleEnemy(state, nullptr, 0) == -1);
}
