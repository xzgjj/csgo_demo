#include <cmath>
#include <cstddef>
#include <cstdint>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/sim/round.h"
#include "recoil/sim/step.h"

namespace {

using recoil::AssignTeams;
using recoil::DefaultContent;
using recoil::PlayerState;
using recoil::RoundPhase;
using recoil::Step;
using recoil::Team;
using recoil::WorldState;

constexpr int kTeamSize = 5;
constexpr float kSpawnY = 1700.0f;
constexpr float kSpawnTolerance = 128.0f;

void FillPlayers(WorldState& state, int count) {
    state.player_count = static_cast<uint8_t>(count);
    for (int i = 0; i < count; ++i) {
        PlayerState& player = state.players[static_cast<size_t>(i)];
        player.alive = true;
        player.health = 100;
        player.on_ground = true;
        player.weapon.mag = 30;
        player.weapon.reserve = 90;
        player.money = static_cast<uint16_t>(100 * (i + 1));
    }
}

WorldState MakeFiveOnFive() {
    WorldState state;
    FillPlayers(state, 10);
    AssignTeams(state);
    state.round.phase = RoundPhase::RoundEnd;
    state.round.phase_ticks = 1;
    return state;
}

WorldState MakeDuel() {
    WorldState state;
    FillPlayers(state, 2);
    state.players[0].team = static_cast<uint8_t>(Team::T);
    state.players[1].team = static_cast<uint8_t>(Team::CT);
    state.round.phase = RoundPhase::RoundEnd;
    state.round.phase_ticks = 1;
    return state;
}

} // namespace

TEST_CASE("AssignTeams 把 10 名玩家分成 5 T 与 5 CT") {
    WorldState state;
    FillPlayers(state, 10);

    AssignTeams(state);

    for (int i = 0; i < kTeamSize; ++i) {
        CHECK(state.players[static_cast<size_t>(i)].team == static_cast<uint8_t>(Team::T));
        CHECK(state.players[static_cast<size_t>(i + kTeamSize)].team ==
              static_cast<uint8_t>(Team::CT));
    }
}

TEST_CASE("AssignTeams 对 2 人保持一名 T 与一名 CT") {
    WorldState state;
    FillPlayers(state, 2);

    AssignTeams(state);

    CHECK(state.players[0].team == static_cast<uint8_t>(Team::T));
    CHECK(state.players[1].team == static_cast<uint8_t>(Team::CT));
}

TEST_CASE("10 人回合重置把 T/CT 分别放到 de_ash 出生区") {
    WorldState state = MakeFiveOnFive();

    const auto result = Step(state, DefaultContent(), {});

    for (int i = 0; i < kTeamSize; ++i) {
        const PlayerState& t = result.state.players[static_cast<size_t>(i)];
        const PlayerState& ct = result.state.players[static_cast<size_t>(i + kTeamSize)];
        CHECK(t.team == static_cast<uint8_t>(Team::T));
        CHECK(ct.team == static_cast<uint8_t>(Team::CT));
        CHECK(std::fabs(t.position.y + kSpawnY) <= kSpawnTolerance);
        CHECK(std::fabs(ct.position.y - kSpawnY) <= kSpawnTolerance);
        CHECK(std::fabs(t.position.x) <= kSpawnTolerance);
        CHECK(std::fabs(ct.position.x) <= kSpawnTolerance);
    }
}

TEST_CASE("10 人回合重置后同队出生点互不重叠") {
    WorldState state = MakeFiveOnFive();

    const auto result = Step(state, DefaultContent(), {});

    for (int i = 0; i < kTeamSize; ++i) {
        for (int j = i + 1; j < kTeamSize; ++j) {
            const auto& a = result.state.players[static_cast<size_t>(i)].position;
            const auto& b = result.state.players[static_cast<size_t>(j)].position;
            const auto& c = result.state.players[static_cast<size_t>(i + kTeamSize)].position;
            const auto& d = result.state.players[static_cast<size_t>(j + kTeamSize)].position;
            CHECK((a.x != b.x || a.y != b.y));
            CHECK((c.x != d.x || c.y != d.y));
        }
    }
}

TEST_CASE("10 人重置把 C4 给第一个 T、拆弹器给第一个 CT") {
    WorldState state;
    FillPlayers(state, 10);
    for (int i = 0; i < 10; ++i) {
        state.players[static_cast<size_t>(i)].team = static_cast<uint8_t>(Team::CT);
    }
    const uint8_t t_players[kTeamSize] = {2, 3, 4, 7, 8};
    for (int i = 0; i < kTeamSize; ++i) {
        state.players[t_players[i]].team = static_cast<uint8_t>(Team::T);
    }
    state.round.phase = RoundPhase::RoundEnd;
    state.round.phase_ticks = 1;

    const auto result = Step(state, DefaultContent(), {});

    CHECK(result.state.players[2].has_bomb);
    CHECK(result.state.players[0].has_bomb == false);
    CHECK(result.state.players[0].has_defuser);
    int bombs = 0;
    int kits = 0;
    for (int i = 0; i < 10; ++i) {
        bombs += result.state.players[static_cast<size_t>(i)].has_bomb ? 1 : 0;
        kits += result.state.players[static_cast<size_t>(i)].has_defuser ? 1 : 0;
    }
    CHECK(bombs == 1);
    CHECK(kits == 1);
}

TEST_CASE("10 人回合重置保留各自经济并分别补满武器") {
    WorldState state = MakeFiveOnFive();
    for (int i = 0; i < 10; ++i) {
        state.players[static_cast<size_t>(i)].money = static_cast<uint16_t>(100 * (i + 1));
        state.players[static_cast<size_t>(i)].weapon.mag = 1;
        state.players[static_cast<size_t>(i)].weapon.reserve = 0;
    }

    const auto result = Step(state, DefaultContent(), {});

    for (int i = 0; i < 10; ++i) {
        const PlayerState& player = result.state.players[static_cast<size_t>(i)];
        CHECK(player.money == static_cast<uint16_t>(100 * (i + 1)));
        CHECK(player.weapon.mag == 30);
        CHECK(player.weapon.reserve == 90);
    }
}

TEST_CASE("2 人回合重置保持原有出生点与队伍") {
    WorldState state = MakeDuel();

    const auto result = Step(state, DefaultContent(), {});

    CHECK(result.state.players[0].team == static_cast<uint8_t>(Team::T));
    CHECK(result.state.players[1].team == static_cast<uint8_t>(Team::CT));
    CHECK(result.state.players[0].position.x == doctest::Approx(0.0f));
    CHECK(result.state.players[0].position.y == doctest::Approx(-200.0f));
    CHECK(result.state.players[1].position.x == doctest::Approx(400.0f));
    CHECK(result.state.players[1].position.y == doctest::Approx(0.0f));
}
