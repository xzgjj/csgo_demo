// 导航寻路基准：300 次不同起终点（miss 路径）+ 300 次同点重复（缓存命中）。
// 用途：验证“搜索缓冲复用 + 多槽缓存”改动没有把寻路变慢，并给出 p50/p99 之外的平均耗时。
// 编译：g++ -std=c++20 -O2 -Icore/include tools/bench/nav_bench.cpp build/librecoil_core.a
#include <chrono>
#include <cstdio>
#include <vector>

#include "recoil/ai/nav.h"
#include "recoil/content/content.h"
#include "recoil/world/map.h"

int main() {
    recoil::MapDef map;
    for (int i = 0; i < 40; ++i) {
        const float x = static_cast<float>((i % 8) * 320 - 1120);
        const float y = static_cast<float>((i / 8) * 320 - 640);
        const float h = 96.0f + static_cast<float>((i * 29) % 96);
        recoil::Wall wall{{x, y, 0.0f}, {x + 160.0f, y + 96.0f, h}};
        recoil::AddWall(map, wall);
    }
    map.nav_min = {-2048.0f, -2048.0f, 0.0f};
    map.nav_max = {2048.0f, 2048.0f, 256.0f};

    std::vector<float> points;
    points.reserve(300);
    for (int i = 0; i < 300; ++i) {
        points.push_back(static_cast<float>((i * 37) % 1800 - 900));
    }

    double total = 0.0;
    int nonempty = 0;
    for (int i = 0; i < 300; ++i) {
        const recoil::Vec3 from{points[static_cast<size_t>(i)],
                                points[static_cast<size_t>((i * 7) % 300)], 0.0f};
        const recoil::Vec3 to{points[static_cast<size_t>((i * 11) % 300)],
                              points[static_cast<size_t>((i * 13) % 300)], 0.0f};
        const auto start = std::chrono::high_resolution_clock::now();
        const std::vector<recoil::Vec3> path = recoil::NavPath(map, from, to);
        const auto end = std::chrono::high_resolution_clock::now();
        total += std::chrono::duration<double, std::micro>(end - start).count();
        nonempty += path.empty() ? 0 : 1;
    }

    const recoil::Vec3 from{-1400.0f, -1200.0f, 0.0f};
    const recoil::Vec3 to{1200.0f, 1200.0f, 0.0f};
    recoil::NavPath(map, from, to);
    const auto hit_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 300; ++i) {
        recoil::NavPath(map, from, to);
    }
    const auto hit_end = std::chrono::high_resolution_clock::now();
    const double hit_total = std::chrono::duration<double, std::micro>(hit_end - hit_start).count();

    std::printf("miss: 300 queries total=%.1fus avg=%.2fus nonempty=%d\n", total, total / 300.0,
                nonempty);
    std::printf("hit : 300 cached queries total=%.1fus avg=%.2fus\n", hit_total, hit_total / 300.0);
    return 0;
}
