#include "recoil/ai/nav.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "recoil/base/vec3.h"

namespace recoil {
namespace {

struct NavGrid {
    int nx = 0;
    int ny = 0;
    float origin_x = 0.0f;
    float origin_y = 0.0f;
};

struct Cell {
    int x = 0;
    int y = 0;
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

Cell CellAt(const NavGrid& grid, const Vec3& point) {
    const int raw_x = static_cast<int>(std::floor((point.x - grid.origin_x) / kNavCellSize));
    const int raw_y = static_cast<int>(std::floor((point.y - grid.origin_y) / kNavCellSize));
    return {ClampIndex(raw_x, grid.nx), ClampIndex(raw_y, grid.ny)};
}

Vec3 CellCenter(const NavGrid& grid, const Cell& cell, float z) {
    return {grid.origin_x + (static_cast<float>(cell.x) + 0.5f) * kNavCellSize,
            grid.origin_y + (static_cast<float>(cell.y) + 0.5f) * kNavCellSize, z};
}

bool CellBlocked(const MapDef& map, const NavGrid& grid, const Cell& cell) {
    const float x0 = grid.origin_x + static_cast<float>(cell.x) * kNavCellSize;
    const float y0 = grid.origin_y + static_cast<float>(cell.y) * kNavCellSize;
    const float x1 = x0 + kNavCellSize;
    const float y1 = y0 + kNavCellSize;
    for (int i = 0; i < map.wall_count; ++i) {
        const Wall& wall = map.walls[i];
        if (wall.min.x < x1 && wall.max.x > x0 && wall.min.y < y1 && wall.max.y > y0) {
            return true;
        }
    }
    return false;
}

int CellIndex(const NavGrid& grid, const Cell& cell) {
    return cell.y * grid.nx + cell.x;
}

bool InGrid(const NavGrid& grid, const Cell& cell) {
    return cell.x >= 0 && cell.y >= 0 && cell.x < grid.nx && cell.y < grid.ny;
}

// BFS：从 start 到 goal 四邻接寻路，parent 记录每格前驱；返回是否连通。
bool BreadthFirst(const MapDef& map, const NavGrid& grid, int start, int goal,
                  std::vector<int>& parent) {
    const int total = grid.nx * grid.ny;
    std::vector<uint8_t> seen(static_cast<size_t>(total), 0);
    std::vector<int> queue;
    queue.reserve(static_cast<size_t>(total));
    queue.push_back(start);
    seen[static_cast<size_t>(start)] = 1;
    const int kDx[4] = {1, -1, 0, 0};
    const int kDy[4] = {0, 0, 1, -1};
    for (size_t head = 0; head < queue.size(); ++head) {
        const int current = queue[head];
        const Cell cell{current % grid.nx, current / grid.nx};
        for (int k = 0; k < 4; ++k) {
            const Cell next{cell.x + kDx[k], cell.y + kDy[k]};
            if (!InGrid(grid, next) || CellBlocked(map, grid, next)) {
                continue;
            }
            const int index = CellIndex(grid, next);
            if (seen[static_cast<size_t>(index)] != 0) {
                continue;
            }
            seen[static_cast<size_t>(index)] = 1;
            parent[static_cast<size_t>(index)] = current;
            if (index == goal) {
                return true;
            }
            queue.push_back(index);
        }
    }
    return false;
}

// 回溯 parent：航点取起点之后各格中心，末点用精确目标点替代。
std::vector<Vec3> BuildPath(const NavGrid& grid, const std::vector<int>& parent, int start,
                            int goal, const Vec3& to, float z) {
    std::vector<int> reversed;
    for (int current = goal; current != start && current >= 0;
         current = parent[static_cast<size_t>(current)]) {
        reversed.push_back(current);
    }
    std::reverse(reversed.begin(), reversed.end());
    std::vector<Vec3> path;
    path.reserve(reversed.size());
    for (size_t i = 0; i + 1 < reversed.size(); ++i) {
        const int index = reversed[i];
        path.push_back(CellCenter(grid, Cell{index % grid.nx, index / grid.nx}, z));
    }
    path.push_back(to);
    return path;
}

} // namespace

std::vector<Vec3> NavPath(const MapDef& map, const Vec3& from, const Vec3& to) {
    const NavGrid grid = MakeGrid(map);
    if (grid.nx <= 0 || grid.ny <= 0) {
        return {};
    }
    const int total = grid.nx * grid.ny;
    const Cell start_cell = CellAt(grid, from);
    const Cell goal_cell = CellAt(grid, to);
    const int start = CellIndex(grid, start_cell);
    const int goal = CellIndex(grid, goal_cell);
    if (start == goal) {
        return {to};
    }
    if (CellBlocked(map, grid, start_cell) || CellBlocked(map, grid, goal_cell)) {
        return {};
    }
    std::vector<int> parent(static_cast<size_t>(total), -1);
    if (!BreadthFirst(map, grid, start, goal, parent)) {
        return {};
    }
    return BuildPath(grid, parent, start, goal, to, map.nav_min.z);
}

} // namespace recoil
