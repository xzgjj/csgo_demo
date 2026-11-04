// 生成文件，勿手改！
// 由 tools/weapons/gen_weapon_table.py 从 content/defs/weapons.json 生成；
// schema: recoil.weapons/1（tickrate 64）。
// 修改数值请改 JSON 后重新生成：python -X utf8 tools/weapons/gen_weapon_table.py
// clang-format off
#pragma once

#include <array>
#include <cstdint>

#include "recoil/content/content.h"
#include "recoil/sim/state.h"

namespace recoil {

inline constexpr int kWeaponTableSize = 5;

// 一把武器的静态数据：WeaponId 枚举 + 展示名 + 内核数值 + 购买价格 + 是否副武器。
struct WeaponRow {
    WeaponId id;
    const char* name;
    WeaponDef def;
    uint16_t price;
    bool sidearm; // true=手枪（进副武器槽）
};

inline constexpr std::array<WeaponRow, kWeaponTableSize> kWeaponTable = {{
    {WeaponId::AK47, "AK-47",
     WeaponDef{36, 30, 90, 6, 160, 0.775f, 8192.0f, 0.98f, 500.0f, 1.0f}, 2700, false},
    {WeaponId::M4A4, "M4A4",
     WeaponDef{33, 30, 90, 6, 198, 0.7f, 8192.0f, 0.97f, 500.0f, 1.0f}, 3100, false},
    {WeaponId::AWP, "AWP",
     WeaponDef{115, 10, 30, 93, 237, 0.975f, 8192.0f, 0.99f, 500.0f, 4.0f}, 4750, false},
    {WeaponId::USP, "USP-S",
     WeaponDef{35, 12, 24, 11, 141, 0.505f, 8192.0f, 0.91f, 500.0f, 1.0f}, 200, true},
    {WeaponId::Glock, "Glock-18",
     WeaponDef{30, 20, 120, 10, 147, 0.47f, 8192.0f, 0.85f, 500.0f, 1.0f}, 200, true},
}};

} // namespace recoil
// clang-format on
