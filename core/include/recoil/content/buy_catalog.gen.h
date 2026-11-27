// 生成文件，勿手改！
// 由 tools/buy/gen_buy_catalog.py 从 content/defs/buy_catalog.json 生成。
// 修改价格/限购/阵营请改 JSON 后重新生成：python -X utf8 tools/buy/gen_buy_catalog.py
// clang-format off
#pragma once

#include "recoil/content/buy_catalog.h"

namespace recoil {

inline constexpr int kBuyEntryCount = 11;
inline constexpr uint16_t kBuyWindowTicks = 320;
inline constexpr bool kBuyZoneRequired = false;
inline constexpr uint8_t kBuyMaxRefunds = 3;
inline constexpr const char* kBuyCategoryNames[6] = {"手枪", "冲锋枪", "步枪", "重型", "装备", "投掷物"};

inline constexpr BuyEntry kBuyEntries[kBuyEntryCount] = {
    {"glock_18", 0, BuyKind::Weapon, WeaponId::Glock, GrenadeKind::HE, BuySlot::Secondary, 200, 1, 1},
    {"usp_s", 0, BuyKind::Weapon, WeaponId::USP, GrenadeKind::HE, BuySlot::Secondary, 200, 1, 2},
    {"ak47", 2, BuyKind::Weapon, WeaponId::AK47, GrenadeKind::HE, BuySlot::Primary, 2700, 1, 1},
    {"m4a4", 2, BuyKind::Weapon, WeaponId::M4A4, GrenadeKind::HE, BuySlot::Primary, 3100, 1, 2},
    {"awp", 3, BuyKind::Weapon, WeaponId::AWP, GrenadeKind::HE, BuySlot::Primary, 4750, 1, 3},
    {"kevlar", 4, BuyKind::Armor, WeaponId::None, GrenadeKind::HE, BuySlot::Gear, 650, 1, 3},
    {"helmet", 4, BuyKind::Helmet, WeaponId::None, GrenadeKind::HE, BuySlot::Gear, 1000, 1, 3},
    {"defuse_kit", 4, BuyKind::DefuseKit, WeaponId::None, GrenadeKind::HE, BuySlot::Gear, 400, 1, 2},
    {"he", 5, BuyKind::GrenadeHE, WeaponId::None, GrenadeKind::HE, BuySlot::Grenade, 300, 1, 3},
    {"flash", 5, BuyKind::GrenadeFlash, WeaponId::None, GrenadeKind::Flash, BuySlot::Grenade, 200, 2, 3},
    {"smoke", 5, BuyKind::GrenadeSmoke, WeaponId::None, GrenadeKind::Smoke, BuySlot::Grenade, 300, 1, 3},
};

} // namespace recoil
