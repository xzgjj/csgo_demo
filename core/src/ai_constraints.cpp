#include "recoil/ai/constraints.h"

#include <cmath>

#include "recoil/content/bot_table.gen.h"
#include "recoil/sim/state.h"

namespace recoil {
namespace {

// 1 米 ≈ 39.37 Unreal units（1 unit = 1 inch）。
constexpr float kUnitsPerMeter = 39.37f;
constexpr float kFireLineWindowM = 1.2f; // FF-1 横向容差
constexpr float kMaxFireLineAngleDeg = 8.0f;
constexpr float kPickupRangeM = 3.0f;  // FF-3
constexpr float kClaimFaceDeg = 45.0f; // FF-3 “面向包”
constexpr float kDefuseGuardM = 2.0f;  // FF-6

// 点到线段的有符号最近点参数 t ∈ [0,1]，返回投影点。
float ProjectOnSegment(const Vec3& p, const Vec3& a, const Vec3& b, Vec3* out) {
    const Vec3 ab = b - a;
    const float len2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
    if (len2 <= 1e-6f) {
        *out = a;
        return 0.0f;
    }
    const Vec3 ap = p - a;
    float t = (ap.x * ab.x + ap.y * ab.y + ap.z * ab.z) / len2;
    if (t < 0.0f) {
        t = 0.0f;
    } else if (t > 1.0f) {
        t = 1.0f;
    }
    *out = {a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t};
    return t;
}

float Distance(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// 平面朝向夹角（度）：把向量投影到 XY 平面，忽略高度差。
float PlanarAngleDeg(const Vec3& from, const Vec3& to, float yaw_deg) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 1e-6f) {
        return 0.0f;
    }
    const float dir_x = dx / len;
    const float dir_y = dy / len;
    const float yaw = yaw_deg * 3.14159265f / 180.0f;
    const float cos_angle = dir_x * std::cos(yaw) + dir_y * std::sin(yaw);
    const float clamped = cos_angle < -1.0f ? -1.0f : (cos_angle > 1.0f ? 1.0f : cos_angle);
    return std::acos(clamped) * 180.0f / 3.14159265f;
}

} // namespace

const BotDifficulty& DifficultyFor(uint8_t level) {
    const uint8_t index = level > 3 ? 3 : level;
    return content::kBotDifficulty[index];
}

bool AllyBlocksFireLine(const WorldState& state, uint8_t shooter, uint8_t target, uint8_t ally,
                        float* out_angle) {
    if (shooter >= state.player_count || target >= state.player_count ||
        ally >= state.player_count || ally == shooter) {
        return false;
    }
    const PlayerState& s = state.players[shooter];
    const PlayerState& t = state.players[target];
    const PlayerState& a = state.players[ally];
    Vec3 closest{};
    ProjectOnSegment(a.position, s.position, t.position, &closest);
    const float lateral = Distance(a.position, closest);
    const float angle = PlanarAngleDeg(s.position, a.position, s.view_yaw);
    if (out_angle != nullptr) {
        *out_angle = angle;
    }
    return lateral <= kFireLineWindowM * kUnitsPerMeter && angle < kMaxFireLineAngleDeg;
}

bool AllyClaimsBomb(const WorldState& state, uint8_t bot, uint8_t human_ally, float pickup_range) {
    if (bot >= state.player_count || human_ally >= state.player_count || bot == human_ally) {
        return false;
    }
    const PlayerState& human = state.players[human_ally];
    const float range = pickup_range > 0.0f ? pickup_range : kPickupRangeM * kUnitsPerMeter;
    const float dist = Distance(human.position, state.bomb.position);
    if (dist > range) {
        return false;
    }
    const float face = PlanarAngleDeg(human.position, state.bomb.position, human.view_yaw);
    return face <= kClaimFaceDeg;
}

bool BotBlocksSpawn(const WorldState& state, uint8_t bot, const Vec3& spawn_center, float radius) {
    if (bot >= state.player_count) {
        return false;
    }
    const float dist = Distance(state.players[bot].position, spawn_center);
    return dist <= (radius > 0.0f ? radius : 2.0f * kUnitsPerMeter);
}

bool AllyIsDefusing(const WorldState& state, uint8_t human_ally, float guard_range, uint8_t bot) {
    if (bot >= state.player_count || human_ally >= state.player_count) {
        return false;
    }
    if (!state.bomb.defusing || state.bomb.defuser != human_ally) {
        return false;
    }
    const float range = guard_range > 0.0f ? guard_range : kDefuseGuardM * kUnitsPerMeter;
    return Distance(state.players[bot].position, state.bomb.position) <= range;
}

uint8_t AiFrameScheduler::BatchSize(uint8_t total) {
    if (total == 0) {
        return 0;
    }
    return static_cast<uint8_t>((total + 3) / 4);
}

uint8_t AiFrameScheduler::NextBatch(uint8_t total) {
    if (total == 0) {
        return 0;
    }
    const uint8_t start = cursor;
    const uint8_t batch = BatchSize(total);
    cursor = static_cast<uint8_t>((cursor + batch) % total);
    return start;
}

bool AiFrameScheduler::Trip(uint16_t elapsed_us, uint16_t budget_us) {
    if (elapsed_us <= budget_us) {
        return false;
    }
    ++budget_overruns;
    return true;
}

} // namespace recoil
