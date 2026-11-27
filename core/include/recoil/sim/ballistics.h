#pragma once

#include <array>
#include <cstdint>

#include "recoil/sim/state.h"

namespace recoil {

// 武器类别：只用于击杀奖励与购买分组（不参与伤害公式）。
enum class WeaponCategory : uint8_t {
    Rifle = 0,
    Pistol = 1,
    Sniper = 2,
    Smg = 3,
    Shotgun = 4,
    None = 255,
};

// 后坐力图案段数（SPEC-02 D2-2：30 段，超出用最后一段）。
constexpr int kRecoilPatternLength = 30;

// 停火多少 tick 后 shot_index 归零（64 tick = 1 s @64Hz，BAL-05）。
constexpr uint32_t kShotIndexResetTicks = 64;

// 第 i 发相对准星视角的累计偏移（度）；pitch 向上为正，yaw 向左为正。
struct RecoilPoint {
    float pitch = 0.0f;
    float yaw = 0.0f;
};

struct RecoilDef {
    std::array<RecoilPoint, kRecoilPatternLength> pattern{};
    // 视觉抬枪缩放（只改相机，不改射线；SPEC-02 §2.9 的 recoil_camera_scale）。
    float camera_scale = 1.0f;
};

// 扩散参数（度 / 度每秒）。
struct InaccuracyDef {
    float base = 0.0f;
    float per_shot = 0.0f;
    float max = 0.0f;
    float move_mult = 1.0f;
    float jump_mult = 1.0f;
    float duck_mult = 1.0f;
    float recovery_initial = 0.0f;
    float recovery_final = 0.0f;
    // 停火多少秒后才开始恢复（≈射击间隔×1.15）：连发窗口内不恢复，保证射速越快累积越高。
    float recovery_delay_s = 0.0f;
    float recovery_switch_s = 0.15f;
};

struct BallisticsDef {
    RecoilDef recoil{};
    InaccuracyDef inaccuracy{};
};

// 一把武器的弹道静态数据（由 content/defs/weapons.json 生成，见 ballistics_table.gen.h）。
struct BallisticsRow {
    WeaponId id = WeaponId::None;
    WeaponCategory category = WeaponCategory::None;
    BallisticsDef def{};
    uint16_t kill_award = 0;
};

// 查表；未知 id 返回 nullptr（调用方用 DefaultBallistics() 保底）。
const BallisticsRow* FindBallisticsRow(WeaponId id);

// 未知武器的零后坐力定义（"删掉 recoil 段即退回直线弹道"的回滚路径）。
const BallisticsDef& DefaultBallistics();

// 未知/无武器时的弹道（始终有效引用）。
const BallisticsDef& BallisticsFor(WeaponId id);

// 姿态扩散倍率：蹲 < 站(=1) < 走 < 跳（BAL-02 红线 3）。
float PostureInaccuracyScale(const PlayerState& player, const InaccuracyDef& def);

// 当前实际扩散（度）= (base + 连发累计) × 姿态倍率。
float EffectiveInaccuracy(const PlayerState& player, const InaccuracyDef& def);

// 一次开火产生的视线偏移（度）：图案累计 + 锥内随机；同时更新玩家状态。
struct AimOffset {
    float pitch = 0.0f;
    float yaw = 0.0f;
    float inaccuracy = 0.0f; // 本次开火使用的实际扩散（度）
    uint8_t shot_index = 0;
};

AimOffset ApplyShot(PlayerState& player, const BallisticsDef& def, uint64_t& rng);

// 每 tick 精度恢复与连发复位（在 Step 的武器计时阶段调用）。
void TickBallistics(PlayerState& player, const BallisticsDef& def, uint64_t tick);

// 击杀奖励：按武器查表（未登记武器回退到类别表）。
uint16_t KillAwardFor(WeaponId id);
uint16_t KillAwardForCategory(WeaponCategory category);

// 确定性随机（xorshift64*）：整个内核唯一随机源，禁止 rand()。
uint64_t NextRandom(uint64_t& rng);
float RandomUnit(uint64_t& rng); // [0,1)

} // namespace recoil
