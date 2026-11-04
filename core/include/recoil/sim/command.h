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
};

enum class BuyItem : uint8_t {
    Armor = 0,
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
};

} // namespace recoil
