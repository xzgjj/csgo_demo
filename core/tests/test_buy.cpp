#include <doctest/doctest.h>

#include <cstdlib>
#include <fstream>

#include "recoil/ai/buy_policy.h"
#include "recoil/content/buy_catalog.gen.h"
#include "recoil/content/content.h"
#include "recoil/content/weapon_table.h"
#include "recoil/sim/buy.h"
#include "recoil/sim/command.h"
#include "recoil/sim/grenades.h"
#include "recoil/sim/round.h"
#include "recoil/sim/step.h"

using namespace recoil;

namespace {

Content BuyContent() {
    Content content = DefaultContent();
    content.buy.enabled = true;
    content.buy.zone_required = false;
    content.buy.max_refunds = 3;
    return content;
}

WorldState TwoPlayers(uint16_t money = 16000) {
    WorldState state;
    state.player_count = 2;
    for (uint8_t i = 0; i < 2; ++i) {
        PlayerState& p = state.players[i];
        p.alive = true;
        p.health = 100;
        p.money = money;
        p.team = i == 0 ? static_cast<uint8_t>(Team::T) : static_cast<uint8_t>(Team::CT);
    }
    state.round.phase = RoundPhase::Freeze;
    state.round.phase_ticks = 64;
    return state;
}

uint8_t Entry(const char* name) {
    const BuyEntry* entry = FindBuyEntryByName(DefaultContent(), name);
    if (entry == nullptr) {
        return 0xFF;
    }
    return static_cast<uint8_t>(entry - kBuyEntries);
}

} // namespace

TEST_CASE("BUY-09 目录价格与内核扣款一致") {
    const Content content = BuyContent();
    CHECK(content.buy.count == kBuyEntryCount);
    CHECK(WeaponPrice(WeaponId::AK47) == 2700);
    CHECK(WeaponPrice(WeaponId::M4A4) == 3100);
    CHECK(WeaponPrice(WeaponId::AWP) == 4750);
    WorldState state = TwoPlayers();
    EventList events;
    CHECK(ApplyBuyOrder(state, content, {0, Entry("ak47")}, events) == BuyResult::Ok);
    CHECK(state.players[0].money == 13300);
    CHECK(state.players[0].primary_id == static_cast<uint8_t>(WeaponId::AK47));
    CHECK(state.players[0].weapon.mag == 30);
    REQUIRE(events.count == 1);
    CHECK(events.items[0].kind == EventKind::BuySucceeded);
    CHECK(events.items[0].value == 13300);
}

TEST_CASE("BUY-01 购买窗口：冻结期可买，Live 宽限 5s（320 tick）后关闭") {
    const Content content = BuyContent();
    CHECK(content.buy.window_ticks == 320);
    WorldState state = TwoPlayers();
    state.round.phase = RoundPhase::Live;
    state.round.phase_ticks = content.round.live_ticks - 400; // elapsed 400 > 320
    EventList events;
    CHECK(ApplyBuyOrder(state, content, {0, Entry("ak47")}, events) == BuyResult::OutOfWindow);
    CHECK(static_cast<int32_t>(events.items[0].value) ==
          static_cast<int32_t>(BuyResult::OutOfWindow));
    CHECK(state.players[0].money == 16000);
    state.round.phase_ticks = content.round.live_ticks; // elapsed 0，宽限内
    CHECK(ApplyBuyOrder(state, content, {0, Entry("ak47")}, events) == BuyResult::Ok);
}

TEST_CASE("BUY-02 金钱不足拒绝且不扣款") {
    const Content content = BuyContent();
    WorldState state = TwoPlayers(1000);
    EventList events;
    CHECK(ApplyBuyOrder(state, content, {0, Entry("ak47")}, events) == BuyResult::NoMoney);
    CHECK(state.players[0].money == 1000);
    CHECK(static_cast<int32_t>(events.items[0].value) == static_cast<int32_t>(BuyResult::NoMoney));
    CHECK(ApplyBuyOrder(state, content, {0, Entry("kevlar")}, events) == BuyResult::Ok);
    CHECK(state.players[0].money == 350);
    CHECK(state.players[0].armor == 100);
}

TEST_CASE("BUY-03 限购：手雷 2 颗、同类装备 1 件") {
    const Content content = BuyContent();
    WorldState state = TwoPlayers();
    EventList events;
    CHECK(ApplyBuyOrder(state, content, {0, Entry("flash")}, events) == BuyResult::Ok);
    CHECK(ApplyBuyOrder(state, content, {0, Entry("flash")}, events) == BuyResult::Ok);
    CHECK(state.players[0].grenade_counts[static_cast<int>(GrenadeKind::Flash)] == 2);
    CHECK(ApplyBuyOrder(state, content, {0, Entry("flash")}, events) == BuyResult::LimitReached);
    CHECK(ApplyBuyOrder(state, content, {0, Entry("he")}, events) == BuyResult::Ok);
    CHECK(ApplyBuyOrder(state, content, {0, Entry("he")}, events) == BuyResult::LimitReached);
    CHECK(ApplyBuyOrder(state, content, {0, Entry("helmet")}, events) == BuyResult::Ok);
    CHECK(ApplyBuyOrder(state, content, {0, Entry("helmet")}, events) == BuyResult::AlreadyOwned);
}

TEST_CASE("BUY-03 阵营限制：T 不能买 USP、CT 不能买 AK") {
    const Content content = BuyContent();
    WorldState state = TwoPlayers();
    EventList events;
    CHECK(ApplyBuyOrder(state, content, {0, Entry("usp_s")}, events) == BuyResult::NotAllowed);
    CHECK(ApplyBuyOrder(state, content, {1, Entry("ak47")}, events) == BuyResult::NotAllowed);
    CHECK(ApplyBuyOrder(state, content, {1, Entry("m4a4")}, events) == BuyResult::Ok);
}

TEST_CASE("BUY-01 购买区：区域外拒绝，区域内放行") {
    Content content = BuyContent();
    content.buy.zone_required = true;
    WorldState state = TwoPlayers();
    state.buy_zone_set = true;
    state.buy_zone_min = {-100.0f, -100.0f, -100.0f};
    state.buy_zone_max = {100.0f, 100.0f, 100.0f};
    state.players[0].position = {500.0f, 0.0f, 0.0f};
    EventList events;
    CHECK(ApplyBuyOrder(state, content, {0, Entry("ak47")}, events) == BuyResult::OutOfZone);
    state.players[0].position = {0.0f, 0.0f, 0.0f};
    CHECK(ApplyBuyOrder(state, content, {0, Entry("ak47")}, events) == BuyResult::Ok);
}

TEST_CASE("BUY-04 退款：未使用全额退，开火后拒绝，退款后禁再买") {
    const Content content = BuyContent();
    WorldState state = TwoPlayers();
    EventList events;
    CHECK(ApplyBuyOrder(state, content, {0, Entry("ak47")}, events) == BuyResult::Ok);
    CHECK(state.players[0].money == 13300);
    CHECK(ApplyRefund(state, content, {0, Entry("ak47")}, events) == BuyResult::Ok);
    CHECK(state.players[0].money == 16000);
    CHECK(state.players[0].primary_id == 0);
    CHECK(ApplyBuyOrder(state, content, {0, Entry("ak47")}, events) == BuyResult::RefundedBlocked);
    CHECK(ApplyRefund(state, content, {0, Entry("ak47")}, events) == BuyResult::NotRefundable);

    WorldState fired = TwoPlayers();
    CHECK(ApplyBuyOrder(fired, content, {0, Entry("ak47")}, events) == BuyResult::Ok);
    ++fired.players[0].fire_count;
    CHECK(ApplyRefund(fired, content, {0, Entry("ak47")}, events) == BuyResult::NotRefundable);
}

TEST_CASE("BUY-04 退款上限 3 次") {
    const Content content = BuyContent();
    WorldState state = TwoPlayers();
    EventList events;
    state.players[0].refund_count = 3;
    CHECK(ApplyBuyOrder(state, content, {0, Entry("ak47")}, events) == BuyResult::Ok);
    CHECK(ApplyRefund(state, content, {0, Entry("ak47")}, events) == BuyResult::RefundLimit);
}

TEST_CASE("BUY-05 继承：存活保留、死亡清空、护甲不继承") {
    PlayerState alive;
    alive.primary_id = static_cast<uint8_t>(WeaponId::AK47);
    alive.armor = 100;
    alive.helmet = true;
    alive.grenade_counts = {{1, 2, 1}};
    ApplyLoadoutInheritance(alive, true, static_cast<uint8_t>(Team::T));
    CHECK(alive.primary_id == static_cast<uint8_t>(WeaponId::AK47));
    CHECK(alive.armor == 100);

    PlayerState dead;
    dead.primary_id = static_cast<uint8_t>(WeaponId::AWP);
    dead.armor = 100;
    dead.helmet = true;
    dead.grenade_counts = {{1, 1, 1}};
    ApplyLoadoutInheritance(dead, false, static_cast<uint8_t>(Team::CT));
    CHECK(dead.primary_id == 0);
    CHECK(dead.armor == 0);
    CHECK(dead.helmet == false);
    CHECK(dead.grenade_counts[0] == 0);
    CHECK(dead.secondary_id == static_cast<uint8_t>(WeaponId::USP));
}

TEST_CASE("BUY-06 重购：上一回合配置逐条重放") {
    Content content = BuyContent();
    WorldState state = TwoPlayers();
    EventList events;
    CHECK(ApplyBuyOrder(state, content, {0, Entry("ak47")}, events) == BuyResult::Ok);
    CHECK(ApplyBuyOrder(state, content, {0, Entry("kevlar")}, events) == BuyResult::Ok);
    state.players[0].last_buy_mask = state.players[0].round_buy_mask;
    state.players[0].primary_id = 0;
    state.players[0].armor = 0;
    state.players[0].bought_counts = {};
    state.players[0].round_buy_mask = 0;
    const int bought = ApplyRebuy(state, content, 0, events);
    CHECK(bought == 2);
    CHECK(state.players[0].primary_id == static_cast<uint8_t>(WeaponId::AK47));
    CHECK(state.players[0].armor == 100);
}

TEST_CASE("BUY-06 投掷消耗库存：买 1 闪投 1 后不可再投") {
    Content content = BuyContent();
    WorldState state = TwoPlayers();
    EventList events;
    CHECK(ApplyBuyOrder(state, content, {0, Entry("flash")}, events) == BuyResult::Ok);
    Command throw_cmd;
    throw_cmd.kind = CommandKind::Throw;
    throw_cmd.player = 0;
    throw_cmd.grenade = GrenadeKind::Flash;
    ThrowGrenade(state, content, throw_cmd, events);
    CHECK(state.players[0].grenade_counts[static_cast<int>(GrenadeKind::Flash)] == 0);
    events.count = 0;
    ThrowGrenade(state, content, throw_cmd, events);
    CHECK(events.items[0].kind == EventKind::CommandRejected);
    CHECK(static_cast<int32_t>(events.items[0].value) ==
          static_cast<int32_t>(RejectReason::NoGrenadeSlot));
}

TEST_CASE("BUY-09 关闭购买系统时新命令被拒，旧路径不变") {
    Content content = BuyContent();
    content.buy.enabled = false;
    WorldState state = TwoPlayers();
    EventList events;
    Command command;
    command.kind = CommandKind::BuyEntry;
    command.player = 0;
    command.entry = Entry("ak47");
    const Command commands[1]{command};
    StepResult result = Step(state, content, commands, nullptr);
    CHECK(result.events.items[0].kind == EventKind::BuyFailed);
    CHECK(result.state.players[0].money == 16000);
}

TEST_CASE("BUY-08 AI 购买策略：full/force/eco/save 的选择") {
    // 策略只看“钱/连败/回合类型”，与 UI 无关；此处锁定规则表，AI 实现在 ai_buy.cpp。
    CHECK(ChooseBuyPolicy(16000, 0) == AiBuyPolicy::Full);
    CHECK(ChooseBuyPolicy(4200, 2) == AiBuyPolicy::Force);
    CHECK(ChooseBuyPolicy(1200, 1) == AiBuyPolicy::Eco);
    CHECK(ChooseBuyPolicy(300, 5) == AiBuyPolicy::Save);
    // 证据链：设置 RECOIL_BUY_CSV 时导出策略表（CI 默认不写文件）。
    if (const char* path = std::getenv("RECOIL_BUY_CSV")) {
        std::ofstream out(path);
        out << "money,loss_streak,policy\n";
        const uint16_t money[] = {16000, 8000, 5000, 4200, 1200, 300};
        for (uint16_t m : money) {
            for (uint8_t loss = 0; loss <= 5; ++loss) {
                out << m << "," << static_cast<int>(loss) << ","
                    << static_cast<int>(ChooseBuyPolicy(m, loss)) << "\n";
            }
        }
    }
}

TEST_CASE("BUY-08 AI 下单：策略→目录条目→同一校验函数全部合法") {
    const Content content = BuyContent();
    WorldState rich = TwoPlayers(16000);
    uint8_t entries[8] = {};
    const int n = PlanBotBuys(rich, content, 0, entries, 8);
    CHECK(n >= 4); // full：AWP + 甲 + 盔 + 投掷物
    EventList events;
    int spent = 0;
    for (int i = 0; i < n; ++i) {
        const BuyEntry& item = content.buy.entries[entries[i]];
        CHECK(ApplyBuyOrder(rich, content, {0, entries[i]}, events) == BuyResult::Ok);
        spent += item.price;
    }
    CHECK(rich.players[0].primary_id == static_cast<uint8_t>(WeaponId::AWP));
    CHECK(rich.players[0].money == static_cast<uint16_t>(16000 - spent));

    WorldState poor = TwoPlayers(300);
    CHECK(PlanBotBuys(poor, content, 0, entries, 8) == 0); // save 不买
    poor.players[0].money = 1200;
    const int eco = PlanBotBuys(poor, content, 0, entries, 8);
    CHECK(eco >= 1); // eco 至少买甲/闪光
    CHECK(content.buy.entries[entries[0]].kind == BuyKind::Armor);
}
