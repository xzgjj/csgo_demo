#pragma once

#include <array>
#include <cstdint>

#include "recoil/base/vec3.h"
#include "recoil/sim/match_types.h"

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
    // 兼容字段：按“当前侧”累计的比分（每次换边与 score_team 双向同步，旧调用方仍可读）。
    uint16_t score_t = 0;
    uint16_t score_ct = 0;
    uint8_t winner = 0; // 0=无, 1=T, 2=CT（按 side，事件/HUD 使用）
    // SPEC-03 赛制状态（仅在 match.enabled 时被 TickMatch/MatchEndRound 维护）。
    std::array<uint16_t, 2> score_team{{0, 0}}; // 真源：按 team_id 记分，换边不变
    std::array<uint8_t, 2> side{{static_cast<uint8_t>(Team::T), static_cast<uint8_t>(Team::CT)}};
    std::array<uint8_t, 2> loss_streak{{1, 1}}; // 半场/加时重置为 1，禁止清零
    uint8_t half = 1;                           // 1=上半场, 2=下半场
    uint8_t overtime = 0;                       // 0=常规时间, 1..overtime_cap=第 n 个加时
    uint8_t ot_half = 0;                        // 加时内 1/2 半场（每半 ot_rounds_per_half 回合）
    uint8_t ot_rounds_in_half = 0;              // 当前加时半场已打回合数
    uint8_t end_reason = 0;                     // RoundEndReason
    uint16_t buy_ticks_left = 0;
    bool buy_closed = false;
    std::array<uint8_t, 2> timeout_left{{3, 3}}; // 每队剩余战术暂停
    uint16_t timeout_ticks_left = 0;
    uint8_t resume_phase = 0; // 暂停结束回到的阶段（RoundPhase 数值）
    bool halves_done = false;
    uint16_t round_ticks_left = 0; // Live 阶段的回合计时（容器）
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
    uint8_t team = 0;    // 当前阵营（Team）
    uint8_t team_id = 0; // 队伍身份（0/1），换边不变
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
    // SPEC-06 购买态（全部进 HashState，保证确定性）：
    // grenade_counts = 手雷库存（HE/Flash/Smoke），购买上限与投掷消耗都读这里；
    // bought_counts[entry] = 本回合各条目购买次数（退款不清零，防刷限购）；
    // buy_fire_marks[slot] = 购买时 fire_count 快照（退款“未使用”判定）；
    // refunded_mask = 本回合已退款条目位，退款后禁再买；round/last_buy_mask = 本回合/上回合购买位。
    std::array<uint8_t, 3> grenade_counts{{0, 0, 0}};
    std::array<uint8_t, 32> bought_counts{};
    std::array<uint16_t, 2> buy_fire_marks{{0, 0}};
    uint8_t refund_count = 0;
    uint32_t refunded_mask = 0;
    uint16_t round_buy_mask = 0;
    uint16_t last_buy_mask = 0;
};

// 视线备忘录：0=本 tick 未算，1=可见，2=被墙/烟雾遮挡。
// 只由位置派生，不进 HashState；tick 变化即整体失效（UpdateReactions 每 tick 复位一次）。
// 作用：同一 tick 内 reaction 与 bot 对同一对玩家最多各算一次 trace，且结论一致。
constexpr uint64_t kVisibilityInvalidTick = 0xFFFFFFFFFFFFFFFFull;

struct VisibilityMemo {
    uint64_t tick = kVisibilityInvalidTick;
    std::array<std::array<uint8_t, kMaxPlayers>, kMaxPlayers> cells{};
};

struct WorldState {
    uint64_t tick = 0;
    uint64_t rng = 0x9E3779B97F4A7C15ull;
    uint8_t player_count = 0;
    // SPEC-06 购买区（地图 layout 标 buy_zone 后由 host 注入）；未注入时按 zone_required=false
    // 放行。
    bool buy_zone_set = false;
    Vec3 buy_zone_min{};
    Vec3 buy_zone_max{};
    MatchRules match{}; // 赛制参数（默认 enabled=false → 旧行为）
    // 出生点由地图注入（host 从盒子清单 kind=spawn 读取）；未注入时回退 de_ash 常量，
    // 保证旧测试/无地图场景仍可运行。x/y 为 sim 平面坐标。
    Vec3 spawn_t{};
    Vec3 spawn_ct{};
    bool spawn_from_map = false;
    RoundState round{};
    BombState bomb{};
    SmokeZone smoke{};
    VisibilityMemo visibility{};
    std::array<GrenadeState, kMaxGrenades> grenades{};
    std::array<GroundWeapon, kMaxDrops> ground_weapons{};
    std::array<PlayerState, kMaxPlayers> players{};
};

} // namespace recoil
