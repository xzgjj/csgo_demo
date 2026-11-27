#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "doctest/doctest.h"
#include "recoil/content/content.h"
#include "recoil/content/weapon_table.h"
#include "recoil/sim/ballistics.h"
#include "recoil/sim/step.h"

namespace {

using recoil::BallisticsFor;
using recoil::Command;
using recoil::CommandKind;
using recoil::Content;
using recoil::DefaultContent;
using recoil::EffectiveInaccuracy;
using recoil::EventKind;
using recoil::PlayerState;
using recoil::Step;
using recoil::StepResult;
using recoil::Team;
using recoil::WeaponId;
using recoil::WorldState;

constexpr float kSprayDistance = 300.0f; // 3 m（10 发短扫练习距离）
constexpr int kShots = 10;
constexpr int kPauseTicks = 26; // 0.4 s @64Hz

struct SprayReport {
    int hits = 0;
    float inacc_first = 0.0f;
    float inacc_last = 0.0f;
    float inacc_recovered = 0.0f;
    float aim_error = 0.0f;
    bool pause_first_hit = false;
};

WorldState MakeSprayRange() {
    WorldState state;
    state.player_count = 2;
    state.rng = 0x5EED1234u;
    for (int i = 0; i < 2; ++i) {
        PlayerState& p = state.players[static_cast<size_t>(i)];
        p.alive = true;
        p.on_ground = true;
        p.health = 100;
        p.primary_id = static_cast<uint8_t>(WeaponId::AK47);
        p.weapon.mag = 30;
        p.inaccuracy = 0.0f;
        p.shot_index = 0;
        p.weapon.reserve = 90;
        p.team = static_cast<uint8_t>(i == 0 ? Team::T : Team::CT);
        p.position = {static_cast<float>(i) * kSprayDistance, 0.0f, 0.0f};
        p.view_yaw = 0.0f;
    }
    return state;
}

bool HitThisStep(const StepResult& result) {
    for (int i = 0; i < result.events.count; ++i) {
        const recoil::Event& e = result.events.items[static_cast<size_t>(i)];
        if (e.kind == EventKind::DamageApplied && e.source == 0 && e.target == 1) {
            return true;
        }
    }
    return false;
}

float AimErrorThisStep(const StepResult& result, float cmd_yaw, float cmd_pitch) {
    for (int i = 0; i < result.events.count; ++i) {
        const recoil::Event& e = result.events.items[static_cast<size_t>(i)];
        if (e.kind == EventKind::WeaponFired) {
            const float pitch = cmd_pitch + e.f0;
            const float yaw = cmd_yaw + e.f1;
            return std::sqrt(pitch * pitch + yaw * yaw);
        }
    }
    return 0.0f;
}

bool FiredThisStep(const StepResult& result) {
    for (int i = 0; i < result.events.count; ++i) {
        if (result.events.items[static_cast<size_t>(i)].kind == EventKind::WeaponFired) {
            return true;
        }
    }
    return false;
}

// 固定种子跑 10 发：每发等射击冷却结束（CommandRejected 不计），再统计是否命中。
SprayReport RunSpray(const Content& content, bool compensate) {
    SprayReport report;
    WorldState state = MakeSprayRange();

    const auto inacc = [&state]() {
        return EffectiveInaccuracy(state.players[0], BallisticsFor(WeaponId::AK47).inaccuracy);
    };
    report.inacc_first = inacc();
    int fired = 0;
    for (int guard = 0; guard < 400 && fired < kShots; ++guard) {
        state.players[1].alive = true;
        state.players[1].health = 100;
        const float before = inacc();
        Command fire;
        fire.kind = CommandKind::Fire;
        fire.player = 0;
        if (compensate) {
            const auto& pattern = BallisticsFor(WeaponId::AK47).recoil.pattern;
            const auto& p = pattern[static_cast<size_t>(fired % 30)];
            fire.yaw_degrees = -p.yaw;
            fire.pitch_degrees = -p.pitch;
        }
        const StepResult result = Step(state, content, std::array<Command, 1>{fire});
        if (FiredThisStep(result)) {
            report.aim_error += AimErrorThisStep(result, fire.yaw_degrees, fire.pitch_degrees);
            ++fired;
            if (HitThisStep(result)) {
                ++report.hits;
            }
            report.inacc_last = before;
        }
        state = result.state;
    }
    for (int i = 0; i < kPauseTicks; ++i) {
        state.players[1].alive = true;
        state.players[1].health = 100;
        state = Step(state, content, std::array<Command, 0>{}).state;
    }
    report.inacc_recovered = inacc();
    state.players[1].alive = true;
    state.players[1].health = 100;
    Command last;
    last.kind = CommandKind::Fire;
    last.player = 0;
    const StepResult after = Step(state, content, std::array<Command, 1>{last});
    report.pause_first_hit = FiredThisStep(after) && HitThisStep(after);
    return report;
}

} // namespace

TEST_CASE("DEP-05 spray_trainer_test：前 10 发命中率固定种子可复现，停火 0.4s 扩散回落") {
    const Content content = DefaultContent();
    const SprayReport raw = RunSpray(content, false);
    const SprayReport practiced = RunSpray(content, true);

    CHECK_EQ(raw.hits, RunSpray(content, false).hits);
    CHECK_LT(practiced.aim_error, raw.aim_error);
    CHECK(raw.hits >= 1);
    CHECK_LE(raw.hits, kShots);
    CHECK_GT(raw.inacc_last, raw.inacc_first);
    CHECK_LT(raw.inacc_recovered, raw.inacc_last);

    const char* csv_path = std::getenv("RECOIL_SPRAY_CSV");
    if (csv_path != nullptr) {
        if (FILE* csv = std::fopen(csv_path, "w")) {
            std::fprintf(csv, "shots,raw_hits,raw_rate,practiced_hits,practiced_rate,"
                              "raw_error,practiced_error,inacc_first,inacc_last,"
                              "inacc_recovered,pause_first_hit\n");
            std::fprintf(csv, "%d,%d,%.3f,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d\n", kShots, raw.hits,
                         static_cast<double>(raw.hits) / kShots, practiced.hits,
                         static_cast<double>(practiced.hits) / kShots, raw.aim_error,
                         practiced.aim_error, raw.inacc_first, raw.inacc_last, raw.inacc_recovered,
                         raw.pause_first_hit ? 1 : 0);
            std::fclose(csv);
        }
    }
}
