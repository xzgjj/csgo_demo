#include <cmath>
#include <cstdint>

#include "doctest/doctest.h"
#include "recoil/ai/constraints.h"
#include "recoil/sim/state.h"

namespace {

using recoil::AiFrameScheduler;
using recoil::AllyBlocksFireLine;
using recoil::AllyClaimsBomb;
using recoil::AllyIsDefusing;
using recoil::BotBlocksSpawn;
using recoil::DifficultyFor;
using recoil::PlayerState;
using recoil::WorldState;

constexpr float kMeter = 39.37f;

WorldState MakeWorld(uint8_t players, uint8_t shooter_yaw = 0.0f) {
    WorldState state;
    state.player_count = players;
    for (uint8_t i = 0; i < players; ++i) {
        state.players[i].alive = true;
        state.players[i].health = 100;
    }
    state.players[0].view_yaw = shooter_yaw;
    return state;
}

bool Near(float actual, float expected, float tolerance_ratio) {
    return std::fabs(actual - expected) <= std::fabs(expected) * tolerance_ratio;
}

} // namespace

TEST_CASE("MTC-11 难度表：四档反应/视距/误差/转身符合 §3.8.3（±10%）") {
    const auto& easy = DifficultyFor(0);
    CHECK_EQ(easy.level, 0);
    CHECK(Near(static_cast<float>(easy.reaction_ticks), 700.0f / 1000.0f * 64.0f, 0.1f));
    CHECK(Near(easy.view_units, 1200.0f, 0.1f));
    CHECK(Near(easy.aim_sigma_deg, 3.2f, 0.1f));
    CHECK(Near(easy.turn_deg_per_s, 240.0f, 0.1f));
    CHECK_EQ(easy.burst_shots, 0); // 泼水

    const auto& normal = DifficultyFor(1);
    CHECK(Near(static_cast<float>(normal.reaction_ticks), 500.0f / 1000.0f * 64.0f, 0.1f));
    CHECK(Near(normal.view_units, 1500.0f, 0.1f));
    CHECK(Near(normal.first_cone_deg, 2.5f, 0.1f));
    CHECK_EQ(normal.burst_shots, 7);

    const auto& hard = DifficultyFor(2);
    CHECK(Near(static_cast<float>(hard.reaction_ticks), 350.0f / 1000.0f * 64.0f, 0.1f));
    CHECK(Near(hard.aim_sigma_deg, 1.4f, 0.1f));
    CHECK(Near(hard.turn_deg_per_s, 400.0f, 0.1f));
    CHECK_EQ(hard.burst_shots, 5);

    const auto& expert = DifficultyFor(3);
    CHECK(Near(static_cast<float>(expert.reaction_ticks), 220.0f / 1000.0f * 64.0f, 0.1f));
    CHECK(Near(expert.view_units, 2000.0f, 0.1f));
    CHECK(Near(expert.turn_deg_per_s, 540.0f, 0.1f));
    // 越界夹到 Expert，调用方不必判空。
    CHECK_EQ(DifficultyFor(9).level, 3);
}

TEST_CASE("FF-1 不挡枪线：队友在射线 1.2 m 内且夹角 <8° 才抑制开火") {
    WorldState state = MakeWorld(3, 90.0f); // 朝 +y（90°）
    state.players[1].position = {0.0f, 500.0f, 0.0f};
    float angle = -1.0f;

    state.players[2].position = {0.0f, 250.0f, 0.0f}; // 正中间
    CHECK(AllyBlocksFireLine(state, 0, 1, 2, &angle));
    CHECK(angle < 1.0f);

    state.players[2].position = {30.0f, 250.0f, 0.0f}; // 30 u 偏移 ≈ 6.8°
    CHECK(AllyBlocksFireLine(state, 0, 1, 2, &angle));
    CHECK(angle < 8.0f);

    state.players[2].position = {60.0f, 250.0f, 0.0f}; // 13.5° 超出夹角
    CHECK_FALSE(AllyBlocksFireLine(state, 0, 1, 2, &angle));

    state.players[2].position = {400.0f, 250.0f, 0.0f}; // 横向 >1.2 m
    CHECK_FALSE(AllyBlocksFireLine(state, 0, 1, 2, &angle));

    state.players[2].position = {0.0f, -50.0f, 0.0f}; // 在射手背后
    CHECK_FALSE(AllyBlocksFireLine(state, 0, 1, 2, &angle));
}

TEST_CASE("FF-3 不抢包：人类距 C4 3 m 内且面向才判定为认领") {
    WorldState state = MakeWorld(2);
    state.bomb.position = {0.0f, 200.0f, 0.0f};
    state.players[0].position = {0.0f, 150.0f, 0.0f}; // 距 50 u
    state.players[0].view_yaw = 90.0f;                // 朝 +y，正对 C4
    CHECK(AllyClaimsBomb(state, 1, 0, 0.0f));

    state.players[0].view_yaw = 270.0f; // 背对
    CHECK_FALSE(AllyClaimsBomb(state, 1, 0, 0.0f));

    state.players[0].position = {0.0f, -100.0f, 0.0f}; // 距 300 u
    state.players[0].view_yaw = 90.0f;
    CHECK_FALSE(AllyClaimsBomb(state, 1, 0, 0.0f));
}

TEST_CASE("FF-5 出生槽：2 m 内视为堵出生") {
    WorldState state = MakeWorld(2);
    state.players[1].position = {0.0f, 0.0f, 0.0f};
    CHECK(BotBlocksSpawn(state, 1, {0.0f, 0.0f, 0.0f}, 0.0f));

    state.players[1].position = {0.0f, 50.0f, 0.0f}; // 50 u < 2 m
    CHECK(BotBlocksSpawn(state, 1, {0.0f, 0.0f, 0.0f}, 0.0f));

    state.players[1].position = {0.0f, 100.0f, 0.0f}; // 100 u > 2 m
    CHECK_FALSE(BotBlocksSpawn(state, 1, {0.0f, 0.0f, 0.0f}, 0.0f));
}

TEST_CASE("FF-6 不抢拆包：人类正在拆包时 bot 不得进入 2 m") {
    WorldState state = MakeWorld(2);
    state.bomb.defusing = true;
    state.bomb.defuser = 0;
    state.bomb.position = {0.0f, 0.0f, 0.0f};
    state.players[1].position = {0.0f, 40.0f, 0.0f};
    CHECK(AllyIsDefusing(state, 0, 0.0f, 1));

    state.players[1].position = {0.0f, 120.0f, 0.0f}; // 3 m 外
    CHECK_FALSE(AllyIsDefusing(state, 0, 0.0f, 1));

    state.bomb.defusing = false;
    state.players[1].position = {0.0f, 10.0f, 0.0f};
    CHECK_FALSE(AllyIsDefusing(state, 0, 0.0f, 1));
}

TEST_CASE("MTC-13 分帧：每 tick ceil(N/4) 个 bot，10 tick 覆盖全部") {
    CHECK_EQ(AiFrameScheduler::BatchSize(0), 0);
    CHECK_EQ(AiFrameScheduler::BatchSize(4), 1);
    CHECK_EQ(AiFrameScheduler::BatchSize(10), 3);
    CHECK_EQ(AiFrameScheduler::BatchSize(6), 2);

    AiFrameScheduler scheduler;
    bool seen[10] = {};
    for (int i = 0; i < 10; ++i) {
        const uint8_t start = scheduler.NextBatch(10);
        for (uint8_t k = 0; k < AiFrameScheduler::BatchSize(10); ++k) {
            seen[(start + k) % 10] = true;
        }
    }
    for (bool ok : seen) {
        CHECK(ok);
    }
}

TEST_CASE("预算熔断：单帧超过 2 ms 记 overrun 并推迟") {
    AiFrameScheduler scheduler;
    CHECK_FALSE(scheduler.Trip(1500, 2000));
    CHECK_EQ(scheduler.budget_overruns, 0);
    CHECK(scheduler.Trip(2500, 2000));
    CHECK_EQ(scheduler.budget_overruns, 1);
    CHECK(scheduler.Trip(4000, 2000));
    CHECK_EQ(scheduler.budget_overruns, 2);
}
