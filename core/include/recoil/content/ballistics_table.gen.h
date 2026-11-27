// 生成文件，勿手改！
// 由 tools/weapons/gen_weapon_table.py 从 content/defs/weapons.json 生成；
// schema: recoil.weapons/1（tickrate 64）。
// 修改数值请改 JSON 后重新生成：python -X utf8 tools/weapons/gen_weapon_table.py
// clang-format off
#pragma once

#include <array>
#include <cstdint>

#include "recoil/sim/ballistics.h"

namespace recoil {

inline constexpr int kBallisticsTableSize = 5;

inline constexpr std::array<BallisticsRow, kBallisticsTableSize> kBallisticsTable = {{
    {WeaponId::AK47, WeaponCategory::Rifle,
     BallisticsDef{RecoilDef{std::array<RecoilPoint, kRecoilPatternLength>{{
            RecoilPoint{0.0f, 0.0f}, RecoilPoint{1.0f, 0.0f}, RecoilPoint{1.9f, 0.0f}, RecoilPoint{2.7f, 0.0f}, RecoilPoint{3.4f, 0.0f},
            RecoilPoint{4.0f, 0.0f}, RecoilPoint{4.5f, 0.0f}, RecoilPoint{4.9f, 0.0f}, RecoilPoint{5.2f, 0.0f}, RecoilPoint{5.4f, 0.0f},
            RecoilPoint{5.6f, 0.6f}, RecoilPoint{5.7f, 1.4f}, RecoilPoint{5.8f, 2.2f}, RecoilPoint{5.9f, 1.5f}, RecoilPoint{6.0f, 0.4f},
            RecoilPoint{6.0f, -0.7f}, RecoilPoint{5.9f, -1.9f}, RecoilPoint{5.8f, -1.4f}, RecoilPoint{5.7f, -0.5f}, RecoilPoint{5.5f, 0.6f},
            RecoilPoint{5.3f, 1.6f}, RecoilPoint{5.0f, 2.0f}, RecoilPoint{4.8f, 1.2f}, RecoilPoint{4.6f, -0.2f}, RecoilPoint{4.5f, -1.4f},
            RecoilPoint{4.4f, -2.2f}, RecoilPoint{4.4f, -1.3f}, RecoilPoint{4.5f, -0.3f}, RecoilPoint{4.6f, 0.8f}, RecoilPoint{4.8f, 1.2f},
                }}, 1.0f},
      InaccuracyDef{0.35f, 0.75f, 3.5f, 3.0f, 8.0f, 0.6f, 15.0f, 10.0f, 0.11f, 0.15f}},
      300},
    {WeaponId::M4A4, WeaponCategory::Rifle,
     BallisticsDef{RecoilDef{std::array<RecoilPoint, kRecoilPatternLength>{{
            RecoilPoint{0.0f, 0.0f}, RecoilPoint{0.7f, 0.0f}, RecoilPoint{1.3f, 0.0f}, RecoilPoint{1.9f, 0.0f}, RecoilPoint{2.4f, 0.0f},
            RecoilPoint{2.8f, 0.0f}, RecoilPoint{3.2f, 0.0f}, RecoilPoint{3.5f, 0.0f}, RecoilPoint{3.7f, 0.0f}, RecoilPoint{3.9f, 0.0f},
            RecoilPoint{4.0f, 0.4f}, RecoilPoint{4.1f, 0.9f}, RecoilPoint{4.1f, 1.2f}, RecoilPoint{4.1f, 0.7f}, RecoilPoint{4.0f, 0.1f},
            RecoilPoint{3.9f, -0.4f}, RecoilPoint{3.8f, -1.1f}, RecoilPoint{3.7f, -0.8f}, RecoilPoint{3.6f, -0.2f}, RecoilPoint{3.5f, 0.3f},
            RecoilPoint{3.4f, 0.9f}, RecoilPoint{3.3f, 1.1f}, RecoilPoint{3.2f, 0.6f}, RecoilPoint{3.1f, -0.1f}, RecoilPoint{3.0f, -0.8f},
            RecoilPoint{2.9f, -1.2f}, RecoilPoint{2.8f, -0.7f}, RecoilPoint{2.7f, -0.1f}, RecoilPoint{2.6f, 0.4f}, RecoilPoint{2.5f, 0.7f},
                }}, 1.0f},
      InaccuracyDef{0.3f, 0.7f, 3.2f, 3.0f, 8.0f, 0.6f, 15.0f, 10.0f, 0.11f, 0.15f}},
      300},
    {WeaponId::AWP, WeaponCategory::Sniper,
     BallisticsDef{RecoilDef{std::array<RecoilPoint, kRecoilPatternLength>{{
            RecoilPoint{0.0f, 0.0f}, RecoilPoint{0.2f, 0.0f}, RecoilPoint{0.4f, 0.0f}, RecoilPoint{0.6f, 0.0f}, RecoilPoint{0.8f, 0.0f},
            RecoilPoint{1.0f, 0.0f}, RecoilPoint{1.2f, 0.0f}, RecoilPoint{1.4f, 0.0f}, RecoilPoint{1.6f, 0.0f}, RecoilPoint{1.8f, 0.0f},
            RecoilPoint{2.0f, 0.1f}, RecoilPoint{2.2f, 0.2f}, RecoilPoint{2.4f, 0.3f}, RecoilPoint{2.6f, 0.2f}, RecoilPoint{2.8f, 0.1f},
            RecoilPoint{3.0f, -0.1f}, RecoilPoint{3.2f, -0.2f}, RecoilPoint{3.4f, -0.3f}, RecoilPoint{3.6f, -0.2f}, RecoilPoint{3.8f, -0.1f},
            RecoilPoint{4.0f, 0.1f}, RecoilPoint{4.2f, 0.2f}, RecoilPoint{4.4f, 0.1f}, RecoilPoint{4.6f, 0.0f}, RecoilPoint{4.8f, -0.1f},
            RecoilPoint{5.0f, -0.2f}, RecoilPoint{5.2f, -0.1f}, RecoilPoint{5.4f, 0.0f}, RecoilPoint{5.6f, 0.1f}, RecoilPoint{5.8f, 0.2f},
                }}, 1.0f},
      InaccuracyDef{0.05f, 0.8f, 8.0f, 12.0f, 20.0f, 0.5f, 15.0f, 10.0f, 1.5f, 0.15f}},
      100},
    {WeaponId::USP, WeaponCategory::Pistol,
     BallisticsDef{RecoilDef{std::array<RecoilPoint, kRecoilPatternLength>{{
            RecoilPoint{0.0f, 0.0f}, RecoilPoint{0.4f, 0.0f}, RecoilPoint{0.7f, 0.0f}, RecoilPoint{1.0f, 0.0f}, RecoilPoint{1.3f, 0.0f},
            RecoilPoint{1.6f, 0.0f}, RecoilPoint{1.9f, 0.0f}, RecoilPoint{2.2f, 0.0f}, RecoilPoint{2.5f, 0.0f}, RecoilPoint{2.8f, 0.0f},
            RecoilPoint{3.0f, 0.2f}, RecoilPoint{3.2f, 0.5f}, RecoilPoint{3.4f, 0.8f}, RecoilPoint{3.6f, 0.5f}, RecoilPoint{3.8f, 0.2f},
            RecoilPoint{3.9f, -0.2f}, RecoilPoint{4.0f, -0.6f}, RecoilPoint{4.1f, -0.4f}, RecoilPoint{4.2f, -0.1f}, RecoilPoint{4.3f, 0.2f},
            RecoilPoint{4.3f, 0.5f}, RecoilPoint{4.4f, 0.7f}, RecoilPoint{4.4f, 0.4f}, RecoilPoint{4.4f, 0.0f}, RecoilPoint{4.4f, -0.4f},
            RecoilPoint{4.4f, -0.7f}, RecoilPoint{4.4f, -0.4f}, RecoilPoint{4.4f, 0.0f}, RecoilPoint{4.4f, 0.3f}, RecoilPoint{4.4f, 0.5f},
                }}, 1.0f},
      InaccuracyDef{0.25f, 0.5f, 2.4f, 2.5f, 8.0f, 0.7f, 15.0f, 10.0f, 0.19f, 0.15f}},
      300},
    {WeaponId::Glock, WeaponCategory::Pistol,
     BallisticsDef{RecoilDef{std::array<RecoilPoint, kRecoilPatternLength>{{
            RecoilPoint{0.0f, 0.0f}, RecoilPoint{0.5f, 0.0f}, RecoilPoint{0.9f, 0.0f}, RecoilPoint{1.3f, 0.0f}, RecoilPoint{1.7f, 0.0f},
            RecoilPoint{2.1f, 0.0f}, RecoilPoint{2.5f, 0.0f}, RecoilPoint{2.9f, 0.0f}, RecoilPoint{3.2f, 0.0f}, RecoilPoint{3.5f, 0.0f},
            RecoilPoint{3.8f, 0.4f}, RecoilPoint{4.0f, 0.9f}, RecoilPoint{4.2f, 1.3f}, RecoilPoint{4.3f, 0.8f}, RecoilPoint{4.4f, 0.2f},
            RecoilPoint{4.5f, -0.4f}, RecoilPoint{4.5f, -1.0f}, RecoilPoint{4.5f, -0.6f}, RecoilPoint{4.5f, -0.1f}, RecoilPoint{4.5f, 0.4f},
            RecoilPoint{4.5f, 0.9f}, RecoilPoint{4.5f, 1.2f}, RecoilPoint{4.5f, 0.7f}, RecoilPoint{4.5f, 0.1f}, RecoilPoint{4.5f, -0.5f},
            RecoilPoint{4.5f, -1.1f}, RecoilPoint{4.5f, -0.7f}, RecoilPoint{4.5f, -0.1f}, RecoilPoint{4.5f, 0.4f}, RecoilPoint{4.5f, 0.8f},
                }}, 1.0f},
      InaccuracyDef{0.4f, 0.9f, 2.4f, 2.5f, 8.0f, 0.7f, 15.0f, 10.0f, 0.17f, 0.15f}},
      300},
}};

} // namespace recoil
// clang-format on
