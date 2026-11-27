#pragma once

#include <array>
#include <cstdint>

#include "recoil/base/vec3.h"

namespace recoil {

constexpr int kMaxPlayers = 10;

enum class Team : uint8_t {
    None = 0,
    T = 1,
    CT = 2,
};

// 武器 id（与 content/defs/weapons.json 的 enum 字段一一对应，由生成器写入常量表）。
// None = 未持武器：内核回退到 Content::rifle 的单一定义（兼容旧状态/测试）。
enum class WeaponId : uint8_t {
    None = 0,
    AK47 = 1,
    M4A4 = 2,
    AWP = 3,
    USP = 4,
    Glock = 5,
};

// 武器槽数量：0=主武器、1=副武器。weapon 字段始终描述当前激活槽。
constexpr int kWeaponSlotCount = 2;

enum class RoundPhase : uint8_t {
    Freeze = 0,
    Live = 1,
    RoundEnd = 2,
    MatchEnd = 3,
};

enum class GrenadeKind : uint8_t {
    HE = 0,
    Flash = 1,
    Smoke = 2,
};

struct GrenadeState {
    bool active = false;
    uint8_t kind = 0;
    uint8_t owner = 0;
    Vec3 position{};
    Vec3 velocity{};
    uint16_t fuse_ticks = 0;
};

struct SmokeZone {
    bool active = false;
    Vec3 position{};
    float radius = 144.0f;
    uint16_t ticks_left = 0;
};

constexpr int kMaxGrenades = 8;

struct BombState {
    bool planted = false;
    bool defused = false;
    bool exploded = false;
    bool defusing = false;
    uint16_t timer_ticks = 0;
    uint16_t defuse_progress = 0;
    uint8_t carrier = 0xFF;
    uint8_t defuser = 0xFF;
    Vec3 position{};
};

struct RoundState {
    RoundPhase phase = RoundPhase::Freeze;
    uint16_t phase_ticks = 0;
    uint16_t number = 1;
    uint16_t score_t = 0;
    uint16_t score_ct = 0;
    uint8_t winner = 0; // 0=无, 1=T, 2=CT
};

struct WeaponState {
    uint16_t mag = 0;
    uint16_t reserve = 0;
    uint16_t cooldown_ticks = 0;
    uint16_t reload_ticks_left = 0;
    bool reloading = false;
};

// 地面掉落武器（模块 6）：击杀后主武器留在死亡位置，存活玩家靠近可拾取。
struct GroundWeapon {
    bool active = false;
    uint8_t weapon_id = 0; // WeaponId
    Vec3 position{};
    uint16_t mag = 0;
    uint16_t reserve = 0;
};

constexpr int kMaxDrops = 8;

struct PlayerState {
    bool alive = false;
    bool helmet = false;
    bool on_ground = false;
    bool ducking = false;
    bool jump_queued = false;
    bool jump_held = false;
    uint8_t team = 0; // Team
    uint16_t health = 0;
    uint16_t armor = 0;
    uint16_t money = 0;
    uint16_t kills = 0;
    uint16_t blind_ticks = 0;
    bool has_bomb = false;
    bool has_defuser = false;
    float wish_forward = 0.0f;
    float wish_side = 0.0f;
    Vec3 position{};
    Vec3 velocity{};
    // 双武器槽：primary_id/secondary_id 为两槽武器，active_slot 指向当前激活槽；
    // weapon 描述激活槽状态，sidearm 保存另一槽状态（切枪时两者交换）。
    uint8_t primary_id = 0;   // WeaponId（主武器槽）
    uint8_t secondary_id = 0; // WeaponId（副武器槽）
    uint8_t active_slot = 0;
    WeaponState weapon{};
    WeaponState sidearm{};
    // 机器人视觉反应延迟状态（Step 每 tick 更新、参与 HashState；确定性，无时钟/随机）：
    // reaction_target = 当前连续看到的敌人索引（0xFF=无），reaction_ticks = 连续看到的 tick 数。
    uint8_t reaction_target = 0xFF;
    uint16_t reaction_ticks = 0;
    // 模块 5：最近一次 Fire/Move 命令的朝向（表现层让枪口/角色朝向一致），进 HashState。
    float view_yaw = 0.0f;
    // 模块 5：本场累计开火数与最后一次开火 tick（0=未开火），进 HashState。
    uint16_t fire_count = 0;
    uint32_t last_fire_tick = 0;
    // SPEC-02 弹道状态（进 HashState，保持确定性）：
    // shot_index = 本次连发的第几发（停火 kShotIndexResetTicks 后归零）；
    // recoil_pitch/yaw = 当前视觉/射线共用的图案偏移（度），停火指数回正；
    // inaccuracy = 连发累计的额外扩散（度），按两段速率恢复。
    uint8_t shot_index = 0;
    float recoil_pitch = 0.0f;
    float recoil_yaw = 0.0f;
    float inaccuracy = 0.0f;
};

struct WorldState {
    uint64_t tick = 0;
    uint64_t rng = 0x9E3779B97F4A7C15ull;
    uint8_t player_count = 0;
    RoundState round{};
    BombState bomb{};
    SmokeZone smoke{};
    std::array<GrenadeState, kMaxGrenades> grenades{};
    std::array<GroundWeapon, kMaxDrops> ground_weapons{};
    std::array<PlayerState, kMaxPlayers> players{};
};

} // namespace recoil
