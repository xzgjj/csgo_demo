#pragma once

#include <cstdint>

#include "recoil/sim/hitbox.h"

namespace recoil {

// 单发伤害的唯一计算入口（SPEC-04 §4.3）。所有模块必须走这里，禁止自行乘倍率。
struct DamageInput {
    uint16_t base = 0;           // 武器基础伤害
    float range_modifier = 1.0f; // 每 range_unit 的衰减底数
    float range_unit = 500.0f;
    float armor_ratio = 0.775f; // 有甲部位的伤害保留比例
    HitGroup group = HitGroup::Chest;
    float distance = 0.0f;          // 命中距离（units）
    uint16_t armor = 0;             // 目标剩余护甲
    bool helmet = false;            // 是否戴头盔
    float penetration_scale = 1.0f; // 穿透层数累计倍率（1.0 = 未穿墙）
};

struct DamageResult {
    uint16_t raw = 0;        // 乘完部位/衰减/穿透、未乘护甲的原始伤害
    uint16_t applied = 0;    // 实际伤害（未截断到剩余 HP）
    uint16_t armor_loss = 0; // 本发扣掉的护甲（基于 raw，不基于 applied）
    float group_mult = 1.0f;
    bool armor_absorbed = false; // 本发是否被护甲减免
};

// 部位倍率：头 4.0 / 腹 1.25 / 腿 0.75 / 其它 1.0（头有无头盔都是 4×，头盔只影响护甲）。
float HitGroupMultiplier(HitGroup group);

// 该部位是否受护甲保护：腿永远免甲；头仅头盔；其余看护甲值。
bool GroupArmorProtected(HitGroup group, bool helmet, uint16_t armor);

DamageResult ComputeDamage(const DamageInput& input);

// 受击方向：射手相对受害者视角的水平夹角，归一化到 (-180, 180]。
// 仅暴露角度，不向表现层泄露射手坐标（SPEC-07 DEP-02）。
float RelativeHitAngle(float attacker_x, float attacker_y, float target_x, float target_y,
                       float target_view_yaw);

} // namespace recoil
