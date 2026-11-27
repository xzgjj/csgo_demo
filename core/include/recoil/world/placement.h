#pragma once
// 落位搜索：把「设计上的出生点/守点」贴着地图几何修正到可站立位置。
//
// 为什么需要它：出生中心与守点来自 layout 的设计坐标，设计坐标是“意图”，不是“净空”；
// 道具/台阶一挪动，原来的点就可能卡进实体。内核在 StartRound / 战术落位前用它做一次
// 确定性的环形扫描（32u 步进、最多 192u），保证 bot 不会“直架在道具里面”。
//
// 纯函数、无状态：同一个输入永远返回同一个输出，回放/哈希不受影响。

#include "recoil/world/map.h"

namespace recoil {

// 站立体壳：与移动/导航一致（半宽 16、身高 72）。
constexpr float kPlacementHalfWidth = 16.0f;
constexpr float kPlacementHeight = 72.0f;
// 扫描预算：半径 192u（6 个身位）、每圈 8 个方向、步进 32u。
constexpr float kPlacementMaxRadius = 192.0f;
constexpr float kPlacementStep = 32.0f;

// feet 是脚底点；检测 [feet.z, feet.z + height] × 半宽方柱是否与任何墙 AABB 相交。
bool SpotFree(const MapDef& map, const Vec3& feet, float half_width = kPlacementHalfWidth,
              float height = kPlacementHeight);

// 从 desired 开始做同心环扫描：中心优先、同半径按 8 方向固定顺序；找不到时原样返回。
Vec3 FindFreeSpot(const MapDef& map, const Vec3& desired, float half_width = kPlacementHalfWidth,
                  float height = kPlacementHeight, float max_radius = kPlacementMaxRadius,
                  float step = kPlacementStep);

} // namespace recoil
