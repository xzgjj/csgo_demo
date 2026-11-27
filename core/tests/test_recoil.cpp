#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "doctest/doctest.h"
#include "recoil/content/ballistics_table.gen.h"
#include "recoil/content/content.h"
#include "recoil/sim/ballistics.h"
#include "recoil/sim/state.h"

namespace {

using recoil::BallisticsFor;
using recoil::EffectiveInaccuracy;
using recoil::InaccuracyDef;
using recoil::kRecoilPatternLength;
using recoil::PlayerState;
using recoil::WeaponId;

constexpr float kPi = 3.14159265358979323846f;
// 1 u = 1 英寸 = 0.0254 m（core/include/recoil/base/vec3.h）。
constexpr float kMetersPerUnit = 0.0254f;
constexpr float kDegToRad = kPi / 180.0f;

PlayerState StandingPlayer() {
    PlayerState player;
    player.on_ground = true;
    player.velocity = {};
    player.ducking = false;
    return player;
}

} // namespace

TEST_CASE("BAL-01 图案表 30 段且首段为零") {
    const recoil::BallisticsDef& ak = BallisticsFor(WeaponId::AK47);
    REQUIRE(static_cast<int>(ak.recoil.pattern.size()) == kRecoilPatternLength);
    CHECK(ak.recoil.pattern[0].pitch == doctest::Approx(0.0f));
    CHECK(ak.recoil.pattern[0].yaw == doctest::Approx(0.0f));
    CHECK(ak.recoil.camera_scale == doctest::Approx(1.0f));
}

TEST_CASE("BAL-01 AK 前 10 发垂直度 >=0.85，后 20 发左右交替") {
    const recoil::BallisticsDef& ak = BallisticsFor(WeaponId::AK47);
    float max_pitch = 0.0f;
    float max_yaw = 0.0f;
    for (int i = 0; i < 10; ++i) {
        const recoil::RecoilPoint point = ak.recoil.pattern[static_cast<size_t>(i)];
        max_pitch = std::max(max_pitch, point.pitch);
        max_yaw = std::max(max_yaw, std::fabs(point.yaw));
    }
    REQUIRE(max_pitch > 0.0f);
    // 垂直度 = 1 - |yaw| / pitch；前 10 发必须 >= 0.85（本数据为纯垂直 = 1.0）。
    CHECK(1.0f - max_yaw / max_pitch >= doctest::Approx(0.85f).epsilon(0.001f));

    bool saw_positive = false;
    bool saw_negative = false;
    for (int i = 10; i < kRecoilPatternLength; ++i) {
        const float yaw = ak.recoil.pattern[static_cast<size_t>(i)].yaw;
        saw_positive = saw_positive || yaw > 0.1f;
        saw_negative = saw_negative || yaw < -0.1f;
    }
    CHECK(saw_positive);
    CHECK(saw_negative);

    // 图案末段仍在合理范围（不会把准星抬出画面）。
    CHECK(ak.recoil.pattern[kRecoilPatternLength - 1].pitch <= doctest::Approx(6.0f));
}

TEST_CASE("BAL-01 枪族图案形状随后坐力递增而分离") {
    const float ak_pitch = BallisticsFor(WeaponId::AK47).recoil.pattern[9].pitch;
    const float m4_pitch = BallisticsFor(WeaponId::M4A4).recoil.pattern[9].pitch;
    const float awp_pitch = BallisticsFor(WeaponId::AWP).recoil.pattern[9].pitch;
    CHECK(ak_pitch > m4_pitch);  // AK 抬升更强
    CHECK(m4_pitch > awp_pitch); // 狙击单发图案几乎不抬
}

TEST_CASE("BAL-02 连发第 10 发扩散 >= 首发 3 倍") {
    const recoil::BallisticsDef& ak = BallisticsFor(WeaponId::AK47);
    const InaccuracyDef& def = ak.inaccuracy;
    PlayerState first = StandingPlayer();
    PlayerState tenth = StandingPlayer();
    for (int i = 1; i < 9; ++i) {
        tenth.inaccuracy = std::min(def.max, tenth.inaccuracy + def.per_shot);
    }
    const float first_cone = EffectiveInaccuracy(first, def);
    const float tenth_cone = EffectiveInaccuracy(tenth, def);
    REQUIRE(first_cone > 0.0f);
    CHECK(tenth_cone / first_cone >= doctest::Approx(3.0f).epsilon(0.01f));
}

TEST_CASE("BAL-02 姿态序 蹲 < 站 < 走 < 跳") {
    const InaccuracyDef& def = BallisticsFor(WeaponId::AK47).inaccuracy;
    PlayerState duck = StandingPlayer();
    duck.ducking = true;
    PlayerState stand = StandingPlayer();
    PlayerState walk = StandingPlayer();
    walk.velocity = {250.0f, 0.0f, 0.0f};
    PlayerState jump = StandingPlayer();
    jump.on_ground = false;

    const float duck_cone = EffectiveInaccuracy(duck, def);
    const float stand_cone = EffectiveInaccuracy(stand, def);
    const float walk_cone = EffectiveInaccuracy(walk, def);
    const float jump_cone = EffectiveInaccuracy(jump, def);

    CHECK(duck_cone <= stand_cone);
    CHECK(stand_cone < walk_cone);
    CHECK(walk_cone < jump_cone);
    CHECK(jump_cone / stand_cone >= doctest::Approx(6.0f).epsilon(0.01f));
    CHECK(walk_cone / stand_cone >= doctest::Approx(2.5f).epsilon(0.01f));
}

TEST_CASE("BAL-02 扩散有上限且不随连发无限增长") {
    const InaccuracyDef& def = BallisticsFor(WeaponId::AK47).inaccuracy;
    PlayerState player = StandingPlayer();
    recoil::AimOffset offset;
    uint64_t rng = 0x9E3779B97F4A7C15ull;
    const recoil::BallisticsDef& ak = BallisticsFor(WeaponId::AK47);
    for (int i = 0; i < 40; ++i) {
        offset = recoil::ApplyShot(player, ak, rng);
    }
    CHECK(player.inaccuracy <= def.max + 1.0e-5f);
    CHECK(offset.inaccuracy <= (def.max + def.base) * def.jump_mult);
}

TEST_CASE("BAL-03 30 m 首发落点偏差 <=25 cm，蹲伏优于站立") {
    const InaccuracyDef& def = BallisticsFor(WeaponId::AK47).inaccuracy;
    const float distance_u = 30.0f / kMetersPerUnit; // 30 m ≈ 1181 u
    PlayerState stand = StandingPlayer();
    PlayerState duck = StandingPlayer();
    duck.ducking = true;

    const float stand_cone_deg = EffectiveInaccuracy(stand, def);
    const float duck_cone_deg = EffectiveInaccuracy(duck, def);
    const float stand_lateral_m =
        std::tan(stand_cone_deg * kDegToRad) * distance_u * kMetersPerUnit;
    const float duck_lateral_m = std::tan(duck_cone_deg * kDegToRad) * distance_u * kMetersPerUnit;

    CHECK(stand_lateral_m <= doctest::Approx(0.25f).epsilon(0.001f));
    CHECK(duck_lateral_m <= stand_lateral_m);
}

TEST_CASE("BAL-03 随机锥只来自 state.rng，范围可复现") {
    const recoil::BallisticsDef& ak = BallisticsFor(WeaponId::AK47);
    PlayerState a = StandingPlayer();
    PlayerState b = StandingPlayer();
    uint64_t rng_a = 42ull;
    uint64_t rng_b = 42ull;
    for (int i = 0; i < 8; ++i) {
        const recoil::AimOffset offset_a = recoil::ApplyShot(a, ak, rng_a);
        const recoil::AimOffset offset_b = recoil::ApplyShot(b, ak, rng_b);
        CHECK(offset_a.pitch == doctest::Approx(offset_b.pitch));
        CHECK(offset_a.yaw == doctest::Approx(offset_b.yaw));
    }
    CHECK(rng_a == rng_b);
}

TEST_CASE("BAL-03 锥内偏移不超过当发实际扩散") {
    const recoil::BallisticsDef& ak = BallisticsFor(WeaponId::AK47);
    PlayerState player = StandingPlayer();
    uint64_t rng = 7ull;
    for (int i = 0; i < 30; ++i) {
        const recoil::AimOffset offset = recoil::ApplyShot(player, ak, rng);
        const recoil::RecoilPoint point =
            ak.recoil.pattern[static_cast<size_t>(std::min(i, kRecoilPatternLength - 1))];
        const float cone_pitch = offset.pitch - point.pitch;
        const float cone_yaw = offset.yaw - point.yaw;
        const float cone_length = std::sqrt(cone_pitch * cone_pitch + cone_yaw * cone_yaw);
        CHECK(cone_length <= offset.inaccuracy + 1.0e-3f);
    }
}
