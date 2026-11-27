#pragma once

#include <cstdint>

#include "recoil/sim/state.h"

namespace recoil {

enum class CommandKind : uint8_t {
    Fire,
    Reload,
    Move,
    Buy,
    Throw,
    Plant,
    Defuse,
    Stop,
    // 新增命令追加在末尾，保持既有数值稳定（场景/脚本按名字调用，但避免不必要变动）。
    BuyWeapon,
    SwitchWeapon,
    Pickup,
    // SPEC-06：目录购买 / 退款 / 重购（与旧 Buy/BuyWeapon 并存，便于回滚）。
    BuyEntry,
    Refund,
    Rebuy,
};

enum class BuyItem : uint8_t {
    Armor = 0,
    Helmet = 1,
    DefuseKit = 2,
};

struct Command {
    CommandKind kind = CommandKind::Stop;
    uint8_t player = 0;
    float yaw_degrees = 0.0f;
    float pitch_degrees = 0.0f;
    // Move 使用：forward/side ∈ [-1,1]；jump/duck 为按住状态。
    float move_forward = 0.0f;
    float move_side = 0.0f;
    bool jump = false;
    bool duck = false;
    BuyItem item = BuyItem::Armor;
    GrenadeKind grenade = GrenadeKind::HE;
    WeaponId weapon_id = WeaponId::None; // BuyWeapon 使用
    uint8_t slot = 0;                    // SwitchWeapon 使用：0=主武器，1=副武器
    uint8_t entry = 0;                   // BuyEntry/Refund 使用：catalog 下标
};

} // namespace recoil
