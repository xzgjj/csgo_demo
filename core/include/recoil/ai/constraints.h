#pragma once

#include <cstdint>

#include "recoil/base/vec3.h"

namespace recoil {

struct WorldState;

// 人机难度参数（SPEC-03 §3.8.3）。数值来自 content/defs/rules.json 的 bot_difficulty 段。
struct BotDifficulty {
    uint8_t level = 1;
    uint16_t reaction_ticks = 32;  // 反应延迟（tick，64 Hz）
    float view_units = 1500.0f;    // 视距（Unreal units）
    float aim_sigma_deg = 2.2f;    // 瞄准高斯误差 σ
    float first_cone_deg = 2.5f;   // 首发偏差锥
    float turn_deg_per_s = 300.0f; // 转身角速度上限
    uint8_t burst_shots = 7;       // 点射发数（0 = 长按泼水）
};

// 难度查表：level 越界时夹到 0..3，保证调用方不用判空。
const BotDifficulty& DifficultyFor(uint8_t level);

// 友军约束（§3.7.1）。全部为纯函数：只读状态，便于回放断言与单测。

// FF-1 不挡枪线：队友在“射手→目标”线段 1.2 m 内且偏离射线 <8° → 应抑制开火。
bool AllyBlocksFireLine(const WorldState& state, uint8_t shooter, uint8_t target, uint8_t ally,
                        float* out_angle);

// FF-3 不抢包：人类队友距 C4 3 m 内且面向 C4 → bot 放弃拾取。
bool AllyClaimsBomb(const WorldState& state, uint8_t bot, uint8_t human_ally, float pickup_range);

// FF-5 不堵出生：冻结结束 1.5 s 后，仍在出生点 2 m 内视为堵出生。
bool BotBlocksSpawn(const WorldState& state, uint8_t bot, const Vec3& spawn_center, float radius);

// FF-6 不抢拆包：人类队友正在拆包 → bot 不得进入 2 m。
bool AllyIsDefusing(const WorldState& state, uint8_t human_ally, float guard_range, uint8_t bot);

// 分帧调度（§3.10.2）：每 tick 处理 ceil(N/4) 个 bot，4 tick 覆盖全部。
struct AiFrameScheduler {
    uint8_t cursor = 0;
    uint8_t budget_overruns = 0;

    // 本 tick 应处理的 bot 数量。
    static uint8_t BatchSize(uint8_t total);
    // 推进游标并返回本 tick 处理的起始索引。
    uint8_t NextBatch(uint8_t total);
    // 单帧耗时超过预算时的熔断：记一次 overrun，并把本批推迟到下一帧。
    bool Trip(uint16_t elapsed_us, uint16_t budget_us);
};

} // namespace recoil
