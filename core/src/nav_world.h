#ifndef RECOIL_SRC_NAV_WORLD_H
#define RECOIL_SRC_NAV_WORLD_H

// 导航世界缓存（仅在 nav.cpp 的匿名命名空间内 include）：
// 墙体按格分桶 + 每格可站层懒计算 + 多槽路径缓存 + 复用搜索缓冲。
// 目标：稳态寻路不再 malloc/清零 4 条 O(节点数) 的数组，也不再被单槽缓存互相挤掉。

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "nav_grid.h"
#include "recoil/base/vec3.h"
#include "recoil/world/map.h"

// 路径缓存槽数：覆盖 5v5 两路包点 + 守点/回防的不同目标，避免稳态抖动。
constexpr size_t kNavCacheSlots = 8;

// 一条缓存记录：起点/终点格 + 两端坐标（z 容差 1u，与旧单槽语义一致）。
struct NavCacheEntry {
    int start_cell = -1;
    int goal_cell = -1;
    Vec3 from{};
    Vec3 to{};
    uint32_t used = 0;
    std::vector<Vec3> path;
};

struct NavWorld {
    const MapDef* map = nullptr;
    NavGrid grid;
    std::vector<std::vector<int>> buckets;
    std::vector<std::vector<Vec3>> levels;
    std::vector<int8_t> built;
    // 复用搜索缓冲：node_stamp/closed_stamp 记“本次搜索是否碰过”，用 epoch 代替逐次清零。
    std::vector<int> parent;
    std::vector<float> g_score;
    std::vector<uint32_t> node_stamp;
    std::vector<uint32_t> closed_stamp;
    uint32_t epoch = 0;
    // 多槽 LRU 路径缓存：不同 bot 目标各自命中，不互相驱逐。
    NavCacheEntry cache[kNavCacheSlots];
    uint32_t use_counter = 0;
};

// 搜索缓冲随节点数定型；地图换表但节点数不变时无需重建（epoch 已能隔开旧值）。
inline void EnsureSearchBuffers(NavWorld& world, int node_count) {
    if (static_cast<int>(world.g_score.size()) == node_count) {
        return;
    }
    world.parent.assign(static_cast<size_t>(node_count), -1);
    world.g_score.assign(static_cast<size_t>(node_count), 0.0f);
    world.node_stamp.assign(static_cast<size_t>(node_count), 0u);
    world.closed_stamp.assign(static_cast<size_t>(node_count), 0u);
    world.epoch = 0;
}

// 清空缓存：地图内容变化（换图/破坏墙）后旧路径不再成立。
inline void ClearPathCache(NavWorld& world) {
    world.use_counter = 0;
    for (NavCacheEntry& entry : world.cache) {
        entry.start_cell = -1;
        entry.goal_cell = -1;
        entry.used = 0;
        entry.path.clear();
    }
}

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
    EnsureSearchBuffers(world, cells * kMaxLevelsPerCell);
    ClearPathCache(world);
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

// 开始一次搜索：拿到本轮 epoch。32 位序号回绕时清表重来（极低频，代价可接受）。
inline uint32_t BeginSearch(NavWorld& world) {
    ++world.epoch;
    if (world.epoch == 0u) {
        std::fill(world.node_stamp.begin(), world.node_stamp.end(), 0u);
        std::fill(world.closed_stamp.begin(), world.closed_stamp.end(), 0u);
        world.epoch = 1u;
    }
    return world.epoch;
}

// 命中判定与旧单槽一致：同起终点格 + z 差 <1u；命中即刷新 LRU 时间戳。
inline const std::vector<Vec3>* NavCacheLookup(NavWorld& world, int start_cell, int goal_cell,
                                               const Vec3& from, const Vec3& to) {
    for (NavCacheEntry& entry : world.cache) {
        if (entry.start_cell != start_cell || entry.goal_cell != goal_cell) {
            continue;
        }
        if (std::fabs(entry.from.z - from.z) >= 1.0f || std::fabs(entry.to.z - to.z) >= 1.0f) {
            continue;
        }
        entry.used = ++world.use_counter;
        return &entry.path;
    }
    return nullptr;
}

// 写入缓存并返回槽内副本引用：优先空槽，否则驱逐最久未用。
inline const std::vector<Vec3>& NavCacheStore(NavWorld& world, int start_cell, int goal_cell,
                                              const Vec3& from, const Vec3& to,
                                              std::vector<Vec3> path) {
    NavCacheEntry* slot = nullptr;
    for (NavCacheEntry& entry : world.cache) {
        if (entry.start_cell < 0) {
            slot = &entry;
            break;
        }
        if (slot == nullptr || entry.used < slot->used) {
            slot = &entry;
        }
    }
    slot->start_cell = start_cell;
    slot->goal_cell = goal_cell;
    slot->from = from;
    slot->to = to;
    slot->used = ++world.use_counter;
    slot->path = std::move(path);
    return slot->path;
}

#endif
