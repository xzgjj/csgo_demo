#pragma once

#include <cstdint>

#include "recoil/base/vec3.h"
#include "recoil/sim/state.h"
#include "recoil/world/map.h"

namespace recoil {

// 视线采样高度：观察者眼睛 64u/蹲 46u、目标胸口 36u/蹲 27u（与 Bot 瞄准/弹道一致）。
constexpr float kEyeHeight = 64.0f;
constexpr float kDuckEyeHeight = 46.0f;
constexpr float kChestHeight = 36.0f;
constexpr float kDuckChestHeight = 27.0f;

// 线段 from→to 是否被地图 AABB 遮挡；map 为 nullptr 时永不遮挡。
bool SegmentBlockedByWalls(const MapDef* map, const Vec3& from, const Vec3& to);

// 线段是否被烟雾遮挡（只判烟雾，便于单独测试）。
bool SegmentBlockedBySmoke(const WorldState& state, const Vec3& from, const Vec3& to);

// 线段是否被墙体或烟雾遮挡（视线/弹道判定共用）。
bool SegmentBlocked(const WorldState& state, const MapDef* map, const Vec3& from, const Vec3& to);

// viewer 能否看到 target：任一方阵亡/越界/自己 → false；不含阵营过滤（队友同样几何可见）。
bool HasLineOfSight(const WorldState& state, const MapDef* map, uint8_t viewer, uint8_t target);

// 带每 tick 视线备忘录（VisibilityMemo）的同一判定：
// Sees 只读——备忘录命中则 O(1)，未命中直接 trace 但不写回（给 const 调用方用）；
// SeesMemo 可写回——同一 tick 内同一对玩家只 trace 一次（供 UpdateReactions 去重）。
bool Sees(const WorldState& state, const MapDef* map, uint8_t viewer, uint8_t target);
bool SeesMemo(WorldState& state, const MapDef* map, uint8_t viewer, uint8_t target);

// 最近的可见敌人；没有可见敌人返回 -1。
int NearestVisibleEnemy(const WorldState& state, const MapDef* map, uint8_t viewer);

} // namespace recoil
