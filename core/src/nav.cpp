#include "recoil/ai/nav.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "recoil/base/vec3.h"

namespace recoil {
namespace {

constexpr float kHalfWidth = 16.0f;
constexpr float kBodyHeight = 72.0f;
constexpr float kStepUp = 32.0f;
constexpr float kMaxDrop = 400.0f;
constexpr float kEpsilon = 1.0e-3f;
// 只把 0..200 的实体顶面当导航层（屋顶 256 不可达）。
constexpr float kMaxWalkableTop = 200.0f;
constexpr float kMinWalkableTop = 8.0f;
constexpr float kSampleOffset = 18.0f;

struct NavGrid {
    int nx = 0;
    int ny = 0;
    float origin_x = 0.0f;
    float origin_y = 0.0f;
};

int CountCells(float low, float high) {
    const float span = high - low;
    if (span <= 0.0f) {
        return 0;
    }
    return static_cast<int>(std::ceil(span / kNavCellSize));
}

NavGrid MakeGrid(const MapDef& map) {
    NavGrid grid;
    grid.nx = CountCells(map.nav_min.x, map.nav_max.x);
    grid.ny = CountCells(map.nav_min.y, map.nav_max.y);
    grid.origin_x = map.nav_min.x;
    grid.origin_y = map.nav_min.y;
    return grid;
}

int ClampIndex(int value, int count) {
    if (count <= 0) {
        return 0;
    }
    return std::clamp(value, 0, count - 1);
}

int CellIndex(const NavGrid& grid, int cx, int cy) {
    return cy * grid.nx + cx;
}

float CenterX(const NavGrid& grid, int cx) {
    return grid.origin_x + (static_cast<float>(cx) + 0.5f) * kNavCellSize;
}

float CenterY(const NavGrid& grid, int cy) {
    return grid.origin_y + (static_cast<float>(cy) + 0.5f) * kNavCellSize;
}

int CellAt(const NavGrid& grid, const Vec3& point, int axis) {
    const float origin = axis == 0 ? grid.origin_x : grid.origin_y;
    const float value = axis == 0 ? point.x : point.y;
    const int count = axis == 0 ? grid.nx : grid.ny;
    return ClampIndex(static_cast<int>(std::floor((value - origin) / kNavCellSize)), count);
}

bool BodyFree(const MapDef& map, float x, float y, float z) {
    for (int i = 0; i < map.wall_count; ++i) {
        const Wall& wall = map.walls[i];
        if (x - kHalfWidth < wall.max.x && x + kHalfWidth > wall.min.x &&
            y - kHalfWidth < wall.max.y && y + kHalfWidth > wall.min.y && z < wall.max.z &&
            z + kBodyHeight > wall.min.z) {
            return false;
        }
    }
    return true;
}

// 格内找一个能容纳玩家的采样点（门洞/墙角处仍可通行），返回代表点。
bool FreeSamplePoint(const MapDef& map, float x, float y, float z, Vec3& out) {
    const float offsets[9][2] = {
        {0.0f, 0.0f},
        {kSampleOffset, 0.0f},
        {-kSampleOffset, 0.0f},
        {0.0f, kSampleOffset},
        {0.0f, -kSampleOffset},
        {kSampleOffset, kSampleOffset},
        {kSampleOffset, -kSampleOffset},
        {-kSampleOffset, kSampleOffset},
        {-kSampleOffset, -kSampleOffset},
    };
    for (const auto& offset : offsets) {
        if (BodyFree(map, x + offset[0], y + offset[1], z)) {
            out = {x + offset[0], y + offset[1], z};
            return true;
        }
    }
    return false;
}

// 两点之间沿线段检查：中间点抬到两端较高处，允许沿台阶步进；端点必须能站人。
bool SegmentFree(const MapDef& map, const Vec3& a, const Vec3& b) {
    if (!BodyFree(map, a.x, a.y, a.z) || !BodyFree(map, b.x, b.y, b.z)) {
        return false;
    }
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / 16.0f)));
    const float z = std::max(a.z, b.z);
    for (int i = 1; i < steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        if (!BodyFree(map, a.x + dx * t, a.y + dy * t, z)) {
            return false;
        }
    }
    return true;
}

// 每格的可站立点：地面 + 实体顶面；每个可达层保存一个可站采样点。
std::vector<std::vector<Vec3>> BuildLevels(const MapDef& map, const NavGrid& grid) {
    std::vector<std::vector<Vec3>> levels(static_cast<size_t>(grid.nx * grid.ny));
    for (int cy = 0; cy < grid.ny; ++cy) {
        for (int cx = 0; cx < grid.nx; ++cx) {
            const float x0 = grid.origin_x + static_cast<float>(cx) * kNavCellSize;
            const float y0 = grid.origin_y + static_cast<float>(cy) * kNavCellSize;
            std::vector<float> candidates{0.0f};
            for (int i = 0; i < map.wall_count; ++i) {
                const Wall& wall = map.walls[i];
                if (!(wall.min.x < x0 + kNavCellSize && wall.max.x > x0 &&
                      wall.min.y < y0 + kNavCellSize && wall.max.y > y0)) {
                    continue;
                }
                if (wall.max.z > kMinWalkableTop && wall.max.z <= kMaxWalkableTop) {
                    candidates.push_back(wall.max.z);
                }
            }
            std::sort(candidates.begin(), candidates.end());
            std::vector<Vec3>& cell = levels[static_cast<size_t>(CellIndex(grid, cx, cy))];
            for (const float candidate : candidates) {
                bool duplicate = false;
                for (const Vec3& existing : cell) {
                    duplicate = duplicate || std::fabs(existing.z - candidate) <= 0.5f;
                }
                if (duplicate) {
                    continue;
                }
                Vec3 point{};
                if (FreeSamplePoint(map, CenterX(grid, cx), CenterY(grid, cy), candidate, point)) {
                    cell.push_back(point);
                }
            }
            std::sort(cell.begin(), cell.end(),
                      [](const Vec3& a, const Vec3& b) { return a.z < b.z; });
        }
    }
    return levels;
}

int NearestLevel(const std::vector<Vec3>& cell_levels, float z) {
    int best = -1;
    float best_distance = std::numeric_limits<float>::max();
    for (size_t i = 0; i < cell_levels.size(); ++i) {
        const float distance = std::fabs(cell_levels[i].z - z);
        if (distance < best_distance) {
            best_distance = distance;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int CellOfNode(const std::vector<int>& offsets, int node) {
    int cell = 0;
    while (cell + 1 < static_cast<int>(offsets.size()) &&
           offsets[static_cast<size_t>(cell + 1)] <= node) {
        ++cell;
    }
    return cell;
}

Vec3 NodePoint(const std::vector<std::vector<Vec3>>& levels, const std::vector<int>& offsets,
               int node) {
    const int cell = CellOfNode(offsets, node);
    const int level = node - offsets[static_cast<size_t>(cell)];
    return levels[static_cast<size_t>(cell)][static_cast<size_t>(level)];
}

std::vector<Vec3> BuildPath(const std::vector<std::vector<Vec3>>& levels,
                            const std::vector<int>& offsets, const std::vector<int>& parent,
                            int start, int goal, const Vec3& to) {
    std::vector<int> chain;
    for (int node = goal; node != start && node >= 0; node = parent[static_cast<size_t>(node)]) {
        chain.push_back(node);
    }
    std::reverse(chain.begin(), chain.end());
    std::vector<Vec3> path;
    path.reserve(chain.size() + 1);
    for (const int node : chain) {
        path.push_back(NodePoint(levels, offsets, node));
    }
    path.push_back(to);
    return path;
}

} // namespace

std::vector<Vec3> NavPath(const MapDef& map, const Vec3& from, const Vec3& to) {
    const NavGrid grid = MakeGrid(map);
    if (grid.nx <= 1 || grid.ny <= 1) {
        return {};
    }
    const int start_cell = CellIndex(grid, CellAt(grid, from, 0), CellAt(grid, from, 1));
    const int goal_cell = CellIndex(grid, CellAt(grid, to, 0), CellAt(grid, to, 1));
    if (start_cell == goal_cell) {
        return {to};
    }
    // 目标点本身被实体占据（如包点被墙覆盖）→ 不可达。
    if (!BodyFree(map, to.x, to.y, to.z)) {
        return {};
    }
    const std::vector<std::vector<Vec3>> levels = BuildLevels(map, grid);
    std::vector<int> offsets(static_cast<size_t>(grid.nx * grid.ny) + 1, 0);
    for (size_t cell = 0; cell < levels.size(); ++cell) {
        offsets[cell + 1] = offsets[cell] + static_cast<int>(levels[cell].size());
    }
    const int total = offsets.back();
    if (total == 0) {
        return {};
    }
    const int start_level = NearestLevel(levels[static_cast<size_t>(start_cell)], from.z);
    const int goal_level = NearestLevel(levels[static_cast<size_t>(goal_cell)], to.z);
    if (start_level < 0 || goal_level < 0) {
        return {};
    }
    // 目标高度必须与该格可站高度接近（防止“目标在屋顶/实体里”返回假路径）。
    const float goal_z = levels[static_cast<size_t>(goal_cell)][static_cast<size_t>(goal_level)].z;
    if (std::fabs(goal_z - to.z) > kStepUp + kEpsilon) {
        return {};
    }
    const int start_node = offsets[static_cast<size_t>(start_cell)] + start_level;
    const int goal_node = offsets[static_cast<size_t>(goal_cell)] + goal_level;
    if (start_node == goal_node) {
        return {to};
    }
    std::vector<int> parent(static_cast<size_t>(total), -1);
    std::vector<uint8_t> seen(static_cast<size_t>(total), 0);
    std::vector<int> queue;
    queue.reserve(static_cast<size_t>(total));
    queue.push_back(start_node);
    seen[static_cast<size_t>(start_node)] = 1;
    // 8 邻接 + 同格上下层（台阶内部的高差 ≤32 也允许）。
    const int kDx[9] = {1, -1, 0, 0, 1, 1, -1, -1, 0};
    const int kDy[9] = {0, 0, 1, -1, 1, -1, 1, -1, 0};
    bool found = false;
    for (size_t head = 0; head < queue.size() && !found; ++head) {
        const int current = queue[head];
        const Vec3 a = NodePoint(levels, offsets, current);
        const int current_cell = CellOfNode(offsets, current);
        const int cx = current_cell % grid.nx;
        const int cy = current_cell / grid.nx;
        for (int k = 0; k < 9 && !found; ++k) {
            const int nx = cx + kDx[k];
            const int ny = cy + kDy[k];
            if (nx < 0 || ny < 0 || nx >= grid.nx || ny >= grid.ny) {
                continue;
            }
            const int next_cell = CellIndex(grid, nx, ny);
            const std::vector<Vec3>& next_levels = levels[static_cast<size_t>(next_cell)];
            for (size_t level = 0; level < next_levels.size(); ++level) {
                const int next_node =
                    offsets[static_cast<size_t>(next_cell)] + static_cast<int>(level);
                if (seen[static_cast<size_t>(next_node)] != 0) {
                    continue;
                }
                const Vec3 b = next_levels[level];
                const float rise = b.z - a.z;
                if (rise > kStepUp + kEpsilon || rise < -kMaxDrop) {
                    continue;
                }
                if (!SegmentFree(map, a, b)) {
                    continue;
                }
                seen[static_cast<size_t>(next_node)] = 1;
                parent[static_cast<size_t>(next_node)] = current;
                if (next_node == goal_node) {
                    found = true;
                    break;
                }
                queue.push_back(next_node);
            }
        }
    }
    if (!found) {
        return {};
    }
    return BuildPath(levels, offsets, parent, start_node, goal_node, to);
}

} // namespace recoil
