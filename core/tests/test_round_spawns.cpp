#include <cstdint>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/sim/match_types.h"
#include "recoil/sim/round.h"
#include "recoil/world/map.h"

// spec-debug2 AC-09：两侧出生点必须在任何模式下真正生效。
// 回归背景：布局里的 spawns.T/CT.slots 曾经只写 JSON、从没被内核读过，
// 回合开始一律走「中心点 + 64u 横排」，1v1 甚至完全用旧常量出生。
// 本用例锁死：①Duel1v1/Squad3v3/Comp5v5 都用布局槽位；②槽位按序分配；
//          ③布局没给槽（custom_slots=false）才退回 64u 横排（间距不变）。

namespace {

using recoil::Content;
using recoil::DefaultContent;
using recoil::EventList;
constexpr int kSlots = 5; // 与布局 spawns.*.slots 一致
using recoil::MapDef;
using recoil::ModeId;
using recoil::PlayerState;
using recoil::StartRound;
using recoil::Team;
using recoil::Vec3;
using recoil::WorldState;

Vec3 SlotT(int index) {
    return {1000.0f + static_cast<float>(index) * 64.0f, 0.0f, 160.0f};
}
Vec3 SlotCT(int index) {
    return {-1000.0f + static_cast<float>(index) * 64.0f, 0.0f, -160.0f};
}

MapDef MakeMapWithSlots() {
    MapDef map;
    for (int i = 0; i < kSlots; ++i) {
        map.spawn_slots_t[i] = SlotT(i);
        map.spawn_slots_ct[i] = SlotCT(i);
    }
    map.custom_slots = true;
    return map;
}

// 按模式规则铺一支满编队伍：team_id 0 起交替 T/CT，顺序与 StartRound 的出生队列一致。
WorldState MakeRoster(int team_size, ModeId mode) {
    WorldState state;
    state.player_count = static_cast<uint8_t>(team_size * 2);
    state.match.enabled = true;
    state.match.team_size = static_cast<uint8_t>(team_size);
    state.match.mode = mode;
    for (int i = 0; i < team_size * 2; ++i) {
        PlayerState& player = state.players[i];
        player.alive = true;
        player.health = 100;
        player.on_ground = true;
        player.team_id = static_cast<uint8_t>(i % 2);
        player.team = i % 2 == 0 ? static_cast<uint8_t>(Team::T) : static_cast<uint8_t>(Team::CT);
    }
    return state;
}

// 出生后按队伍内的排队序号取期望槽位（T/CT 各自从 slot0 开始）。
void ExpectSlots(WorldState& state, int team_size, const MapDef& map) {
    Content content = DefaultContent();
    EventList events;
    int t_slot = 0;
    int ct_slot = 0;
    StartRound(state, content, events, false, &map);
    for (int i = 0; i < team_size * 2; ++i) {
        const bool is_t = i % 2 == 0;
        const int slot = is_t ? t_slot++ : ct_slot++;
        const Vec3& want = is_t ? map.spawn_slots_t[slot] : map.spawn_slots_ct[slot];
        CHECK(state.players[i].position.x == doctest::Approx(want.x));
        CHECK(state.players[i].position.y == doctest::Approx(want.y));
        CHECK(state.players[i].position.z == doctest::Approx(want.z));
    }
}

} // namespace

TEST_CASE("1v1 模式也从布局出生槽出生") {
    MapDef map = MakeMapWithSlots();
    WorldState state = MakeRoster(1, ModeId::Duel1v1);
    ExpectSlots(state, 1, map);
}

TEST_CASE("3v3 模式按序占用前三个槽") {
    MapDef map = MakeMapWithSlots();
    WorldState state = MakeRoster(3, ModeId::Squad3v3);
    ExpectSlots(state, 3, map);
}

TEST_CASE("5v5 模式用满五个出生槽") {
    MapDef map = MakeMapWithSlots();
    WorldState state = MakeRoster(5, ModeId::Comp5v5);
    ExpectSlots(state, 5, map);
}

TEST_CASE("布局没有槽位时退回 64u 横排") {
    MapDef map;
    map.custom_slots = false;
    WorldState state = MakeRoster(5, ModeId::Comp5v5);
    Content content = DefaultContent();
    EventList events;
    StartRound(state, content, events, false, &map);
    float first_t_x = 0.0f;
    int t_seen = 0;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        if (state.players[i].team != static_cast<uint8_t>(Team::T)) {
            continue;
        }
        if (t_seen == 0) {
            first_t_x = state.players[i].position.x;
        }
        CHECK(state.players[i].position.x ==
              doctest::Approx(first_t_x + static_cast<float>(t_seen) * 64.0f));
        ++t_seen;
    }
    CHECK(t_seen == kSlots);
}
