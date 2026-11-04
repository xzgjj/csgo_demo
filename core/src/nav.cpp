#include "recoil/ai/nav.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

#include "recoil/base/vec3.h"

namespace recoil {
namespace {

constexpr float kHalfWidth = 18.0f; // 比玩家半宽 16 多 2u 余量，避免贴角卡死
constexpr float kBodyHeight = 72.0f;
constexpr float kStepUp = 32.0f;
constexpr float kMaxDrop = 400.0f;
constexpr float kEpsilon = 1.0e-3f;
constexpr float kMaxWalkableTop = 200.0f;
constexpr float kMinWalkableTop = 8.0f;
constexpr float kSampleOffset = 18.0f;
constexpr int kMaxLevelsPerCell = 10;

struct NavGrid {
    int nx = 0;
    int ny = 0;
    float origin_x = 0.0f;
    float origin_y = 0.0f;
};

// 导航世界：墙体按格分桶 + 每格可站层懒计算，避免 NavPath 每次重建整张网。
struct NavWorld {
    const MapDef* map = nullptr;
    NavGrid grid;
    std::vector<std::vector<int>> buckets;
    std::vector<std::vector<Vec3>> levels;
    std::vector<int8_t> built;
};

int CountCells(float low, float high) {
    const float span = high - low;
    if (span <= 0.0f) {
        return 0;
    }
    return static_cast<int>(std::ceil(span / kNavCellSize));
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

int CellX(const NavGrid& grid, float x) {
    return ClampIndex(static_cast<int>(std::floor((x - grid.origin_x) / kNavCellSize)), grid.nx);
}

int CellY(const NavGrid& grid, float y) {
    return ClampIndex(static_cast<int>(std::floor((y - grid.origin_y) / kNavCellSize)), grid.ny);
}

float CenterX(const NavGrid& grid, int cx) {
    return grid.origin_x + (static_cast<float>(cx) + 0.5f) * kNavCellSize;
}

float CenterY(const NavGrid& grid, int cy) {
    return grid.origin_y + (static_cast<float>(cy) + 0.5f) * kNavCellSize;
}

void EnsureWorld(NavWorld& world, const MapDef& map) {
    world.map = &map;
    world.grid.nx = CountCells(map.nav_min.x, map.nav_max.x);
    world.grid.ny = CountCells(map.nav_min.y, map.nav_max.y);
    world.grid.origin_x = map.nav_min.x;
    world.grid.origin_y = map.nav_min.y;
    const int cells = world.grid.nx * world.grid.ny;
    world.buckets.assign(static_cast<size_t>(cells), {});
    world.levels.assign(static_cast<size_t>(cells), {});
    world.built.assign(static_cast<size_t>(cells), 0);
    // 墙体按“扩展后覆盖的格”入桶：查询点所在格即可覆盖玩家 AABB。
    for (int i = 0; i < map.wall_count; ++i) {
        const Wall& wall = map.walls[i];
        const int x0 = CellX(world.grid, wall.min.x - kHalfWidth);
        const int x1 = CellX(world.grid, wall.max.x + kHalfWidth);
        const int y0 = CellY(world.grid, wall.min.y - kHalfWidth);
        const int y1 = CellY(world.grid, wall.max.y + kHalfWidth);
        for (int cy = y0; cy <= y1; ++cy) {
            for (int cx = x0; cx <= x1; ++cx) {
                world.buckets[static_cast<size_t>(CellIndex(world.grid, cx, cy))].push_back(i);
            }
        }
    }
}

bool BodyFree(const NavWorld& world, float x, float y, float z) {
    const NavGrid& grid = world.grid;
    const int cell = CellIndex(grid, CellX(grid, x), CellY(grid, y));
    for (const int index : world.buckets[static_cast<size_t>(cell)]) {
        const Wall& wall = world.map->walls[index];
        if (x - kHalfWidth < wall.max.x && x + kHalfWidth > wall.min.x &&
            y - kHalfWidth < wall.max.y && y + kHalfWidth > wall.min.y && z < wall.max.z &&
            z + kBodyHeight > wall.min.z) {
            return false;
        }
    }
    return true;
}

// 候选高度必须有实体顶面支撑（防止生成“悬空导航层”，如台阶上方 32u 的假 64 层）。
bool SupportAt(const NavWorld& world, float x, float y, float z) {
    if (z <= 0.5f) {
        return true;
    }
    const NavGrid& grid = world.grid;
    const int cell = CellIndex(grid, CellX(grid, x), CellY(grid, y));
    for (const int index : world.buckets[static_cast<size_t>(cell)]) {
        const Wall& wall = world.map->walls[index];
        if (std::fabs(wall.max.z - z) <= 0.5f && x >= wall.min.x && x <= wall.max.x &&
            y >= wall.min.y && y <= wall.max.y) {
            return true;
        }
    }
    return false;
}

bool FreeSupportedPoint(const NavWorld& world, float x, float y, float z, Vec3& out) {
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
        const float px = x + offset[0];
        const float py = y + offset[1];
        if (BodyFree(world, px, py, z) && SupportAt(world, px, py, z)) {
            out = {px, py, z};
            return true;
        }
    }
    return false;
}

bool RectIntersects(const Wall& wall, float x0, float x1, float y0, float y1) {
    return wall.min.x < x1 && wall.max.x > x0 && wall.min.y < y1 && wall.max.y > y0;
}

// 懒计算某格的可站层（地面 + 实体顶面，按高度升序，最多 kMaxLevelsPerCell 层）。
void EnsureCellLevels(NavWorld& world, int cell) {
    if (world.built[static_cast<size_t>(cell)] != 0) {
        return;
    }
    world.built[static_cast<size_t>(cell)] = 1;
    const NavGrid& grid = world.grid;
    const int cx = cell % grid.nx;
    const int cy = cell / grid.nx;
    const float x0 = grid.origin_x + static_cast<float>(cx) * kNavCellSize;
    const float y0 = grid.origin_y + static_cast<float>(cy) * kNavCellSize;
    std::vector<float> candidates{0.0f};
    for (const int index : world.buckets[static_cast<size_t>(cell)]) {
        const Wall& wall = world.map->walls[index];
        if (!RectIntersects(wall, x0, x0 + kNavCellSize, y0, y0 + kNavCellSize)) {
            continue;
        }
        if (wall.max.z > kMinWalkableTop && wall.max.z <= kMaxWalkableTop) {
            candidates.push_back(wall.max.z);
        }
    }
    std::sort(candidates.begin(), candidates.end());
    std::vector<Vec3>& levels = world.levels[static_cast<size_t>(cell)];
    for (const float candidate : candidates) {
        if (static_cast<int>(levels.size()) >= kMaxLevelsPerCell) {
            break;
        }
        bool duplicate = false;
        for (const Vec3& existing : levels) {
            duplicate = duplicate || std::fabs(existing.z - candidate) <= 0.5f;
        }
        if (duplicate) {
            continue;
        }
        Vec3 point{};
        if (FreeSupportedPoint(world, CenterX(grid, cx), CenterY(grid, cy), candidate, point)) {
            levels.push_back(point);
        }
    }
    std::sort(levels.begin(), levels.end(), [](const Vec3& a, const Vec3& b) { return a.z < b.z; });
}

Vec3 NodePoint(NavWorld& world, int node) {
    const int cell = node / kMaxLevelsPerCell;
    EnsureCellLevels(world, cell);
    const int level = node % kMaxLevelsPerCell;
    const std::vector<Vec3>& levels = world.levels[static_cast<size_t>(cell)];
    if (level < static_cast<int>(levels.size())) {
        return levels[static_cast<size_t>(level)];
    }
    return {CenterX(world.grid, cell % world.grid.nx), CenterY(world.grid, cell / world.grid.nx),
            0.0f};
}

int NearestLevel(NavWorld& world, int cell, float z) {
    EnsureCellLevels(world, cell);
    const std::vector<Vec3>& levels = world.levels[static_cast<size_t>(cell)];
    int best = -1;
    float best_distance = std::numeric_limits<float>::max();
    for (size_t i = 0; i < levels.size(); ++i) {
        const float distance = std::fabs(levels[i].z - z);
        if (distance < best_distance) {
            best_distance = distance;
            best = static_cast<int>(i);
        }
    }
    return best;
}

bool HasLevelNear(NavWorld& world, int cell, float z) {
    EnsureCellLevels(world, cell);
    for (const Vec3& level : world.levels[static_cast<size_t>(cell)]) {
        if (std::fabs(level.z - z) <= 8.0f) {
            return true;
        }
    }
    return false;
}

bool SegmentFree(const NavWorld& world, const Vec3& a, const Vec3& b) {
    if (!BodyFree(world, a.x, a.y, a.z) || !BodyFree(world, b.x, b.y, b.z)) {
        return false;
    }
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / 12.0f)));
    const float z = std::max(a.z, b.z);
    for (int i = 1; i < steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        if (!BodyFree(world, a.x + dx * t, a.y + dy * t, z)) {
            return false;
        }
    }
    return true;
}

// 八邻接格距离启发式（octile）：与移动代价一致、可采纳。
float CellHeuristic(const NavGrid& grid, int a, int b) {
    const int ax = a % grid.nx;
    const int ay = a / grid.nx;
    const int bx = b % grid.nx;
    const int by = b / grid.nx;
    const int dx = std::abs(ax - bx);
    const int dy = std::abs(ay - by);
    return static_cast<float>(std::max(dx, dy)) +
           0.41421356f * static_cast<float>(std::min(dx, dy));
}

std::vector<Vec3> BuildPath(NavWorld& world, const std::vector<int>& parent,
                            const std::vector<int>& cell_of_node, int start, int goal,
                            const Vec3& to) {
    std::vector<int> chain;
    for (int node = goal; node != start && node >= 0; node = parent[static_cast<size_t>(node)]) {
        chain.push_back(node);
    }
    std::reverse(chain.begin(), chain.end());
    std::vector<Vec3> path;
    path.reserve(chain.size() + 1);
    for (const int node : chain) {
        const int cell = cell_of_node[static_cast<size_t>(node)];
        const int level = node % kMaxLevelsPerCell;
        EnsureCellLevels(world, cell);
        const std::vector<Vec3>& levels = world.levels[static_cast<size_t>(cell)];
        if (level < static_cast<int>(levels.size())) {
            path.push_back(levels[static_cast<size_t>(level)]);
        }
    }
    path.push_back(to);
    return path;
}

} // namespace

std::vector<Vec3> NavPath(const MapDef& map, const Vec3& from, const Vec3& to) {
    NavWorld world;
    EnsureWorld(world, map);
    const NavGrid& grid = world.grid;
    if (grid.nx <= 1 || grid.ny <= 1) {
        return {};
    }
    const int start_cell = CellIndex(grid, CellX(grid, from.x), CellY(grid, from.y));
    const int goal_cell = CellIndex(grid, CellX(grid, to.x), CellY(grid, to.y));
    if (start_cell == goal_cell) {
        return {to};
    }
    if (!BodyFree(world, to.x, to.y, to.z)) {
        return {};
    }
    const int start_level = NearestLevel(world, start_cell, from.z);
    const int goal_level = NearestLevel(world, goal_cell, to.z);
    if (start_level < 0 || goal_level < 0) {
        return {};
    }
    const float goal_z =
        world.levels[static_cast<size_t>(goal_cell)][static_cast<size_t>(goal_level)].z;
    if (std::fabs(goal_z - to.z) > kStepUp + kEpsilon) {
        return {};
    }
    const int node_count = grid.nx * grid.ny * kMaxLevelsPerCell;
    const int start_node = start_cell * kMaxLevelsPerCell + start_level;
    const int goal_node = goal_cell * kMaxLevelsPerCell + goal_level;
    if (start_node == goal_node) {
        return {to};
    }
    std::vector<int> parent(static_cast<size_t>(node_count), -1);
    std::vector<int> cell_of_node(static_cast<size_t>(node_count), -1);
    std::vector<float> g_score(static_cast<size_t>(node_count), std::numeric_limits<float>::max());
    std::vector<uint8_t> closed(static_cast<size_t>(node_count), 0);
    using QueueItem = std::pair<float, int>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> open;
    g_score[static_cast<size_t>(start_node)] = 0.0f;
    cell_of_node[static_cast<size_t>(start_node)] = start_cell;
    open.push({CellHeuristic(grid, start_cell, goal_cell), start_node});
    const int kDx[9] = {1, -1, 0, 0, 1, 1, -1, -1, 0};
    const int kDy[9] = {0, 0, 1, -1, 1, -1, 1, -1, 0};
    bool found = false;
    while (!open.empty() && !found) {
        const int current = open.top().second;
        open.pop();
        if (closed[static_cast<size_t>(current)] != 0) {
            continue;
        }
        closed[static_cast<size_t>(current)] = 1;
        if (current == goal_node) {
            found = true;
            break;
        }
        const int current_cell = cell_of_node[static_cast<size_t>(current)];
        const Vec3 a = NodePoint(world, current);
        const int cx = current_cell % grid.nx;
        const int cy = current_cell / grid.nx;
        for (int k = 0; k < 9; ++k) {
            const int nx = cx + kDx[k];
            const int ny = cy + kDy[k];
            if (nx < 0 || ny < 0 || nx >= grid.nx || ny >= grid.ny) {
                continue;
            }
            if (k >= 4 && k != 8) {
                // 对角移动要求两条正交邻格在起跳高度也有支撑，避免切角悬空。
                if (!HasLevelNear(world, CellIndex(grid, nx, cy), a.z) ||
                    !HasLevelNear(world, CellIndex(grid, cx, ny), a.z)) {
                    continue;
                }
            }
            const int next_cell = CellIndex(grid, nx, ny);
            EnsureCellLevels(world, next_cell);
            const std::vector<Vec3>& next_levels = world.levels[static_cast<size_t>(next_cell)];
            const float step_cost = (k >= 4 && k != 8) ? 1.41421356f : 1.0f;
            for (size_t level = 0; level < next_levels.size(); ++level) {
                const int next_node = next_cell * kMaxLevelsPerCell + static_cast<int>(level);
                const Vec3 b = next_levels[level];
                const float rise = b.z - a.z;
                if (rise > kStepUp + kEpsilon || rise < -kMaxDrop) {
                    continue;
                }
                if (!SegmentFree(world, a, b)) {
                    continue;
                }
                const float tentative = g_score[static_cast<size_t>(current)] + step_cost;
                if (tentative >= g_score[static_cast<size_t>(next_node)]) {
                    continue;
                }
                g_score[static_cast<size_t>(next_node)] = tentative;
                parent[static_cast<size_t>(next_node)] = current;
                cell_of_node[static_cast<size_t>(next_node)] = next_cell;
                open.push({tentative + CellHeuristic(grid, next_cell, goal_cell), next_node});
            }
        }
    }
    if (!found) {
        return {};
    }
    return BuildPath(world, parent, cell_of_node, start_node, goal_node, to);
}

} // namespace recoil
