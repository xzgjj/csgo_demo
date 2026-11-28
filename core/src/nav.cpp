#include "recoil/ai/nav.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

#include "nav_grid.h"
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

#include "nav_world.h" // 导航世界/路径缓存（依赖上面的 kHalfWidth 等常量）

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

// 该点是否落在坡道上；返回坡面标高（不在坡道内返回 found=false）。
// 坡道不是墙体，导航必须单独采样，否则 128u 平台只能靠台阶到达（spec-debug2 把台阶换成坡后
// 会出现「地图审计可达、bot 导航为空」的假阴性）。
bool RampSurfaceAt(const NavWorld& world, float x, float y, float& out_z) {
    for (int i = 0; i < world.map->ramp_count; ++i) {
        const Ramp& ramp = world.map->ramps[i];
        if (x < ramp.bounds.min.x || x > ramp.bounds.max.x || y < ramp.bounds.min.y ||
            y > ramp.bounds.max.y) {
            continue;
        }
        out_z = RampFloorAt(ramp, x, y);
        return true;
    }
    return false;
}

// 候选高度必须有实体顶面支撑（防止生成“悬空导航层”，如台阶上方 32u 的假 64 层）。
bool SupportAt(const NavWorld& world, float x, float y, float z) {
    if (z <= 0.5f) {
        return true;
    }
    float ramp_z = 0.0f;
    if (RampSurfaceAt(world, x, y, ramp_z) && std::fabs(ramp_z - z) <= 0.5f) {
        return true; // 坡面本身就是可站表面
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
    // 坡面采样：以格心坡高作为该格的可站层；坡面明显高于地面时不要同时保留 0 层，
    // 否则 A* 会生成「从坡体内部穿过去」的假路径（移动层会把 >32u 的抬脚判成墙）。
    float ramp_z = 0.0f;
    if (RampSurfaceAt(world, CenterX(grid, cx), CenterY(grid, cy), ramp_z)) {
        if (ramp_z > 0.5f) {
            candidates[0] = ramp_z;
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

// 坡道上的相邻格高差可以大于一级台阶（35° 坡在 64u 格距上抬 45u）。
// 规则：只要有一端贴在坡面上，就按「≤45° 可走斜面」放行，而不是按台阶 32u 判死；
// 这样地面格 → 坡脚格、坡面格 → 坡顶平台格都能连上。
bool RampLink(const NavWorld& world, const Vec3& a, const Vec3& b, float rise) {
    float za = 0.0f;
    float zb = 0.0f;
    const bool on_a = RampSurfaceAt(world, a.x, a.y, za) && std::fabs(za - a.z) <= 2.0f;
    const bool on_b = RampSurfaceAt(world, b.x, b.y, zb) && std::fabs(zb - b.z) <= 2.0f;
    if (!on_a && !on_b) {
        return false;
    }
    const float run = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
    return rise <= run + 1.0f; // 45° 上限，与 AddRamp / MAX_SLOPE 一致
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
    // 越界点直接拒绝：clamp 会生成“贴着边界”的错误路径，比空路径更危险。
    const float z_min = map.nav_min.z - kStepUp;
    const float z_max = map.nav_max.z + kStepUp;
    if (from.x < map.nav_min.x || from.x > map.nav_max.x || from.y < map.nav_min.y ||
        from.y > map.nav_max.y || from.z < z_min || from.z > z_max || to.x < map.nav_min.x ||
        to.x > map.nav_max.x || to.y < map.nav_min.y || to.y > map.nav_max.y || to.z < z_min ||
        to.z > z_max) {
        return {};
    }
    NavWorld& world = SharedWorld(map);
    const NavGrid& grid = world.grid;
    if (grid.nx <= 1 || grid.ny <= 1) {
        return {};
    }
    const int start_cell = CellIndex(grid, CellX(grid, from.x), CellY(grid, from.y));
    const int goal_cell = CellIndex(grid, CellX(grid, to.x), CellY(grid, to.y));
    if (world.cache_start == start_cell && world.cache_goal == goal_cell &&
        std::fabs(world.cache_from.z - from.z) < 1.0f &&
        std::fabs(world.cache_to.z - to.z) < 1.0f) {
        return world.cache_path;
    }
    auto remember = [&](std::vector<Vec3> path) {
        world.cache_start = start_cell;
        world.cache_goal = goal_cell;
        world.cache_from = from;
        world.cache_to = to;
        world.cache_path = path;
        return path;
    };
    if (start_cell == goal_cell) {
        return remember({to});
    }
    const int start_level = NearestLevel(world, start_cell, from.z);
    const int goal_level = NearestLevel(world, goal_cell, to.z);
    if (start_level < 0 || goal_level < 0) {
        return remember({});
    }
    // 目标高度允许被吸附到该格最近的可行走层（navmesh 的 3D 投影）：
    // 调用方常给“包点地面坐标”（z=0），而包点实际是 64/128 平台；旧逻辑按 |Δz|>32 直接拒绝，
    // 结果 bot 拿不到任何去包点的路径、整局原地不动。这里把终点改写到该层高度再校验净空。
    const Vec3 snapped_to{
        to.x, to.y,
        world.levels[static_cast<size_t>(goal_cell)][static_cast<size_t>(goal_level)].z};
    // 只允许「目标低于可走层」的向上吸附（包点地面坐标 z=0 → 64/128 平台），
    // 反向（目标在可走层上方，如屋顶 256）仍判不可达，保住 demo_map 的 roof 断言。
    if (snapped_to.z < to.z - kStepUp || snapped_to.z > to.z + 128.0f) {
        return remember({});
    }
    if (!BodyFree(world, snapped_to.x, snapped_to.y, snapped_to.z)) {
        return remember({});
    }
    const int node_count = grid.nx * grid.ny * kMaxLevelsPerCell;
    const int start_node = start_cell * kMaxLevelsPerCell + start_level;
    const int goal_node = goal_cell * kMaxLevelsPerCell + goal_level;
    if (start_node == goal_node) {
        return remember({to});
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
                if (rise < -kMaxDrop) {
                    continue;
                }
                if (rise > kStepUp + kEpsilon && !RampLink(world, a, b, rise)) {
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
        return remember({});
    }
    return remember(BuildPath(world, parent, cell_of_node, start_node, goal_node, snapped_to));
}

} // namespace recoil
