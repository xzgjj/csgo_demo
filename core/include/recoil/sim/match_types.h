#pragma once

#include <array>
#include <cstdint>

namespace recoil {

// 对局模式：索引顺序必须与 content/defs/rules.json 的 modes 顺序一致（生成器校验）。
enum class ModeId : uint8_t {
    Duel1v1 = 0,
    Squad3v3 = 1,
    Comp5v5 = 2,
};

// 回合/对局阶段。前 4 个值保持 SPEC-00/01 的既有语义，新增值追加在后，
// 避免旧状态快照与旧测试的数值漂移。
enum class RoundPhase : uint8_t {
    Freeze = 0,
    Live = 1,
    RoundEnd = 2,
    MatchEnd = 3,
    Warmup = 4,
    Halftime = 5,
    Timeout = 6,
};

// SPEC-03 §3.12 的对外名称；与 RoundPhase 同值，避免两套状态机。
using MatchPhase = RoundPhase;

// 回合结束原因：决定奖金口径与优先级（§3.6.2）。
enum class RoundEndReason : uint8_t {
    None = 0,
    Elimination = 1,  // 歼灭（对方全灭）
    BombExploded = 2, // C4 引爆
    BombDefused = 3,  // 拆包完成
    TimeExpired = 4,  // 回合计时到、包未下（CT 胜）
    Surrender = 5,    // 投降
    DeadRound = 6,    // 冻结期内异常死亡 → 重开，不计分
    Draw = 7,         // 强制平局（加时上限）
};

// 服务端校验的行为集合（§3.5.3 的 10 行）。
enum class Action : uint8_t {
    Move = 0,
    Fire = 1,
    Buy = 2,
    DropWeapon = 3,
    Pickup = 4,
    Plant = 5,
    Defuse = 6,
    SwitchView = 7,
    Respawn = 8,
    Vote = 9,
};

constexpr int kActionCount = 10;

// 赛制参数：全部来自 rules.json 生成的 kMatchRules；内核不写死任何数值。
struct MatchRules {
    bool enabled = false; // false = 旧版单半场行为（向后兼容开关）
    ModeId mode = ModeId::Comp5v5;
    uint8_t team_size = 5;
    uint8_t site_count = 2;
    uint16_t max_rounds = 24;
    uint16_t win_score = 13;
    uint16_t freeze_ticks = 960;
    uint16_t round_ticks = 7360;
    uint16_t buy_ticks = 1280;
    uint16_t round_end_ticks = 320;
    uint16_t warmup_ticks = 1920;
    uint16_t halftime_ticks = 11520;
    uint16_t c4_ticks = 2560;
    uint16_t defuse_ticks = 640;
    uint16_t defuse_kit_ticks = 320;
    uint32_t money_start = 800;
    uint32_t money_cap = 16000;
    uint16_t win_elim = 3250;
    uint16_t win_objective = 3500;
    std::array<uint16_t, 5> loss_ladder{{1400, 1900, 2400, 2900, 3400}};
    uint16_t plant_bonus = 300;
    uint16_t defuse_bonus = 300;
    uint16_t plant_loss_bonus = 600;
    // 击杀奖励，按 WeaponCategory 索引（0 rifle / 1 pistol / 2 sniper / 3 smg / 4 shotgun）。
    std::array<uint16_t, 5> kill_award{{300, 300, 100, 600, 900}};
    uint8_t timeout_count = 3;
    uint16_t timeout_ticks = 1920;
    bool friendly_fire = false;
    uint8_t overtime_cap = 10;
    uint8_t ot_rounds_per_half = 3;
    uint8_t ot_win_score = 4;
    uint32_t ot_money = 10000;

    // 换边点：5v5 第 12 回合、3v3 第 8 回合、1v1 第 5 回合（= max_rounds / 2）。
    uint16_t swap_round() const {
        return static_cast<uint16_t>(max_rounds / 2);
    }
};

} // namespace recoil
