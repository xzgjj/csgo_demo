#include "recoil/sim/hitbox.h"

#include <algorithm>
#include <cmath>

namespace recoil {
namespace {

constexpr float kParallel = 1.0e-6f;
constexpr float kPi = 3.14159265358979323846f;

struct HitBox {
    float half_x;
    float half_y;
    float z_min;
    float z_max;
    HitGroup group;
};

// 站立/蹲伏两套盒；与玩家 32×32×72（蹲 54）碰撞体一致，避免“看得见打不到”。
constexpr HitBox kStanding[] = {
    {4.0f, 4.0f, 65.0f, 72.0f, HitGroup::Head},
    {16.0f, 10.0f, 48.0f, 65.0f, HitGroup::Chest},
    {15.0f, 12.0f, 32.0f, 48.0f, HitGroup::Stomach},
    {14.0f, 14.0f, 0.0f, 32.0f, HitGroup::Legs},
};

constexpr HitBox kDucking[] = {
    {4.0f, 4.0f, 48.0f, 54.0f, HitGroup::Head},
    {16.0f, 10.0f, 34.0f, 48.0f, HitGroup::Chest},
    {15.0f, 12.0f, 20.0f, 34.0f, HitGroup::Stomach},
    {14.0f, 14.0f, 0.0f, 20.0f, HitGroup::Legs},
};

// 局部空间轴对齐盒求交，返回进入比例（t 与 dir 同尺度）。
bool RayVsLocalBox(const Vec3& origin, const Vec3& dir, const HitBox& box, float max_range,
                   float& t_out) {
    float enter = 0.0f;
    float exit = max_range;
    const float o[3] = {origin.x, origin.y, origin.z};
    const float d[3] = {dir.x, dir.y, dir.z};
    const float lo[3] = {-box.half_x, -box.half_y, box.z_min};
    const float hi[3] = {box.half_x, box.half_y, box.z_max};
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(d[i]) < kParallel) {
            if (o[i] < lo[i] || o[i] > hi[i]) {
                return false;
            }
            continue;
        }
        const float inv = 1.0f / d[i];
        float t0 = (lo[i] - o[i]) * inv;
        float t1 = (hi[i] - o[i]) * inv;
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        enter = std::max(enter, t0);
        exit = std::min(exit, t1);
        if (enter > exit) {
            return false;
        }
    }
    t_out = enter;
    return true;
}

} // namespace

PlayerHit TracePlayerHitboxes(const PlayerState& target, const Vec3& origin, const Vec3& dir,
                              float max_range) {
    PlayerHit best;
    const float yaw = target.view_yaw * (kPi / 180.0f);
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    const Vec3 d = origin - target.position;
    // 旋转到玩家局部坐标（绕 -yaw），盒仍轴对齐。
    const Vec3 local_origin{d.x * c + d.y * s, -d.x * s + d.y * c, d.z};
    const Vec3 local_dir{dir.x * c + dir.y * s, -dir.x * s + dir.y * c, dir.z};
    const HitBox* boxes = target.ducking ? kDucking : kStanding;
    const int count = 4;
    for (int i = 0; i < count; ++i) {
        float t = 0.0f;
        if (!RayVsLocalBox(local_origin, local_dir, boxes[i], max_range, t)) {
            continue;
        }
        if (!best.hit || t < best.t) {
            best.hit = true;
            best.t = t;
            best.group = boxes[i].group;
            best.point = origin + dir * t;
        }
    }
    return best;
}

} // namespace recoil
