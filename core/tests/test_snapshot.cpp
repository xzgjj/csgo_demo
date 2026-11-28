#include <cstdint>

#include "doctest/doctest.h"
#include "recoil/sim/snapshot.h"
#include "recoil/sim/state.h"

namespace {

using recoil::WorldState;

} // namespace

// 快照（SimSnapshot）是宿主读取面：必须逐字段等于内核状态，且不影响状态本身。
TEST_CASE("快照逐字段等于内核状态") {
    WorldState state;
    state.player_count = 2;
    state.tick = 17;
    state.players[0].money = 4200;
    state.players[0].blind_ticks = 37;
    state.players[0].inaccuracy = 1.25f;
    state.players[0].position = {12.0f, 34.0f, 56.0f};
    state.players[0].view_yaw = -90.0f;
    state.players[1].health = 37;
    state.players[1].armor = 55;
    state.smoke.active = true;
    state.smoke.position = {128.0f, 256.0f, 0.0f};
    state.smoke.radius = 144.0f;
    state.smoke.ticks_left = 88;

    recoil::SimSnapshot snap;
    recoil::BuildSnapshot(state, snap);

    REQUIRE(snap.player_count == state.player_count);
    CHECK(snap.tick == state.tick);
    CHECK(snap.smoke_active == 1);
    CHECK(snap.smoke_x == state.smoke.position.x);
    CHECK(snap.smoke_y == state.smoke.position.y);
    CHECK(snap.smoke_ticks_left == state.smoke.ticks_left);
    for (uint8_t i = 0; i < state.player_count; ++i) {
        const recoil::PlayerState& p = state.players[i];
        const float* row = snap.players[i];
        CHECK(row[recoil::snapshot_field::kX] == p.position.x);
        CHECK(row[recoil::snapshot_field::kY] == p.position.y);
        CHECK(row[recoil::snapshot_field::kZ] == p.position.z);
        CHECK(row[recoil::snapshot_field::kYaw] == p.view_yaw);
        CHECK(row[recoil::snapshot_field::kHealth] == static_cast<float>(p.health));
        CHECK(row[recoil::snapshot_field::kArmor] == static_cast<float>(p.armor));
        CHECK(row[recoil::snapshot_field::kAlive] == (p.alive ? 1.0f : 0.0f));
        CHECK(row[recoil::snapshot_field::kTeam] == static_cast<float>(p.team));
        CHECK(row[recoil::snapshot_field::kMag] == static_cast<float>(p.weapon.mag));
        CHECK(row[recoil::snapshot_field::kReserve] == static_cast<float>(p.weapon.reserve));
        CHECK(row[recoil::snapshot_field::kMoney] == static_cast<float>(p.money));
        CHECK(row[recoil::snapshot_field::kBlindTicks] == static_cast<float>(p.blind_ticks));
        CHECK(row[recoil::snapshot_field::kInaccuracy] == p.inaccuracy);
        CHECK(row[recoil::snapshot_field::kActiveWeapon] ==
              static_cast<float>(recoil::SnapshotActiveWeapon(p)));
    }
}

TEST_CASE("快照是只读投影：改快照不回写状态") {
    WorldState state;
    state.player_count = 2;
    state.players[0].health = 100;
    recoil::SimSnapshot snap;
    recoil::BuildSnapshot(state, snap);

    snap.players[0][recoil::snapshot_field::kHealth] = 1.0f;
    snap.players[0][recoil::snapshot_field::kX] = 9999.0f;

    CHECK(state.players[0].health == 100);
    CHECK(state.players[0].position.x != 9999.0f);
}
