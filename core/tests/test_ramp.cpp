#include <cmath>
#include <vector>

#include "doctest/doctest.h"
#include "recoil/ai/nav.h"
#include "recoil/movement/movement.h"
#include "recoil/world/map.h"

// spec-debug2：坡道是「斜面」而不是台阶。
// 回归要点：①沿坡走脚底贴面、Z 单调；②下坡不腾空；③从坡侧撞上高差 >32u 被挡住；
//          ④坡度 >45° 在载入期拒绝；⑤y 轴坡同样可用。对应 AC-02 / AC-05。

namespace {

using recoil::Aabb;
using recoil::AddRamp;
using recoil::MapDef;
using recoil::MapError;
using recoil::MovementDef;
using recoil::PlayerState;
using recoil::RampFloorAt;
using recoil::StepMovement;
using recoil::Vec3;

// A-14 的尺寸：沿 +x 460u，抬高 128u（约 15.6°，符合 CS 可走坡）。
MapDef MakeRampMap(float rise = 128.0f) {
    MapDef map;
    const Aabb bounds{{0.0f, -100.0f, 0.0f}, {460.0f, 100.0f, rise}};
    REQUIRE(AddRamp(map, bounds, 0, true) == MapError::None);
    return map;
}

PlayerState MakePlayer(float x, float y, float z) {
    PlayerState player;
    player.alive = true;
    player.health = 100;
    player.on_ground = true;
    player.position = {x, y, z};
    player.weapon.mag = 30;
    player.weapon.reserve = 90;
    return player;
}

} // namespace

TEST_CASE("沿坡上行：脚底贴着坡面连续抬升，全程可走") {
    const MapDef map = MakeRampMap();
    const MovementDef def;
    PlayerState player = MakePlayer(-20.0f, 0.0f, 0.0f);
    float previous_z = player.position.z;
    float max_step_rise = 0.0f;
    for (int tick = 0; tick < 200 && player.position.x < 440.0f; ++tick) {
        player.wish_forward = 1.0f;
        StepMovement(player, def, &map);
        const float floor = RampFloorAt(map.ramps[0], player.position.x, 0.0f);
        max_step_rise = std::max(max_step_rise, player.position.z - previous_z);
        CHECK(player.position.z >= previous_z - 0.01f);
        CHECK(std::fabs(player.position.z - floor) <= 2.0f);
        CHECK(player.on_ground);
        previous_z = player.position.z;
    }
    CHECK(player.position.z > 120.0f); // 到达坡顶
    CHECK(max_step_rise < 32.0f);      // 不是台阶式跳变
}

TEST_CASE("沿坡下行：贴地下滑，不腾空、不穿地") {
    const MapDef map = MakeRampMap();
    const MovementDef def;
    PlayerState player = MakePlayer(440.0f, 0.0f, 128.0f);
    for (int tick = 0; tick < 200 && player.position.x > 20.0f; ++tick) {
        player.wish_forward = -1.0f;
        StepMovement(player, def, &map);
        const float floor = RampFloorAt(map.ramps[0], player.position.x, 0.0f);
        CHECK(player.position.z >= floor - 0.6f); // 不低于坡面（不穿地）
        CHECK(player.position.z <= floor + 2.0f); // 也不悬空
        CHECK(player.on_ground);
    }
    CHECK(player.position.z < 10.0f); // 已经下到坡脚
}

TEST_CASE("从坡侧进入：高差超过一级台阶时被挡住") {
    const MapDef map = MakeRampMap();
    const MovementDef def;
    PlayerState player = MakePlayer(230.0f, -160.0f, 0.0f);
    for (int tick = 0; tick < 60; ++tick) {
        player.wish_side = 1.0f;
        StepMovement(player, def, &map);
    }
    // 坡在 x=230 处高 64u；从侧面走会停在坡脚外（hull 半宽 16 的外扩处）。
    CHECK(player.position.y < -100.0f);
    CHECK(player.position.z < 1.0f);
}

TEST_CASE("坡度超过 45°：载入期拒绝，地图不被修改") {
    MapDef map;
    const Aabb steep{{0.0f, 0.0f, 0.0f}, {100.0f, 100.0f, 140.0f}};
    CHECK(AddRamp(map, steep, 0, true) == MapError::SlopeTooSteep);
    CHECK(map.ramp_count == 0);
}

TEST_CASE("de_dust2 A-14 实际尺寸：从坡脚走上 128 平台") {
    MapDef map;
    const Aabb bounds{{740.0f, 1430.0f, 0.0f}, {1124.0f, 1720.0f, 128.0f}};
    REQUIRE(AddRamp(map, bounds, 0, true) == MapError::None);
    recoil::Wall top{{1140.0f, 1430.0f, 0.0f}, {1880.0f, 1720.0f, 128.0f}};
    REQUIRE(recoil::AddWall(map, top) == MapError::None);
    const MovementDef def;
    PlayerState player = MakePlayer(730.0f, 1550.0f, 0.0f);
    for (int tick = 0; tick < 2000 && player.position.x < 1160.0f; ++tick) {
        player.wish_forward = 1.0f;
        StepMovement(player, def, &map);
    }
    CHECK(player.position.x > 1160.0f);
    CHECK(player.position.z > 120.0f);
}

TEST_CASE("导航能沿坡道到达 128u 平台（bot 不会因换坡而原地不动）") {
    MapDef map = MakeRampMap();
    map.nav_min = {-256.0f, -256.0f, 0.0f};
    map.nav_max = {1024.0f, 256.0f, 256.0f};
    recoil::Wall platform{{460.0f, -100.0f, 0.0f}, {900.0f, 100.0f, 128.0f}};
    REQUIRE(recoil::AddWall(map, platform) == MapError::None);
    const std::vector<Vec3> path =
        recoil::NavPath(map, {-40.0f, 0.0f, 0.0f}, {600.0f, 0.0f, 128.0f});
    REQUIRE(!path.empty()); // 旧实现：平台 128 > 一级台阶 32，A* 直接无解
    CHECK(std::fabs(path.back().z - 128.0f) <= 32.0f);
}

TEST_CASE("沿 y 轴下降的坡：方向正确、可上下") {
    MapDef map;
    const Aabb bounds{{-100.0f, 0.0f, 0.0f}, {100.0f, 200.0f, 96.0f}};
    REQUIRE(AddRamp(map, bounds, 1, false) == MapError::None); // y 越大越低
    CHECK(RampFloorAt(map.ramps[0], 0.0f, 0.0f) == doctest::Approx(96.0f));
    CHECK(RampFloorAt(map.ramps[0], 0.0f, 200.0f) == doctest::Approx(0.0f));

    const MovementDef def;
    PlayerState player = MakePlayer(0.0f, -20.0f, 96.0f);
    for (int tick = 0; tick < 120; ++tick) {
        player.wish_side = 1.0f;
        StepMovement(player, def, &map);
    }
    CHECK(player.position.y > 150.0f);
    CHECK(player.position.z < 40.0f);
}
