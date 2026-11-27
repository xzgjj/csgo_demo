#pragma once

// 导航网格索引工具（nav.cpp 内部头，不对外安装）。
// 单独抽出是为了让 nav.cpp 只保留寻路逻辑，文件行数符合 400 行预算。

#include <algorithm>
#include <cmath>

#include "recoil/ai/nav.h"
#include "recoil/base/vec3.h"

namespace recoil {
namespace {

struct NavGrid {
    int nx = 0;
    int ny = 0;
    float origin_x = 0.0f;
    float origin_y = 0.0f;
};

inline int CountCells(float low, float high) {
    const float span = high - low;
    if (span <= 0.0f) {
        return 0;
    }
    return static_cast<int>(std::ceil(span / kNavCellSize));
}

inline int ClampIndex(int value, int count) {
    if (count <= 0) {
        return 0;
    }
    return std::clamp(value, 0, count - 1);
}

inline int CellIndex(const NavGrid& grid, int cx, int cy) {
    return cy * grid.nx + cx;
}

inline int CellX(const NavGrid& grid, float x) {
    return ClampIndex(static_cast<int>(std::floor((x - grid.origin_x) / kNavCellSize)), grid.nx);
}

inline int CellY(const NavGrid& grid, float y) {
    return ClampIndex(static_cast<int>(std::floor((y - grid.origin_y) / kNavCellSize)), grid.ny);
}

inline float CenterX(const NavGrid& grid, int cx) {
    return grid.origin_x + (static_cast<float>(cx) + 0.5f) * kNavCellSize;
}

inline float CenterY(const NavGrid& grid, int cy) {
    return grid.origin_y + (static_cast<float>(cy) + 0.5f) * kNavCellSize;
}

} // namespace
} // namespace recoil
