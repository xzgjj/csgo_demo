#pragma once

#include <godot_cpp/variant/packed_float32_array.hpp>

#include "recoil/world/map.h"

namespace recoil_host {

// 把 Godot 侧 8 float/盒（min xyz / max xyz / surface / yaw）写入 MapDef。
// 遇到非法盒即停并返回已写入数量；调用方据返回值判断是否完整。
inline int LoadMapBoxes(recoil::MapDef& map, const godot::PackedFloat32Array& boxes) {
    map = recoil::MapDef{};
    constexpr int kStride = 8;
    const int count = boxes.size() / kStride;
    int written = 0;
    for (int i = 0; i < count; ++i) {
        const int base = i * kStride;
        recoil::Wall wall;
        wall.min = {static_cast<float>(boxes[base + 0]), static_cast<float>(boxes[base + 1]),
                    static_cast<float>(boxes[base + 2])};
        wall.max = {static_cast<float>(boxes[base + 3]), static_cast<float>(boxes[base + 4]),
                    static_cast<float>(boxes[base + 5])};
        wall.surface = static_cast<recoil::Surface>(static_cast<int>(boxes[base + 6]));
        wall.yaw_degrees = static_cast<float>(boxes[base + 7]);
        if (recoil::AddWall(map, wall) != recoil::MapError::None) {
            break;
        }
        ++written;
    }
    return written;
}

} // namespace recoil_host
