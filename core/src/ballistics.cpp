#include "recoil/sim/ballistics.h"

#include <algorithm>
#include <cmath>

#include "recoil/content/ballistics_table.gen.h"

namespace recoil {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kTickDt = 1.0f / 64.0f;
// 视觉回正速率（1/s）：快于精度恢复，让准星 200ms 内基本归位。
constexpr float kRecoilDecayPerSecond = 8.0f;
// 低于该水平速度视为静止（u/s；走 130、跑 250）。
constexpr float kMoveSpeedThreshold = 20.0f;

const BallisticsDef kDefaultBallistics{};

} // namespace

const BallisticsRow* FindBallisticsRow(WeaponId id) {
    for (const BallisticsRow& row : kBallisticsTable) {
        if (row.id == id) {
            return &row;
        }
    }
    return nullptr;
}

const BallisticsDef& DefaultBallistics() {
    return kDefaultBallistics;
}

const BallisticsDef& BallisticsFor(WeaponId id) {
    const BallisticsRow* row = FindBallisticsRow(id);
    return row != nullptr ? row->def : kDefaultBallistics;
}

float PostureInaccuracyScale(const PlayerState& player, const InaccuracyDef& def) {
    if (!player.on_ground) {
        return def.jump_mult;
    }
    const float horizontal_speed =
        std::sqrt(player.velocity.x * player.velocity.x + player.velocity.y * player.velocity.y);
    if (horizontal_speed > kMoveSpeedThreshold) {
        return def.move_mult;
    }
    if (player.ducking) {
        return def.duck_mult;
    }
    return 1.0f;
}

float EffectiveInaccuracy(const PlayerState& player, const InaccuracyDef& def) {
    const float accumulated = def.base + player.inaccuracy;
    return accumulated * PostureInaccuracyScale(player, def);
}

uint64_t NextRandom(uint64_t& rng) {
    uint64_t x = rng;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng = x;
    return x * 0x2545F4914F6CDD1Dull;
}

float RandomUnit(uint64_t& rng) {
    // 取高 53 位映射到 [0,1)，与 double 精度对齐。
    return static_cast<float>(NextRandom(rng) >> 11) * (1.0f / 9007199254740992.0f);
}

AimOffset ApplyShot(PlayerState& player, const BallisticsDef& def, uint64_t& rng) {
    const int index = std::min(static_cast<int>(player.shot_index), kRecoilPatternLength - 1);
    const RecoilPoint point = def.recoil.pattern[static_cast<size_t>(index)];
    const float inaccuracy = EffectiveInaccuracy(player, def.inaccuracy);

    // 均匀圆盘采样：半径按 sqrt 分布，角度均匀；最大偏移 = inaccuracy（度）。
    const float radius = inaccuracy * std::sqrt(RandomUnit(rng));
    const float theta = 2.0f * kPi * RandomUnit(rng);
    const float cone_pitch = radius * std::sin(theta);
    const float cone_yaw = radius * std::cos(theta);

    player.recoil_pitch = point.pitch;
    player.recoil_yaw = point.yaw;
    if (player.shot_index < 255) {
        ++player.shot_index;
    }
    player.inaccuracy = std::min(def.inaccuracy.max, player.inaccuracy + def.inaccuracy.per_shot);

    AimOffset offset;
    offset.pitch = point.pitch + cone_pitch;
    offset.yaw = point.yaw + cone_yaw;
    offset.inaccuracy = inaccuracy;
    offset.shot_index = static_cast<uint8_t>(index);
    return offset;
}

void TickBallistics(PlayerState& player, const BallisticsDef& def, uint64_t tick) {
    const uint32_t age = static_cast<uint32_t>(tick) - player.last_fire_tick;
    if (age > kShotIndexResetTicks) {
        player.shot_index = 0;
    }
    if (player.inaccuracy > 0.0f) {
        const float since_fire = static_cast<float>(age) * kTickDt;
        // 连发窗口内不恢复（否则射速间隔的恢复量会吃掉 per_shot，永远不累积）。
        if (since_fire > def.inaccuracy.recovery_delay_s) {
            const float since_delay = since_fire - def.inaccuracy.recovery_delay_s;
            // 两段恢复：停火后前 recovery_switch_s 用 initial（更快），之后用 final。
            const float rate = since_delay < def.inaccuracy.recovery_switch_s
                                   ? def.inaccuracy.recovery_initial
                                   : def.inaccuracy.recovery_final;
            player.inaccuracy = std::max(0.0f, player.inaccuracy - rate * kTickDt);
        }
    }
    // 视觉回正（射线只认开火瞬间的图案，故这里不影响落点）。
    const float decay = std::exp(-kRecoilDecayPerSecond * kTickDt);
    player.recoil_pitch *= decay;
    player.recoil_yaw *= decay;
    if (std::fabs(player.recoil_pitch) < 1.0e-4f) {
        player.recoil_pitch = 0.0f;
    }
    if (std::fabs(player.recoil_yaw) < 1.0e-4f) {
        player.recoil_yaw = 0.0f;
    }
}

uint16_t KillAwardFor(WeaponId id) {
    const BallisticsRow* row = FindBallisticsRow(id);
    return row != nullptr ? row->kill_award : 0;
}

uint16_t KillAwardForCategory(WeaponCategory category) {
    switch (category) {
    case WeaponCategory::Rifle:
    case WeaponCategory::Pistol:
        return 300;
    case WeaponCategory::Smg:
        return 600;
    case WeaponCategory::Shotgun:
        return 900;
    case WeaponCategory::Sniper:
        return 100;
    default:
        return 0;
    }
}

} // namespace recoil
