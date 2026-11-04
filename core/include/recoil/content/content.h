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
    float range_modifier = 1.0f;
    float range_modifier_unit = 500.0f;
    float scope_magnification = 1.0f; // 1=无倍镜；AWP=4x
};

struct RoundDef {
    uint16_t freeze_ticks = 192;
    uint16_t live_ticks = 7360; // 115s @64tick（CS:GO 回合时长）
    uint16_t end_ticks = 320;
    uint16_t max_score = 13;          // MR12：先到 13 胜
    uint16_t spawn_armor = 100;       // 匹配局出生默认护甲
    bool spawn_helmet = true;         // 匹配局出生默认头盔
    float bot_aim_error_deg = 2.4f;   // 机器人瞄准误差上限（真实化）
    uint16_t bot_reaction_ticks = 16; // 首次看到敌人后开火延迟（250ms@64tick）
    uint16_t start_money = 800;
    uint16_t max_money = 16000;
    uint16_t win_reward = 3250;
    uint16_t loss_reward = 1400;
    uint16_t kill_reward = 300;
    uint16_t armor_price = 650;
    uint16_t helmet_price = 1000;
    uint16_t defuse_kit_price = 400;
    uint16_t plant_reward = 300;
    uint16_t defuse_reward = 300;
    uint16_t bomb_ticks = 2560;
    uint16_t defuse_ticks = 640;
    uint16_t defuse_kit_ticks = 320;
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
