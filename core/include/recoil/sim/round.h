#pragma once

#include <cstdint>

#include "recoil/content/content.h"
#include "recoil/sim/command.h"
#include "recoil/sim/event.h"
#include "recoil/sim/state.h"
#include "recoil/world/map.h"

namespace recoil {

void AwardMoney(PlayerState& player, uint16_t amount, const RoundDef& def);

// 队伍分配：前一半玩家为 T，后一半为 CT（10 人即 5v5）。
void AssignTeams(WorldState& state);

// 开始一场 5v5：设置 10 名玩家、分队、发枪与出生点（de_ash 出生屋），进入首回合冻结期。
// 纯函数式状态初始化；地图仍由调用方通过 Step 的 MapDef 传入。
void BeginMatch(WorldState& state, const Content& content, EventList& events,
                const MapDef* map = nullptr);

// 回合状态机：冻结 → 进行中 → 结算 → 重置；含胜负判定与经济结算。
void AdvanceRound(WorldState& state, const Content& content, EventList& events,
                  const MapDef* map = nullptr);

void EndRound(WorldState& state, const Content& content, uint8_t winner, EventList& events);

// 购买窗口：冻结期 + Live 开局 buy_grace_ticks（演示可玩性）；其余阶段不可购买。
bool CanBuy(const WorldState& state, const Content& content);

// 购买（当前仅护甲）；只允许购买窗口内。
void ApplyBuy(WorldState& state, const Content& content, const Command& command, EventList& events);

// 购买主/副武器（BuyWeapon）：只允许冻结期；手枪进副武器槽，其余进主武器槽。
void ApplyBuyWeapon(WorldState& state, const Content& content, const Command& command,
                    EventList& events);

// 切换武器槽（SwitchWeapon）：交换主/副武器状态并切换激活槽；换弹中切枪会打断换弹。
void ApplySwitchWeapon(WorldState& state, const Command& command, EventList& events);

// 按队伍发默认主武器/副武器（T：AK-47 + Glock-18；CT：M4A4 + USP-S），并补满弹匣。
// 无 weapon_table 依赖的玩家（旧用例）保持 WeaponId::None。
void GiveDefaultLoadout(PlayerState& player, uint8_t team);

} // namespace recoil
