#pragma once

// 模式预置（Mode preset）：把“这局按哪种模式跑”收成一张表与一组命名谓词。
// 目的：把散落在 round/buy/grenades 的 `match.enabled` / `buy.enabled` 直读，
// 收敛成“问语义”的调用点；新增模式只改本文件的预置表，不去各模块改条件。
//
// 边界：本文件只决定**开关怎么设**，不重写规则实现。Legacy/Training 仍走
// round.cpp 的旧单半场分支；真正的“折叠成一条规则路径”需要产品上先取消
// 2 人演示模式，届时把 Legacy 预置删掉即可，调用点无需再改。

#include <cstdint>

#include "recoil/content/content.h"
#include "recoil/content/rules_table.gen.h"
#include "recoil/sim/state.h"

namespace recoil {

// 三种可玩组合（与 rules.json 的 ModeId 正交：前两种不跑赛制，第三种按 ModeId 取规则）。
enum class PlayMode : uint8_t {
    kLegacyDemo = 0,  // match 关 + 目录购买关：灰盒/双人练习
    kTraining = 1,    // match 关 + 目录购买开：训练/买枪台
    kCompetitive = 2, // match 开（ModeId 决定 1v1/3v3/5v5）+ 目录购买开
};

// 语义谓词：调用方只问“这个模式下某机制是否生效”，不直接摸布尔字段。
inline bool MatchScoringOn(const WorldState& state) {
    return state.match.enabled;
}
inline bool CatalogBuyOn(const Content& content) {
    return content.buy.enabled;
}
inline bool IsCompetitive(const WorldState& state) {
    return MatchScoringOn(state);
}
inline bool IsTraining(const WorldState& state, const Content& content) {
    return !MatchScoringOn(state) && CatalogBuyOn(content);
}
inline bool IsLegacyDemo(const WorldState& state, const Content& content) {
    return !MatchScoringOn(state) && !CatalogBuyOn(content);
}

// 预置应用：只写模式开关与赛制参数，不重置回合/比分（重置由 BeginMatch* 负责）。
// 调用顺序约定：ApplyModePreset → BeginMatch / BeginMatchWithRules。
inline void ApplyModePreset(WorldState& state, Content& content, PlayMode mode,
                            ModeId rules_mode = ModeId::Comp5v5) {
    switch (mode) {
    case PlayMode::kCompetitive:
        state.match = content::MatchRulesFor(rules_mode);
        state.match.enabled = true;
        content.buy.enabled = true;
        break;
    case PlayMode::kTraining:
        state.match = MatchRules{};
        content.buy.enabled = true;
        break;
    case PlayMode::kLegacyDemo:
    default:
        state.match = MatchRules{};
        content.buy.enabled = false;
        break;
    }
}

// 一致性自检：给测试与宿主启动自检用，避免“模式说不计分、开关却开着”这类漂移。
inline bool ModeConsistent(const WorldState& state, const Content& content) {
    if (IsCompetitive(state)) {
        return CatalogBuyOn(content) && state.match.team_size >= 1 && state.match.team_size <= 5;
    }
    return !state.match.enabled;
}

} // namespace recoil
