#include <cmath>
#include <cstdint>
#include <vector>

#include "doctest/doctest.h"
#include "recoil/ai/nav.h"
#include "recoil/ai/tactics.h"
#include "recoil/content/content.h"
#include "recoil/sim/round.h"
#include "recoil/world/map.h"

namespace {

using recoil::BombSite;
using recoil::ChooseAttackSite;
using recoil::CommandKind;
using recoil::DefaultContent;
using recoil::DefendSiteForSlot;
using recoil::MapDef;
using recoil::NavPath;
using recoil::PlayerState;
using recoil::RoleForPlayer;
using recoil::RoleForSlot;
using recoil::RoundPhase;
using recoil::SiteLayout;
using recoil::TacticalDecision;
using recoil::TacticState;
using recoil::Team;
using recoil::TeamRole;
using recoil::TeamSlot;
using recoil::Vec3;
using recoil::Wall;
using recoil::WorldState;

constexpr float kPi = 3.14159265358979323846f;
constexpr uint8_t kTeamSize = recoil::kTacticalTeamSize;

// 固定包点布局：A 在 +x、B 在 -x，便于按坐标断言。
SiteLayout TestLayout() {
    SiteLayout layout;
    layout.site_a = {512.0f, 0.0f, 0.0f};
    layout.site_b = {-512.0f, 0.0f, 0.0f};
    return layout;
}

void PlacePlayer(WorldState& state, uint8_t index, Team team, const Vec3& position) {
    PlayerState& player = state.players[index];
    player.alive = true;
    player.health = 100;
    player.team = static_cast<uint8_t>(team);
    player.position = position;
}

// 10 人 5v5：索引 0..4 为 T、5..9 为 CT；默认全存活于原点，回合 1 进行中。
WorldState LiveFiveOnFive() {
    WorldState state;
    state.player_count = static_cast<uint8_t>(kTeamSize * 2);
    for (uint8_t i = 0; i < state.player_count; ++i) {
        PlacePlayer(state, i, i < kTeamSize ? Team::T : Team::CT, {0.0f, 0.0f, 0.0f});
    }
    state.round.phase = RoundPhase::Live;
    state.round.number = 1;
    return state;
}

// 让指定队伍全部阵亡，避免 BotDecide 因"可见敌人"提前交火。
void KillTeam(WorldState& state, Team team) {
    for (uint8_t i = 0; i < state.player_count; ++i) {
        if (state.players[i].team == static_cast<uint8_t>(team)) {
            state.players[i].alive = false;
        }
    }
}

float YawDegrees(const Vec3& from, const Vec3& to) {
    const Vec3 d = to - from;
    return std::atan2(d.y, d.x) * (180.0f / kPi);
}

MapDef OpenMap() {
    MapDef map;
    map.nav_min = {-1024.0f, -1024.0f, 0.0f};
    map.nav_max = {1024.0f, 1024.0f, 256.0f};
    return map;
}

void AddWall(MapDef& map, const Vec3& low, const Vec3& high) {
    map.walls[map.wall_count] = Wall{low, high};
    ++map.wall_count;
}

TacticalDecision Decide(const WorldState& state, uint8_t self) {
    return recoil::TacticalDecide(state, DefaultContent(), self, TestLayout());
}

TacticalDecision Decide(const WorldState& state, uint8_t self, const MapDef& map) {
    return recoil::TacticalDecide(state, DefaultContent(), self, TestLayout(), &map);
}

} // namespace

TEST_CASE("战术角色按同队 slot 确定性分配") {
    CHECK(RoleForSlot(0) == TeamRole::Entry);
    CHECK(RoleForSlot(1) == TeamRole::Support);
    CHECK(RoleForSlot(2) == TeamRole::Anchor);
    CHECK(RoleForSlot(3) == TeamRole::Rotator);
    CHECK(RoleForSlot(4) == TeamRole::Lurker);
    CHECK(RoleForSlot(5) == TeamRole::Lurker); // 越界钳到 Lurker

    const WorldState state = LiveFiveOnFive();
    CHECK(RoleForPlayer(state, 0) == TeamRole::Entry);
    CHECK(RoleForPlayer(state, 4) == TeamRole::Lurker);
    CHECK(RoleForPlayer(state, 5) == TeamRole::Entry); // CT 使用同一张 slot 表
    CHECK(RoleForPlayer(state, 9) == TeamRole::Lurker);
    CHECK(RoleForPlayer(state, 10) == TeamRole::Lurker); // 越界
}

TEST_CASE("队内序号按同队成员计数而非全局索引") {
    WorldState state;
    state.player_count = 4;
    PlacePlayer(state, 0, Team::T, {});
    PlacePlayer(state, 1, Team::CT, {});
    PlacePlayer(state, 2, Team::T, {});
    PlacePlayer(state, 3, Team::CT, {});

    CHECK(TeamSlot(state, 0) == 0);
    CHECK(TeamSlot(state, 1) == 0);
    CHECK(TeamSlot(state, 2) == 1);
    CHECK(TeamSlot(state, 3) == 1);
    CHECK(TeamSlot(state, 4) == recoil::kTacticalTeamSize);
}

TEST_CASE("T 按回合号奇偶选点，CT 按 slot 分配两点守备") {
    WorldState state = LiveFiveOnFive();
    CHECK(ChooseAttackSite(state) == BombSite::A); // 回合 1（奇数）
    state.round.number = 2;
    CHECK(ChooseAttackSite(state) == BombSite::B);
    state.round.number = 3;
    CHECK(ChooseAttackSite(state) == BombSite::A);

    CHECK(DefendSiteForSlot(0) == BombSite::A);
    CHECK(DefendSiteForSlot(1) == BombSite::A);
    CHECK(DefendSiteForSlot(2) == BombSite::B);
    CHECK(DefendSiteForSlot(3) == BombSite::None); // Rotator 按距离机动
    CHECK(DefendSiteForSlot(4) == BombSite::B);
}

TEST_CASE("T 无 C4 时向所选包点推进，Lurker 走另一侧") {
    WorldState state = LiveFiveOnFive();
    KillTeam(state, Team::CT);

    const TacticalDecision entry = Decide(state, 0);
    CHECK(entry.role == TeamRole::Entry);
    CHECK(entry.site == BombSite::A);
    CHECK(entry.state == TacticState::Advance);
    CHECK(entry.command.kind == CommandKind::Move);
    CHECK(entry.command.player == 0);
    CHECK(entry.command.move_forward == doctest::Approx(1.0f));
    CHECK(entry.command.yaw_degrees == doctest::Approx(0.0).epsilon(0.01));

    const TacticalDecision lurker = Decide(state, 4);
    CHECK(lurker.role == TeamRole::Lurker);
    CHECK(lurker.site == BombSite::B);
    CHECK(lurker.command.kind == CommandKind::Move);
    CHECK(lurker.command.yaw_degrees == doctest::Approx(180.0).epsilon(0.01));
}

TEST_CASE("T 携带 C4 抵达包点后下包") {
    WorldState state = LiveFiveOnFive();
    KillTeam(state, Team::CT);
    state.players[0].has_bomb = true;
    state.players[0].position = {512.0f, 0.0f, 0.0f};

    const TacticalDecision on_site = Decide(state, 0);
    CHECK(on_site.state == TacticState::Plant);
    CHECK(on_site.command.kind == CommandKind::Plant);
    CHECK(on_site.command.player == 0);
    CHECK(on_site.command.yaw_degrees == doctest::Approx(0.0).epsilon(0.01));

    state.players[0].position = {470.0f, 16.0f, 0.0f}; // 半径内（plant_radius=96）
    CHECK(Decide(state, 0).command.kind == CommandKind::Plant);
}

TEST_CASE("T 携带 C4 未到包点时不提前下包") {
    WorldState state = LiveFiveOnFive();
    KillTeam(state, Team::CT);
    state.players[0].has_bomb = true;

    const TacticalDecision far = Decide(state, 0);
    CHECK(far.state == TacticState::Advance);
    CHECK(far.site == BombSite::A);
    CHECK(far.command.kind == CommandKind::Move);

    state.players[0].position = {400.0f, 0.0f, 0.0f}; // 距 A 中心 112 > 96
    CHECK(Decide(state, 0).command.kind == CommandKind::Move);
}

TEST_CASE("T 在包点附近巡逻守点并在落位后停止") {
    WorldState state = LiveFiveOnFive();
    KillTeam(state, Team::CT);
    PlacePlayer(state, 1, Team::T, {512.0f, 0.0f, 0.0f}); // slot1 Support，落位在 -x 侧

    const TacticalDecision patrol = Decide(state, 1);
    CHECK(patrol.state == TacticState::Hold);
    CHECK(patrol.command.kind == CommandKind::Move);
    CHECK(patrol.command.yaw_degrees == doctest::Approx(180.0).epsilon(0.01));

    state.players[1].position = {416.0f, 0.0f, 0.0f}; // 落位点
    const TacticalDecision settled = Decide(state, 1);
    CHECK(settled.state == TacticState::Hold);
    CHECK(settled.command.kind == CommandKind::Stop);
}

TEST_CASE("转点：T 队友阵亡后按距离改打最近包点") {
    WorldState state = LiveFiveOnFive();
    KillTeam(state, Team::CT);
    state.players[2].alive = false; // T 阵亡 1 人
    state.players[0].position = {-800.0f, 0.0f, 0.0f};

    const TacticalDecision decision = Decide(state, 0);
    CHECK(decision.site == BombSite::B); // 最近包点（默认选点仍是 A）
    CHECK(decision.state == TacticState::Rotate);
    CHECK(decision.command.kind == CommandKind::Move);
    CHECK(decision.command.yaw_degrees == doctest::Approx(0.0).epsilon(0.01)); // 朝 +x 的 B
}

TEST_CASE("转点：CT 队友阵亡后回防最近包点") {
    WorldState state = LiveFiveOnFive();
    KillTeam(state, Team::T);
    state.players[9].alive = false;                    // CT 阵亡 1 人
    state.players[5].position = {-800.0f, 0.0f, 0.0f}; // slot0 本守 A

    const TacticalDecision decision = Decide(state, 5);
    CHECK(decision.site == BombSite::B);
    CHECK(decision.state == TacticState::Rotate);
    CHECK(decision.command.kind == CommandKind::Move);
    CHECK(decision.command.yaw_degrees == doctest::Approx(0.0).epsilon(0.01)); // 朝 +x 的 B
}

TEST_CASE("CT 按分配守两点，Rotator 按距离机动") {
    WorldState state = LiveFiveOnFive();
    KillTeam(state, Team::T);

    const TacticalDecision anchor = Decide(state, 5); // slot0 → A，距 A 512 > hold_radius
    CHECK(anchor.site == BombSite::A);
    CHECK(anchor.state == TacticState::Advance);
    CHECK(anchor.command.kind == CommandKind::Move);
    CHECK(anchor.command.yaw_degrees == doctest::Approx(0.0).epsilon(0.01));

    state.players[8].position = {-480.0f, 0.0f, 0.0f}; // slot3 Rotator → 最近 B
    const TacticalDecision rotator = Decide(state, 8);
    CHECK(rotator.role == TeamRole::Rotator);
    CHECK(rotator.site == BombSite::B);
    CHECK(rotator.state == TacticState::Hold);
    CHECK(rotator.command.kind == CommandKind::Move); // 巡逻到落位点
}

TEST_CASE("CT 在包点且炸弹已埋时拆包，远离时回防") {
    WorldState state = LiveFiveOnFive();
    KillTeam(state, Team::T);
    state.bomb.planted = true;
    state.bomb.timer_ticks = 1000;
    state.bomb.position = {512.0f, 0.0f, 0.0f};
    state.players[5].position = {512.0f, 0.0f, 0.0f};

    const TacticalDecision defuse = Decide(state, 5);
    CHECK(defuse.state == TacticState::Defuse);
    CHECK(defuse.command.kind == CommandKind::Defuse);
    CHECK(defuse.command.player == 5);

    state.players[5].position = {0.0f, 0.0f, 0.0f}; // 距炸弹 512 > defuse_radius
    const TacticalDecision retake = Decide(state, 5);
    CHECK(retake.state == TacticState::Rotate);
    CHECK(retake.command.kind == CommandKind::Move);
    CHECK(retake.command.yaw_degrees == doctest::Approx(0.0).epsilon(0.01));
}

TEST_CASE("T 在炸弹已埋后回防并守包") {
    WorldState state = LiveFiveOnFive();
    KillTeam(state, Team::CT);
    state.bomb.planted = true;
    state.bomb.timer_ticks = 1000;
    state.bomb.position = {512.0f, 0.0f, 0.0f};
    state.players[1].position = {0.0f, 0.0f, 0.0f};

    const TacticalDecision retake = Decide(state, 1);
    CHECK(retake.state == TacticState::Rotate);
    CHECK(retake.command.kind == CommandKind::Move);
    CHECK(retake.command.yaw_degrees == doctest::Approx(0.0).epsilon(0.01));

    state.players[1].position = {512.0f, 0.0f, 0.0f}; // 已到包点
    const TacticalDecision guard = Decide(state, 1);
    CHECK(guard.state == TacticState::Hold);
    CHECK(guard.command.kind == CommandKind::Stop);
}

TEST_CASE("炸弹已爆炸或已拆除后停止") {
    WorldState state = LiveFiveOnFive();
    KillTeam(state, Team::CT);
    state.bomb.exploded = true;
    CHECK(Decide(state, 0).command.kind == CommandKind::Stop);
    CHECK(Decide(state, 0).state == TacticState::Idle);

    state.bomb.exploded = false;
    state.bomb.defused = true;
    CHECK(Decide(state, 0).command.kind == CommandKind::Stop);
    CHECK(Decide(state, 0).state == TacticState::Idle);
}

TEST_CASE("冻结阶段不移动也不交火") {
    WorldState state = LiveFiveOnFive();
    state.round.phase = RoundPhase::Freeze;
    state.players[0].has_bomb = true;

    CHECK(Decide(state, 0).command.kind == CommandKind::Stop);
    CHECK(Decide(state, 0).state == TacticState::Idle);

    state.players[5].position = {0.0f, 300.0f, 0.0f}; // 可见敌人也不开火
    CHECK(Decide(state, 0).command.kind == CommandKind::Stop);
}

TEST_CASE("可见敌人时优先交战开火") {
    WorldState state = LiveFiveOnFive();
    KillTeam(state, Team::CT);
    PlacePlayer(state, 5, Team::CT, {0.0f, 300.0f, 0.0f}); // 只留一名可见敌人
    state.players[0].has_bomb = true;                      // 携带 C4 也先交火

    const TacticalDecision decision = Decide(state, 0);
    CHECK(decision.state == TacticState::Engage);
    CHECK(decision.command.kind == CommandKind::Fire);
    CHECK(decision.command.yaw_degrees == doctest::Approx(90.0).epsilon(0.01));
}

TEST_CASE("有墙时沿导航路径首航点推进") {
    WorldState state = LiveFiveOnFive();
    KillTeam(state, Team::CT);
    MapDef map = OpenMap();
    AddWall(map, {256.0f, -192.0f, 0.0f}, {320.0f, 192.0f, 256.0f});
    const Vec3 from = state.players[0].position;
    const Vec3 to = {512.0f, 0.0f, 0.0f};
    const std::vector<Vec3> path = NavPath(map, from, to);
    REQUIRE(!path.empty());

    const TacticalDecision decision = Decide(state, 0, map);
    CHECK(decision.command.kind == CommandKind::Move);
    CHECK(decision.command.yaw_degrees ==
          doctest::Approx(YawDegrees(from, path.front())).epsilon(0.001));
    CHECK(std::fabs(decision.command.yaw_degrees) > 1.0f); // 不是直线朝包点
}

TEST_CASE("目标被墙占据导致无路径时原地停止") {
    WorldState state = LiveFiveOnFive();
    KillTeam(state, Team::CT);
    MapDef map = OpenMap();
    AddWall(map, {500.0f, 0.0f, 0.0f}, {520.0f, 32.0f, 256.0f}); // 覆盖 A 目标格
    REQUIRE(NavPath(map, {0.0f, 0.0f, 0.0f}, {512.0f, 0.0f, 0.0f}).empty());

    const TacticalDecision decision = Decide(state, 0, map);
    CHECK(decision.command.kind == CommandKind::Stop);
    CHECK(decision.state == TacticState::Idle);
}
