#ifndef RECOIL_SRC_NAV_WORLD_H
#define RECOIL_SRC_NAV_WORLD_H

// 导航世界缓存（仅在 nav.cpp 的匿名命名空间内 include）：
// 墙体按格分桶 + 每格可站层懒计算 + 单条路径缓存，避免每次 NavPath 重建全图。

#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "nav_grid.h"
#include "recoil/base/vec3.h"
#include "recoil/world/map.h"

struct NavWorld {
    const MapDef* map = nullptr;
    NavGrid grid;
    std::vector<std::vector<int>> buckets;
    std::vector<std::vector<Vec3>> levels;
    std::vector<int8_t> built;
    // 单条路径缓存：AI 卡住时会用同一 (起点格, 目标格) 每 tick 重算，缓存空结果避免稳态抖动。
    int cache_start = -1;
    int cache_goal = -1;
    Vec3 cache_from{};
    Vec3 cache_to{};
    std::vector<Vec3> cache_path;
};

inline void EnsureWorld(NavWorld& world, const MapDef& map) {
    world.map = &map;
    world.grid.nx = CountCells(map.nav_min.x, map.nav_max.x);
    world.grid.ny = CountCells(map.nav_min.y, map.nav_max.y);
    world.grid.origin_x = map.nav_min.x;
    world.grid.origin_y = map.nav_min.y;
    const int cells = world.grid.nx * world.grid.ny;
    world.buckets.assign(static_cast<size_t>(cells), {});
    world.levels.assign(static_cast<size_t>(cells), {});
    world.built.assign(static_cast<size_t>(cells), 0);
    world.cache_start = -1;
    world.cache_goal = -1;
    world.cache_path.clear();
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

// 地图内容指纹：同一地址上的 MapDef 可能被复用（测试里尤为常见），只比指针/墙数会串缓存。
inline uint64_t MapFingerprint(const MapDef& map) {
    uint64_t h = 1469598103934665603ULL;
    auto mix = [&h](float value) {
        h ^= static_cast<uint64_t>(std::bit_cast<uint32_t>(value));
        h *= 1099511628211ULL;
    };
    mix(map.nav_min.x);
    mix(map.nav_min.y);
    mix(map.nav_min.z);
    mix(map.nav_max.x);
    mix(map.nav_max.y);
    mix(map.nav_max.z);
    for (int i = 0; i < map.wall_count; ++i) {
        mix(map.walls[i].min.x);
        mix(map.walls[i].min.y);
        mix(map.walls[i].min.z);
        mix(map.walls[i].max.x);
        mix(map.walls[i].max.y);
        mix(map.walls[i].max.z);
    }
    return h;
}

// 导航世界按地图缓存：墙桶/可站层只在地图切换或墙体内容变化（可破坏移除/换图）时重建。
// 之前每次 NavPath 都 EnsureWorld 重建全图，AI 稳态每 tick 调一次会把 p99 拉到 10ms+。
inline NavWorld& SharedWorld(const MapDef& map) {
    static NavWorld world;
    static uint64_t cached_key = 0;
    const uint64_t key = MapFingerprint(map);
    if (cached_key != key) {
        EnsureWorld(world, map);
        cached_key = key;
    }
    return world;
}

#endif
