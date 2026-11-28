#include <cmath>
#include <cstdint>
#include <vector>

#include "doctest/doctest.h"
#include "recoil/ai/tactics.h"
#include "recoil/content/content.h"
#include "recoil/sim/match.h"
#include "recoil/sim/match_types.h"
#include "recoil/sim/round.h"
#include "recoil/sim/step.h"
#include "recoil/world/map.h"

// spec-debug2 AC-15：队友（bot）必须会动，任何模式都一样。
// 用户实测：1v1 / 5v5 开局后队友一直站着。根因两条：
//   ① 战术层只在 RoundPhase::Live 出命令，而比赛开局有 30 秒 Warmup + 15 秒 Freeze；
//   ② 训练局（非 match）走的是 sim.tick()，根本没驱动 bot。
// 修复后：热身期允许移动（只在 Live 开火），冻结期仍站定；两种模式都走 tick_auto。

namespace {

using recoil::BeginMatchWithRules;
using recoil::Command;
using recoil::Content;
using recoil::DefaultContent;
using recoil::EventList;
using recoil::MapDef;
using recoil::MatchError;
using recoil::MatchRules;
using recoil::ModeId;
using recoil::RoundPhase;
using recoil::SiteLayout;
using recoil::Step;
using recoil::StepResult;
using recoil::TacticalBotDecide;
using recoil::Vec3;
using recoil::WorldState;

MapDef OpenMap() {
    MapDef map;
    map.nav_min = {-2400.0f, -2400.0f, 0.0f};
    map.nav_max = {2400.0f, 2400.0f, 256.0f};
    return map;
}

// de_dust2 规模的空图：A/B 各占一侧；台面标高取 0，保证空图也有导航层。
SiteLayout Dust2Sites() {
    SiteLayout layout;
    layout.site_a = {1500.0f, 1566.0f, 0.0f};
    layout.site_b = {-1506.0f, 1771.0f, 0.0f};
    return layout;
}

WorldState Roster(ModeId mode, uint8_t team_size) {
    WorldState state;
    MatchRules rules;
    rules.enabled = true;
    rules.mode = mode;
    // 1v1 只有一个包点，5v5 两个：与内容表对齐，否则 BeginMatchWithRules 返回 SiteConflict。
    rules.site_count = mode == ModeId::Comp5v5 ? 2 : 1;
    rules.team_size = team_size;
    EventList events;
    const MatchError error = BeginMatchWithRules(state, rules, events);
    REQUIRE(error == MatchError::Ok);
    CHECK(state.player_count == static_cast<uint8_t>(team_size * 2));
    return state;
}

// 跑 ticks 个 tick：除 0 号（人类）外都按战术层命令行动，返回 1 号累计平面位移。
float DriveBots(WorldState& state, const Content& content, MapDef& map, const SiteLayout& sites,
                int ticks) {
    float moved = 0.0f;
    for (int tick = 0; tick < ticks; ++tick) {
        std::vector<Command> commands;
        for (uint8_t i = 0; i < state.player_count; ++i) {
            if (i != 0) {
                commands.push_back(TacticalBotDecide(state, content, i, sites, &map));
            }
        }
        const Vec3 before = state.players[1].position;
        StepResult result = Step(state, content, commands, &map, &map);
        state = result.state;
        moved += std::fabs(state.players[1].position.x - before.x) +
                 std::fabs(state.players[1].position.y - before.y);
    }
    return moved;
}

} // namespace

TEST_CASE("5v5 热身期 bot 队友就位移动（不再呆立 45 秒）") {
    MapDef map = OpenMap();
    Content content = DefaultContent();
    WorldState state = Roster(ModeId::Comp5v5, 5);
    const SiteLayout sites = Dust2Sites();
    // 开局即 Warmup（1920 tick），只跑 600 tick（约 9 秒）就必须有位移。
    const float moved = DriveBots(state, content, map, sites, 600);
    CHECK(state.round.phase == RoundPhase::Warmup);
    CHECK(moved > 200.0f);
}

TEST_CASE("1v1 训练局 bot 会移动") {
    MapDef map = OpenMap();
    Content content = DefaultContent();
    WorldState state = Roster(ModeId::Duel1v1, 1);
    const SiteLayout sites = Dust2Sites();
    const float moved = DriveBots(state, content, map, sites, 600);
    CHECK(moved > 200.0f);
}

TEST_CASE("冻结期 bot 站定，等开局才动") {
    MapDef map = OpenMap();
    Content content = DefaultContent();
    WorldState state = Roster(ModeId::Comp5v5, 5);
    const SiteLayout sites = Dust2Sites();
    state.round.phase = RoundPhase::Freeze;
    state.round.phase_ticks = 200; // 跑 64 tick 仍是冻结期
    const float moved = DriveBots(state, content, map, sites, 64);
    CHECK(state.round.phase == RoundPhase::Freeze);
    CHECK(moved < 1.0f);
}
