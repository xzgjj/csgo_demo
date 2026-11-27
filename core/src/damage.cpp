#include "recoil/sim/damage.h"

#include <algorithm>
#include <cmath>

namespace recoil {

float HitGroupMultiplier(HitGroup group) {
    switch (group) {
    case HitGroup::Head:
        return 4.0f;
    case HitGroup::Stomach:
        return 1.25f;
    case HitGroup::Legs:
        return 0.75f;
    case HitGroup::Chest:
    case HitGroup::None:
    default:
        return 1.0f;
    }
}

bool GroupArmorProtected(HitGroup group, bool helmet, uint16_t armor) {
    if (group == HitGroup::Legs || group == HitGroup::None) {
        return false; // 腿永远免甲
    }
    if (armor == 0) {
        return false;
    }
    if (group == HitGroup::Head) {
        return helmet; // 头只认头盔
    }
    return true;
}

DamageResult ComputeDamage(const DamageInput& input) {
    DamageResult result;
    result.group_mult = HitGroupMultiplier(input.group);
    const float unit = input.range_unit > 0.0f ? input.range_unit : 500.0f;
    const float falloff = std::pow(input.range_modifier, input.distance / unit);
    const float penetration = input.penetration_scale > 0.0f ? input.penetration_scale : 0.0f;
    const float raw = static_cast<float>(input.base) * result.group_mult * falloff * penetration;
    result.raw = static_cast<uint16_t>(std::lround(raw));

    result.armor_absorbed = GroupArmorProtected(input.group, input.helmet, input.armor);
    const float final_raw = result.armor_absorbed ? raw * input.armor_ratio : raw;
    result.applied = static_cast<uint16_t>(std::lround(final_raw));

    if (result.armor_absorbed) {
        // 护甲损耗基于 raw（红线 3），且不超过剩余护甲。
        const float loss = raw * (1.0f - input.armor_ratio) * 0.5f;
        result.armor_loss = static_cast<uint16_t>(std::min<float>(
            static_cast<float>(input.armor), static_cast<float>(std::lround(loss))));
    }
    return result;
}

float RelativeHitAngle(float attacker_x, float attacker_y, float target_x, float target_y,
                       float target_view_yaw) {
    constexpr float kRadToDeg = 57.2957795f;
    const float raw =
        std::atan2(attacker_y - target_y, attacker_x - target_x) * kRadToDeg - target_view_yaw;
    return std::fmod(raw + 540.0f, 360.0f) - 180.0f;
}

} // namespace recoil
