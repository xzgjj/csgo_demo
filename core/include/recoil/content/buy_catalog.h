#pragma once

#include <cstdint>

#include "recoil/sim/state.h"

namespace recoil {

constexpr int kMaxBuyEntries = 32;

// 商品种类：武器走 WeaponId，装备与投掷物各有独立类型（避免 UI 猜测槽位）。
enum class BuyKind : uint8_t {
    Weapon = 0,
    Armor = 1,
    Helmet = 2,
    DefuseKit = 3,
    GrenadeHE = 4,
    GrenadeFlash = 5,
    GrenadeSmoke = 6,
};

enum class BuySlot : uint8_t {
    Primary = 0,
    Secondary = 1,
    Gear = 2,
    Grenade = 3,
};

// 阵营位：bit0=T，bit1=CT；sides=3 表示双方均可购买。
constexpr uint8_t kBuySideT = 1u;
constexpr uint8_t kBuySideCT = 2u;

struct BuyEntry {
    const char* id;   // 与 catalog JSON 的 weapon/item 名称一致
    uint8_t category; // 分类下标（0..5，对应 kBuyCategoryNames）
    BuyKind kind;
    WeaponId weapon;     // kind=Weapon 时有效
    GrenadeKind grenade; // kind=Grenade* 时有效
    BuySlot slot;
    uint16_t price;
    uint8_t limit; // 每回合上限（手雷可 2 颗）
    uint8_t sides; // kBuySideT | kBuySideCT
};

// 目录静态参数；价格/限购/阵营全部来自 content/defs/buy_catalog.json。
struct BuyCatalog {
    bool enabled;
    bool zone_required;
    uint16_t window_ticks; // Live 阶段额外宽限（冻结期始终可买）
    uint8_t max_refunds;   // 单回合退款上限
    uint8_t count;
    const BuyEntry* entries;
};

} // namespace recoil
