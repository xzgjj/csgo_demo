#pragma once

#include <cstdint>

#include "recoil/content/content.h"
#include "recoil/sim/state.h"

namespace recoil {

// AI 购买策略（BUY-08）：与玩家共用 ApplyBuyOrder 校验，策略只决定“买什么档位”。
enum class AiBuyPolicy : uint8_t {
    Full = 0,  // 钱足：主武器 + 甲 + 道具
    Force = 1, // 半买：主武器或甲，视余额
    Eco = 2,   // 省：手枪局/护甲
    Save = 3,  // 全存：不买
};

// 单回合决策：余额 ≤ 350 或连败 ≥5 时 save；<1200 走 eco；<5000（或连败中 <5500）force；
// 其余 full。规则写成纯函数，便于 CSV 证据与回归。
AiBuyPolicy ChooseBuyPolicy(uint16_t money, uint8_t loss_streak);

// 把策略落成 catalog 下标列表（最多 max 条）；只做“预算内选择”，合法性仍由 ApplyBuyOrder 逐条校验。
// 返回条数；调用方按顺序提交 BuyEntry 命令（AI 与玩家共用同一入口）。
int PlanBotBuys(const WorldState& state, const Content& content, uint8_t player, uint8_t* entries,
                int max_entries);

} // namespace recoil
