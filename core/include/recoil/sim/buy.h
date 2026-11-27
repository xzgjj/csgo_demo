#pragma once

#include <cstdint>

#include "recoil/content/buy_catalog.h"
#include "recoil/content/content.h"
#include "recoil/sim/command.h"
#include "recoil/sim/event.h"
#include "recoil/sim/state.h"

namespace recoil {

// 购买失败原因（BUY-01/02/03；同时写入 Event::value，UI 据此显示禁用原因）。
enum class BuyResult : uint8_t {
    Ok = 0,
    InvalidPlayer = 1,
    OutOfWindow = 2,
    OutOfZone = 3,
    NoMoney = 4,
    LimitReached = 5,
    NotAllowed = 6, // 阵营不符 / 未上架
    AlreadyOwned = 7,
    NotRefundable = 8,
    RefundLimit = 9,
    InvalidEntry = 10,
    // 本回合已退款该条目：禁止再买/重购（防“试枪”刷经济）。
    RefundedBlocked = 11,
};

// 购买请求：entry 为 catalog 下标（UI/AI 共用同一入口，禁止直接改钱/槽位）。
struct BuyRequest {
    uint8_t player = 0;
    uint8_t entry = 0;
};

BuyResult ApplyBuyOrder(WorldState& state, const Content& content, BuyRequest request,
                        EventList& events);
BuyResult ApplyRefund(WorldState& state, const Content& content, BuyRequest request,
                      EventList& events);
// Step 分发：buy.enabled=false 时发 NotAllowed BuyFailed；否则按 kind 走 BuyEntry/Refund/Rebuy。
void ApplyCatalogCommand(WorldState& state, const Content& content, const Command& command,
                         EventList& events);

inline bool IsCatalogCommand(CommandKind kind) {
    return kind == CommandKind::BuyEntry || kind == CommandKind::Refund ||
           kind == CommandKind::Rebuy;
}
// 重购：按上一回合成功购买的条目依次下单；逐条原子，失败不影响已成功项。
int ApplyRebuy(WorldState& state, const Content& content, uint8_t player, EventList& events);
// 回合继承：存活保留武器/手雷/护甲；死亡清空主武器、手雷与护甲（副武器保留手枪位）。
void ApplyLoadoutInheritance(PlayerState& player, bool survived, uint8_t team);

bool InBuyZone(const WorldState& state, const PlayerState& player, const Content& content);
// 目录购买窗口是否开启（冻结期全程 + Live 的 window_ticks 宽限）；UI/宿主与内核共用同一判定。
bool IsBuyWindowOpen(const WorldState& state, const Content& content);
bool CanBuyEntry(const WorldState& state, const Content& content, uint8_t player, uint8_t entry,
                 BuyResult& reason);
const BuyEntry* FindBuyEntry(const Content& content, WeaponId weapon);
const BuyEntry* FindBuyEntryByName(const Content& content, const char* name);

} // namespace recoil
