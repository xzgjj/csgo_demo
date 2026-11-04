#include <array>
#include <cmath>
#include <cstdint>

#include "doctest/doctest.h"
#include "recoil/ai/bot.h"
#include "recoil/ai/nav.h"
#include "recoil/content/content.h"
#include "recoil/sim/event.h"
#include "recoil/sim/step.h"
#include "recoil/world/map.h"

namespace {

using recoil::BotDecide;
using recoil::Command;
using recoil::CommandKind;
using recoil::Content;
using recoil::DefaultContent;
using recoil::EventKind;
using recoil::HashState;
using recoil::MapDef;
using recoil::NavPath;
using recoil::PlayerState;
using recoil::RoundPhase;
using recoil::Step;
using recoil::StepResult;
using recoil::Team;
using recoil::Vec3;
using recoil::Wall;
using recoil::WorldState;

constexpr float kPi = 3.14159265358979323846f;

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

WorldState LiveDuel() {
    WorldState state = Duel();
    state.round.phase = RoundPhase::Live;
    state.round.phase_ticks = 60000;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        state.players[i].on_ground = true;
        state.players[i].weapon.mag = 30;
        state.players[i].weapon.reserve = 90;
    }
    return state;
}

// 反应延迟已经走完（模拟 Step 连续看到敌人 N tick 后的状态）。
void ArmReaction(WorldState& state, uint8_t self, uint8_t target,
                 uint16_t ticks = DefaultContent().round.bot_reaction_ticks) {
    state.players[self].reaction_target = target;
    state.players[self].reaction_ticks = ticks;
}

void ActivateSmoke(WorldState& state, const Vec3& position) {
    state.smoke.active = true;
    state.smoke.position = position;
    state.smoke.radius = 144.0f;
    state.smoke.ticks_left = 100;
}

float YawDegrees(const Vec3& from, const Vec3& to) {
    const Vec3 d = to - from;
    return std::atan2(d.y, d.x) * (180.0f / kPi);
}

int CountEvents(const recoil::EventList& events, EventKind kind) {
    int count = 0;
    for (int i = 0; i < events.count; ++i) {
        if (events.items[i].kind == kind) {
            ++count;
        }
    }
    return count;
}

} // namespace

TEST_CASE("Bot 看到敌人时开火并朝向敌人") {
    WorldState state = Duel();
    ArmReaction(state, 0, 1);
    const auto command = BotDecide(state, DefaultContent(), 0);
    CHECK(command.kind == CommandKind::Fire);
    CHECK(std::abs(command.yaw_degrees - 90.0f) <= DefaultContent().round.bot_aim_error_deg + 0.2f);
}

TEST_CASE("Bot 被烟雾遮挡时不开火") {
    WorldState state = Duel();
    ActivateSmoke(state, {0.0f, 250.0f, 0.0f});
    const auto command = BotDecide(state, DefaultContent(), 0);
    CHECK(command.kind == CommandKind::Move);
    CHECK(command.move_forward > 0.0f);
}

TEST_CASE("Bot 死亡或没有敌人时停止") {
    WorldState state = Duel();
    state.players[0].alive = false;
    CHECK(BotDecide(state, DefaultContent(), 0).kind == CommandKind::Stop);
}

TEST_CASE("没有存活敌人时 Bot 停止") {
    WorldState state = Duel();
    state.players[1].alive = false;
    CHECK(BotDecide(state, DefaultContent(), 0).kind == CommandKind::Stop);
}

TEST_CASE("最近敌人被烟雾遮挡时改打可见的较远敌人") {
    WorldState state;
    state.player_count = 3;
    PlacePlayer(state, 0, Team::T, {0.0f, 0.0f, 0.0f});
    PlacePlayer(state, 1, Team::CT, {0.0f, 300.0f, 0.0f});
    PlacePlayer(state, 2, Team::CT, {0.0f, -900.0f, 0.0f}); // 更远的敌人 2
    ActivateSmoke(state, {0.0f, 150.0f, 0.0f});
    ArmReaction(state, 0, 2);

    const auto command = BotDecide(state, DefaultContent(), 0);

    CHECK(command.kind == CommandKind::Fire);
    CHECK(std::abs(command.yaw_degrees + 90.0f) <= DefaultContent().round.bot_aim_error_deg + 0.2f);
}

TEST_CASE("无可见敌人时沿导航路径推进") {
    WorldState state = Duel();
    ActivateSmoke(state, {0.0f, 250.0f, 0.0f});
    MapDef map;
    map.walls[0] = Wall{{-96.0f, 128.0f, 0.0f}, {96.0f, 192.0f, 256.0f}};
    map.wall_count = 1;
    const Vec3 from = state.players[0].position;
    const Vec3 to = state.players[1].position;
    const auto path = NavPath(map, from, to);
    REQUIRE(!path.empty());

    const auto command = BotDecide(state, DefaultContent(), 0, &map);

    CHECK(command.kind == CommandKind::Move);
    CHECK(command.move_forward == doctest::Approx(1.0f));
    CHECK(command.yaw_degrees == doctest::Approx(YawDegrees(from, path.front())).epsilon(0.001));
    CHECK(std::fabs(command.yaw_degrees - 90.0f) > 1.0f);
}

TEST_CASE("导航不可达时仍朝敌人方向推进") {
    WorldState state = Duel();
    ActivateSmoke(state, {0.0f, 250.0f, 0.0f});
    MapDef map;
    map.walls[0] = Wall{{-64.0f, 448.0f, 0.0f}, {0.0f, 512.0f, 256.0f}};
    map.walls[1] = Wall{{64.0f, 448.0f, 0.0f}, {128.0f, 512.0f, 256.0f}};
    map.walls[2] = Wall{{0.0f, 384.0f, 0.0f}, {64.0f, 448.0f, 256.0f}};
    map.walls[3] = Wall{{0.0f, 512.0f, 0.0f}, {64.0f, 576.0f, 256.0f}};
    map.wall_count = 4;
    const Vec3 from = state.players[0].position;
    const Vec3 to = state.players[1].position;
    REQUIRE(NavPath(map, from, to).empty());

    const auto command = BotDecide(state, DefaultContent(), 0, &map);

    CHECK(command.kind == CommandKind::Move);
    CHECK(command.move_forward == doctest::Approx(1.0f));
    CHECK(std::abs(command.yaw_degrees - 90.0f) <= DefaultContent().round.bot_aim_error_deg + 0.2f);
}

TEST_CASE("Bot 被墙体遮挡时不开火而是推进") {
    const WorldState state = Duel();
    MapDef map;
    map.wall_count = 1;
    map.walls[0] = Wall{{-64.0f, 200.0f, 0.0f}, {64.0f, 232.0f, 128.0f}};

    const auto command = BotDecide(state, DefaultContent(), 0, &map);

    CHECK(command.kind == CommandKind::Move);
    CHECK(command.move_forward > 0.0f);
}

TEST_CASE("反应延迟：首次看到敌人要等 bot_reaction_ticks 才开火") {
    const Content content = DefaultContent();
    WorldState state = LiveDuel();

    const StepResult sighting = Step(state, content, {});
    CHECK(sighting.state.players[0].reaction_target == 1);
    CHECK(sighting.state.players[0].reaction_ticks == 1);
    CHECK(BotDecide(sighting.state, content, 0).kind != CommandKind::Fire);

    WorldState current = sighting.state;
    for (uint16_t tick = 2; tick < content.round.bot_reaction_ticks; ++tick) {
        current = Step(current, content, {}).state;
    }
    CHECK(current.players[0].reaction_ticks == content.round.bot_reaction_ticks - 1);
    CHECK(BotDecide(current, content, 0).kind != CommandKind::Fire);

    current = Step(current, content, {}).state;
    CHECK(current.players[0].reaction_ticks == content.round.bot_reaction_ticks);
    CHECK(BotDecide(current, content, 0).kind == CommandKind::Fire);
}

TEST_CASE("反应延迟：换目标后从 1 重新计时") {
    const Content content = DefaultContent();
    WorldState state = LiveDuel();
    state.player_count = 3;
    PlacePlayer(state, 2, Team::CT, {0.0f, -900.0f, 0.0f}); // 更远的敌人 2
    state.players[0].reaction_target = 2;                   // 之前锁定较远的敌人 2
    state.players[0].reaction_ticks = 15;

    // 敌人 1（更近）进入视野 → 目标切换为 1，反应计时清零重来。
    const StepResult switched = Step(state, content, {});
    CHECK(switched.state.players[0].reaction_target == 1);
    CHECK(switched.state.players[0].reaction_ticks == 1);
    CHECK(BotDecide(switched.state, content, 0).kind != CommandKind::Fire);
}

TEST_CASE("反应延迟：敌人从视野消失后反应清零") {
    const Content content = DefaultContent();
    WorldState state = LiveDuel();
    state.players[0].reaction_target = 1;
    state.players[0].reaction_ticks = 15;
    ActivateSmoke(state, {0.0f, 250.0f, 0.0f}); // 烟雾遮断唯一的敌人

    const StepResult result = Step(state, content, {});

    CHECK(result.state.players[0].reaction_target == 0xFF);
    CHECK(result.state.players[0].reaction_ticks == 0);
}

TEST_CASE("反应延迟：墙后看不到敌人时反应清零") {
    const Content content = DefaultContent();
    WorldState state = LiveDuel();
    MapDef map;
    map.wall_count = 1;
    map.walls[0] = Wall{{-64.0f, 200.0f, 0.0f}, {64.0f, 232.0f, 128.0f}};

    const StepResult result = Step(state, content, {}, &map);

    CHECK(result.state.players[0].reaction_target == 0xFF);
    CHECK(result.state.players[0].reaction_ticks == 0);
    CHECK(BotDecide(result.state, content, 0, &map).kind != CommandKind::Fire);
}

TEST_CASE("反应状态进 WorldState 且参与 HashState") {
    WorldState base = LiveDuel();
    WorldState armed = base;
    armed.players[0].reaction_target = 1;
    armed.players[0].reaction_ticks = 5;

    CHECK(HashState(base) == HashState(base));
    CHECK(HashState(base) != HashState(armed));
    WorldState longer = base;
    longer.players[0].reaction_target = 1;
    longer.players[0].reaction_ticks = 6;
    CHECK(HashState(armed) != HashState(longer));
}

TEST_CASE("Bot 首发瞄准误差不超过 bot_aim_error_deg") {
    const Content content = DefaultContent();
    WorldState state = LiveDuel();
    ArmReaction(state, 0, 1);

    const Command command = BotDecide(state, content, 0);

    REQUIRE(command.kind == CommandKind::Fire);
    CHECK(std::fabs(command.yaw_degrees - 90.0f) <=
          static_cast<double>(content.round.bot_aim_error_deg) + 0.2);
}

struct HitRateRun {
    int fired = 0;
    int damaged = 0;
    uint64_t hash = 0;

    bool operator==(const HitRateRun&) const = default;
};

TEST_CASE("固定场景命中率：DamageApplied/WeaponFired 比例在区间内且两次运行哈希一致") {
    const Content content = DefaultContent();
    const auto run = [&content]() {
        WorldState state = LiveDuel();
        state.players[1].health = 10000; // 靶子不死，便于统计整段命中率
        HitRateRun out;
        for (int tick = 0; tick < 300; ++tick) {
            const std::array<Command, 1> commands{BotDecide(state, content, 0)};
            const StepResult result = Step(state, content, commands);
            out.fired += CountEvents(result.events, EventKind::WeaponFired);
            out.damaged += CountEvents(result.events, EventKind::DamageApplied);
            state = result.state;
        }
        out.hash = HashState(state);
        return out;
    };
    const HitRateRun first = run();
    const HitRateRun second = run();

    CHECK(first == second);  // 两次运行逐 tick 一致（含最终状态哈希）
    CHECK(first.fired > 20); // 300 tick 内 AK 至少能打 20 发（含 16 tick 反应延迟）
    CHECK(first.damaged > 0);
    const double rate = static_cast<double>(first.damaged) / static_cast<double>(first.fired);
    CHECK(rate >= 0.3);
    CHECK(rate <= 1.0);
}
