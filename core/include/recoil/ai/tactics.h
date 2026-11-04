#pragma once

#include <cstdint>

#include "recoil/base/vec3.h"
#include "recoil/content/content.h"
#include "recoil/sim/command.h"
#include "recoil/sim/state.h"
#include "recoil/world/map.h"

namespace recoil {

// 5v5 战术层（Command 生产者）：角色分配、选点/转点、下包/守包/拆包。
// 纯函数只读 WorldState；唯一状态写入口仍是 Step(state, commands)。

// 队伍规模（5v5）。
constexpr uint8_t kTacticalTeamSize = 5;

// 战术角色：每队 5 人各一个，按同队 slot 确定性分配。
enum class TeamRole : uint8_t {
    Entry = 0,
    Support = 1,
    Anchor = 2,
    Rotator = 3,
    Lurker = 4,
};

// 包点；None 表示"按距离动态选择"（CT 机动位）。
enum class BombSite : uint8_t {
    None = 0,
    A = 1,
    B = 2,
};

// 战术状态机状态。
enum class TacticState : uint8_t {
    Idle = 0,    // 冻结/结算/无路径：原地
    Advance = 1, // 向战术目标推进
    Hold = 2,    // 已在包点附近巡逻/守点
    Rotate = 3,  // 转点或回防（队友死亡/炸弹事件）
    Plant = 4,   // 在包点下包
    Defuse = 5,  // 在包点拆包
    Engage = 6,  // 交战：开火
};

// 包点布局与判定半径（世界坐标；内容数据化前的默认值）。
struct SiteLayout {
    Vec3 site_a{512.0f, 0.0f, 0.0f};
    Vec3 site_b{-512.0f, 0.0f, 0.0f};
    float plant_radius = 96.0f;
    float hold_radius = 192.0f;
    float defuse_radius = 48.0f;
    float patrol_radius = 32.0f;
};

// 决策结果：状态机状态 + 角色/包点 + 本 tick 命令。
struct TacticalDecision {
    TacticState state = TacticState::Idle;
    TeamRole role = TeamRole::Support;
    BombSite site = BombSite::None;
    Command command{};
};

// 队内序号：按玩家索引导出的同队升序序号（0..4）；越界返回 kTacticalTeamSize。
uint8_t TeamSlot(const WorldState& state, uint8_t self);

// 按同队 slot 分配角色：0 Entry、1 Support、2 Anchor、3 Rotator、4 Lurker；
// 越界钳到 Lurker。
TeamRole RoleForSlot(uint8_t slot_in_team);

// 玩家角色（死亡或越界时仍按 slot 确定性给出）。
TeamRole RoleForPlayer(const WorldState& state, uint8_t self);

// T 团队选点：回合号为奇数打 A、偶数打 B（确定性，无随机）。
BombSite ChooseAttackSite(const WorldState& state);

// CT 两点守备：slot 0/1 → A、slot 2/4 → B、slot 3（Rotator）→ None（按距离机动）。
BombSite DefendSiteForSlot(uint8_t slot_in_team);

// 离 position 平面距离最近的包点（平局取 A）。
BombSite NearestSite(const SiteLayout& layout, const Vec3& position);

// 守点/巡逻落位点：包点中心 + 角色偏移（每个角色固定一侧）。
Vec3 HoldPoint(const SiteLayout& layout, BombSite site, TeamRole role);

// 完整战术决策：先交战（复用 BotDecide 的可见敌人开火），再按回合阶段与炸弹事件
// 决定选点/转点/下包/守包/回防拆包；有 map 时沿 NavPath 首航点推进，无路径则原地停止。
TacticalDecision TacticalDecide(const WorldState& state, const Content& content, uint8_t self,
                                const SiteLayout& layout, const MapDef* map = nullptr);

// 便捷入口：使用默认包点布局。
TacticalDecision TacticalDecide(const WorldState& state, const Content& content, uint8_t self,
                                const MapDef* map = nullptr);

// 战术命令入口（方便 Step 前统一收集 Command）。
Command TacticalBotDecide(const WorldState& state, const Content& content, uint8_t self,
                          const SiteLayout& layout, const MapDef* map = nullptr);

// 便捷入口：使用默认包点布局。
Command TacticalBotDecide(const WorldState& state, const Content& content, uint8_t self,
                          const MapDef* map = nullptr);

} // namespace recoil
