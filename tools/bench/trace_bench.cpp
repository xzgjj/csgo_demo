// 137 盒线性 Trace 基准：决定是否需要宽相/BVH（P1-5）。
// 编译：g++ -std=c++20 -O2 -Icore/include tools/bench/trace_bench.cpp build/librecoil_core.a
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

#include "recoil/world/trace.h"

int main() {
    recoil::MapDef map;
    int made = 0;
    for (int i = 0; i < 137; ++i) {
        const float x = static_cast<float>((i % 12) * 160 - 960);
        const float y = static_cast<float>((i / 12) * 160 - 880);
        const float h = 32.0f + static_cast<float>((i * 37) % 96);
        recoil::Wall wall{{x, y, 0.0f}, {x + 64.0f, y + 64.0f, h}};
        if (recoil::AddWall(map, wall) == recoil::MapError::None) {
            ++made;
        }
    }
    constexpr int kRays = 200000;
    std::vector<float> micros(kRays);
    unsigned seed = 12345u;
    int hits = 0;
    for (int i = 0; i < kRays; ++i) {
        seed = seed * 1664525u + 1013904223u;
        const float yaw = static_cast<float>(seed % 6283u) * 0.001f;
        seed = seed * 1664525u + 1013904223u;
        const float pitch = (static_cast<float>(seed % 1000u) - 500.0f) * 0.001f;
        const recoil::Vec3 dir{std::cos(yaw) * std::cos(pitch), std::sin(yaw) * std::cos(pitch),
                               std::sin(pitch)};
        const auto start = std::chrono::high_resolution_clock::now();
        const recoil::TraceHit hit = recoil::TraceRay(map, {0.0f, 0.0f, 36.0f}, dir * 4000.0f);
        const auto end = std::chrono::high_resolution_clock::now();
        micros[static_cast<size_t>(i)] =
            std::chrono::duration<float, std::micro>(end - start).count();
        hits += hit.hit ? 1 : 0;
    }
    std::sort(micros.begin(), micros.end());
    std::printf("trace_bench boxes=%d rays=%d hits=%d p50=%.3fus p99=%.3fus max=%.3fus\n", made,
                kRays, hits, micros[kRays / 2], micros[kRays * 99 / 100], micros.back());
    return 0;
}
