#pragma once

#include <cstdint>

#include "recoil/movement/movement.h"

namespace recoil {

// 数值为 CS:GO 参考基线（AK-47 量级）；后续由 content/defs JSON 加载并校准。
struct WeaponDef {
    uint16_t damage = 36;
    uint16_t magazine = 30;
    uint16_t reserve = 90;
    uint16_t fire_interval_ticks = 6;
    uint16_t reload_ticks = 156;
    float armor_ratio = 0.775f;
    float range = 8192.0f;
};

struct RoundDef {
    uint16_t freeze_ticks = 192;
    uint16_t live_ticks = 1920;
    uint16_t end_ticks = 320;
    uint16_t max_score = 2;
    uint16_t start_money = 800;
    uint16_t max_money = 16000;
    uint16_t win_reward = 3250;
    uint16_t loss_reward = 1400;
    uint16_t kill_reward = 300;
    uint16_t armor_price = 650;
};

struct GrenadeDef {
    uint16_t fuse_ticks = 96;
    float throw_speed = 700.0f;
    float gravity = 800.0f;
    uint16_t he_damage = 57;
    float he_radius = 350.0f;
    float flash_radius = 512.0f;
    uint16_t flash_max_ticks = 128;
    float smoke_radius = 144.0f;
    uint16_t smoke_ticks = 640;
};

struct Content {
    WeaponDef rifle{};
    MovementDef movement{};
    RoundDef round{};
    GrenadeDef grenades{};
};

Content DefaultContent();

} // namespace recoil
