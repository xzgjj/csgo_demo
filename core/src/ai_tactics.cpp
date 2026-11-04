#include "recoil/ai/tactics.h"

#include <cmath>
#include <vector>

#include "recoil/ai/bot.h"
#include "recoil/ai/nav.h"
#include "recoil/base/vec3.h"

namespace recoil {
namespace {

constexpr float kPi = 3.14159265358979323846f;
// 守点落位相对包点中心的偏移半径（四个角色各占一侧）。
constexpr float kHoldRing = 96.0f;

float FlatDistance(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

float YawDegrees(const Vec3& from, const Vec3& to) {
    return std::atan2(to.y - from.y, to.x - from.x) * (180.0f / kPi);
}

bool OnTeam(uint8_t team, Team want) {
    return team == static_cast<uint8_t>(want);
}

// 队伍名单人数（含阵亡）：存活 < 名单即观察到"队友死亡"事件。
uint8_t RosterCount(const WorldState& state, Team team) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        if (OnTeam(state.players[i].team, team)) {
            ++count;
        }
    }
    return count;
}

uint8_t LivingCount(const WorldState& state, Team team) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < state.player_count; ++i) {
        const PlayerState& player = state.players[i];
        if (player.alive && OnTeam(player.team, team)) {
            ++count;
        }
    }
    return count;
}

Vec3 SiteCenter(const SiteLayout& layout, BombSite site) {
    return site == BombSite::B ? layout.site_b : layout.site_a;
}

BombSite OtherSite(BombSite site) {
    return site == BombSite::A ? BombSite::B : BombSite::A;
}

Command StopCommand(uint8_t self) {
    Command command;
    command.kind = CommandKind::Stop;
    command.player = self;
    return command;
}

// 朝 target 推进：有 map 时取 NavPath 首航点；路径为空则原地停止（state 回退 Idle）。
void AimMove(const WorldState& state, uint8_t self, const Vec3& target, const MapDef* map,
             TacticalDecision& out) {
    const Vec3& position = state.players[self].position;
    Vec3 waypoint = target;
    if (map != nullptr) {
        const std::vector<Vec3> path = NavPath(*map, position, target);
        if (path.empty()) {
            out.state = TacticState::Idle;
            out.command = StopCommand(self);
            return;
        }
        waypoint = path.front();
    }
    out.command = StopCommand(self);
    out.command.kind = CommandKind::Move;
    out.command.yaw_degrees = YawDegrees(position, waypoint);
    out.command.move_forward = 1.0f;
}

// 已在包点 hold_radius 内：巡逻到角色落位点，落位后停止。
void PatrolHold(const WorldState& state, uint8_t self, const SiteLayout& layout, BombSite site,
                TeamRole role, const MapDef* map, TacticalDecision& out) {
    out.state = TacticState::Hold;
    const Vec3 hold = HoldPoint(layout, site, role);
    if (FlatDistance(state.players[self].position, hold) > layout.patrol_radius) {
        AimMove(state, self, hold, map, out);
    } else {
        out.command = StopCommand(self);
    }
}

// 守点：hold_radius 外推进到包点，内则巡逻落位。
void ApproachSite(const WorldState& state, uint8_t self, const SiteLayout& layout, BombSite site,
                  TeamRole role, const MapDef* map, TacticalDecision& out) {
    const Vec3 center = SiteCenter(layout, site);
    if (FlatDistance(state.players[self].position, center) > layout.hold_radius) {
        out.state = TacticState::Advance;
        AimMove(state, self, center, map, out);
        return;
    }
    PatrolHold(state, self, layout, site, role, map, out);
}

void DecideT(const WorldState& state, uint8_t self, const SiteLayout& layout, const MapDef* map,
             TacticalDecision& out) {
    const PlayerState& player = state.players[self];
    // 炸弹事件：C4 已埋 → 全体回防/守包。
    if (state.bomb.planted) {
        out.site = NearestSite(layout, state.bomb.position);
        const float distance = FlatDistance(player.position, state.bomb.position);
        if (distance > layout.hold_radius) {
            out.state = TacticState::Rotate;
            AimMove(state, self, state.bomb.position, map, out);
            return;
        }
        out.state = TacticState::Hold;
        if (distance > layout.patrol_radius) {
            AimMove(state, self, state.bomb.position, map, out);
        } else {
            out.command = StopCommand(self);
        }
        return;
    }
    // 选点：默认按回合号选 A/B；队友死亡 → 按距离转最近点；Lurker 无 C4 时走另一侧。
    const uint8_t roster = RosterCount(state, Team::T);
    const bool rotating = roster > 1 && LivingCount(state, Team::T) < roster;
    BombSite target = ChooseAttackSite(state);
    if (rotating) {
        target = NearestSite(layout, player.position);
    } else if (out.role == TeamRole::Lurker && !player.has_bomb) {
        target = OtherSite(target);
    }
    out.site = target;
    const Vec3 center = SiteCenter(layout, target);
    // 下包：携带 C4 且在 plant_radius 内。
    if (player.has_bomb) {
        if (FlatDistance(player.position, center) <= layout.plant_radius) {
            out.state = TacticState::Plant;
            out.command = StopCommand(self);
            out.command.kind = CommandKind::Plant;
            out.command.yaw_degrees = YawDegrees(player.position, center);
            return;
        }
        out.state = TacticState::Advance;
        AimMove(state, self, center, map, out);
        return;
    }
    if (rotating) {
        out.state = TacticState::Rotate;
        if (FlatDistance(player.position, center) > layout.hold_radius) {
            AimMove(state, self, center, map, out);
        } else {
            PatrolHold(state, self, layout, target, out.role, map, out);
        }
        return;
    }
    ApproachSite(state, self, layout, target, out.role, map, out);
}

void DecideCT(const WorldState& state, uint8_t self, const SiteLayout& layout, const MapDef* map,
              TacticalDecision& out) {
    const PlayerState& player = state.players[self];
    // 炸弹事件：回防到 C4；进入 defuse_radius 即拆包。
    if (state.bomb.planted) {
        out.site = NearestSite(layout, state.bomb.position);
        const float distance = FlatDistance(player.position, state.bomb.position);
        if (distance <= layout.defuse_radius) {
            out.state = TacticState::Defuse;
            out.command = StopCommand(self);
            out.command.kind = CommandKind::Defuse;
            out.command.yaw_degrees = YawDegrees(player.position, state.bomb.position);
            return;
        }
        out.state = TacticState::Rotate;
        AimMove(state, self, state.bomb.position, map, out);
        return;
    }
    // 两点守备：按 slot 分配；Rotator 与队友死亡事件按距离选最近点。
    const uint8_t roster = RosterCount(state, Team::CT);
    const bool rotating = roster > 1 && LivingCount(state, Team::CT) < roster;
    BombSite site = DefendSiteForSlot(TeamSlot(state, self));
    if (rotating || site == BombSite::None) {
        site = NearestSite(layout, player.position);
    }
    out.site = site;
    if (rotating) {
        out.state = TacticState::Rotate;
        const Vec3 center = SiteCenter(layout, site);
        if (FlatDistance(player.position, center) > layout.hold_radius) {
            AimMove(state, self, center, map, out);
        } else {
            PatrolHold(state, self, layout, site, out.role, map, out);
        }
        return;
    }
    ApproachSite(state, self, layout, site, out.role, map, out);
}

} // namespace

uint8_t TeamSlot(const WorldState& state, uint8_t self) {
    if (self >= state.player_count) {
        return kTacticalTeamSize;
    }
    const uint8_t team = state.players[self].team;
    uint8_t slot = 0;
    for (uint8_t i = 0; i < self; ++i) {
        if (state.players[i].team == team) {
            ++slot;
        }
    }
    return slot;
}

TeamRole RoleForSlot(uint8_t slot_in_team) {
    switch (slot_in_team) {
    case 0:
        return TeamRole::Entry;
    case 1:
        return TeamRole::Support;
    case 2:
        return TeamRole::Anchor;
    case 3:
        return TeamRole::Rotator;
    default:
        return TeamRole::Lurker;
    }
}

TeamRole RoleForPlayer(const WorldState& state, uint8_t self) {
    return RoleForSlot(TeamSlot(state, self));
}

BombSite ChooseAttackSite(const WorldState& state) {
    return state.round.number % 2 == 1 ? BombSite::A : BombSite::B;
}

BombSite DefendSiteForSlot(uint8_t slot_in_team) {
    switch (slot_in_team) {
    case 0:
    case 1:
        return BombSite::A;
    case 2:
    case 4:
        return BombSite::B;
    default:
        return BombSite::None;
    }
}

BombSite NearestSite(const SiteLayout& layout, const Vec3& position) {
    const float to_a = FlatDistance(position, layout.site_a);
    const float to_b = FlatDistance(position, layout.site_b);
    return to_a <= to_b ? BombSite::A : BombSite::B;
}

Vec3 HoldPoint(const SiteLayout& layout, BombSite site, TeamRole role) {
    const Vec3 center = SiteCenter(layout, site);
    switch (role) {
    case TeamRole::Entry:
        return {center.x, center.y + kHoldRing, center.z};
    case TeamRole::Support:
        return {center.x - kHoldRing, center.y, center.z};
    case TeamRole::Anchor:
        return {center.x + kHoldRing, center.y, center.z};
    case TeamRole::Rotator:
        return {center.x, center.y - kHoldRing, center.z};
    default:
        return center;
    }
}

TacticalDecision TacticalDecide(const WorldState& state, const Content& content, uint8_t self,
                                const SiteLayout& layout, const MapDef* map) {
    TacticalDecision out;
    out.command = StopCommand(self);
    out.role = RoleForPlayer(state, self);
    if (self >= state.player_count || !state.players[self].alive) {
        return out;
    }
    // 非进行中回合（冻结/结算/比赛结束）不做战术移动。
    if (state.round.phase != RoundPhase::Live) {
        return out;
    }
    // 交战优先：复用既有 BotDecide 的可见敌人开火。
    const Command combat = BotDecide(state, content, self, map);
    if (combat.kind == CommandKind::Fire) {
        out.state = TacticState::Engage;
        out.command = combat;
        return out;
    }
    // 炸弹已结算（拆除/爆炸）：保持原地，由回合系统收尾。
    if (state.bomb.exploded || state.bomb.defused) {
        return out;
    }
    const uint8_t team = state.players[self].team;
    if (OnTeam(team, Team::T)) {
        DecideT(state, self, layout, map, out);
    } else if (OnTeam(team, Team::CT)) {
        DecideCT(state, self, layout, map, out);
    }
    return out;
}

TacticalDecision TacticalDecide(const WorldState& state, const Content& content, uint8_t self,
                                const MapDef* map) {
    return TacticalDecide(state, content, self, SiteLayout{}, map);
}

Command TacticalBotDecide(const WorldState& state, const Content& content, uint8_t self,
                          const SiteLayout& layout, const MapDef* map) {
    return TacticalDecide(state, content, self, layout, map).command;
}

Command TacticalBotDecide(const WorldState& state, const Content& content, uint8_t self,
                          const MapDef* map) {
    return TacticalDecide(state, content, self, map).command;
}

} // namespace recoil
