#pragma once

#include <vector>

#include "recoil/base/vec3.h"
#include "recoil/world/map.h"

namespace recoil {

// 均匀导航网格边长（Source 单位）。
constexpr float kNavCellSize = 64.0f;

// 由 MapDef 墙体生成均匀网格并用 BFS 求最短路径（纯函数、无随机）。
// 返回航点序列：起点格之后的各格中心，末点为目标点；起止同格返回 {to}。
// 目标格被墙占据或与起点不连通时返回空。
std::vector<Vec3> NavPath(const MapDef& map, const Vec3& from, const Vec3& to);

} // namespace recoil
