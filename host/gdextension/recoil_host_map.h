#pragma once

#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

#include "recoil/world/map.h"

namespace recoil_host {

// 加载结果复用内核 MapLoadReport：宿主据此拒绝开局而不是静默截断。
using MapLoadReport = recoil::MapLoadReport;

// 把 Godot 侧 8 float/盒（min xyz / max xyz / surface / yaw）写入 MapDef。
// 非法盒跳过并计数；不再 break（首个斜盒导致整图 0 盒是 MAP-04 的回归点）。
inline int SetMapBoxes(recoil::MapDef& map, MapLoadReport& report,
                       const godot::PackedFloat32Array& boxes) {
    map = recoil::MapDef{};
    report = MapLoadReport{};
    constexpr int kStride = 8;
    const int count = boxes.size() / kStride;
    for (int i = 0; i < count; ++i) {
        const int base = i * kStride;
        recoil::Wall wall;
        wall.min = {static_cast<float>(boxes[base + 0]), static_cast<float>(boxes[base + 1]),
                    static_cast<float>(boxes[base + 2])};
        wall.max = {static_cast<float>(boxes[base + 3]), static_cast<float>(boxes[base + 4]),
                    static_cast<float>(boxes[base + 5])};
        wall.surface = static_cast<recoil::Surface>(static_cast<int>(boxes[base + 6]));
        wall.yaw_degrees = static_cast<float>(boxes[base + 7]);
        const recoil::MapError error = recoil::AddWall(map, wall);
        if (error != recoil::MapError::None) {
            recoil::NoteReject(report, error);
            continue;
        }
        ++report.written;
    }
    return report.written;
}

// 逐盒 HP（与 written 顺序一致；0 = 不可破坏）。越界索引忽略。
inline int LoadWallHp(recoil::MapDef& map, const godot::PackedInt32Array& hp) {
    int applied = 0;
    const int count = hp.size() < map.wall_count ? hp.size() : map.wall_count;
    for (int i = 0; i < count; ++i) {
        const int value = hp[i];
        map.wall_hp[i] = static_cast<uint16_t>(value > 0 ? value : 0);
        ++applied;
    }
    return applied;
}

// 导航范围 + 出生中心注入（de_dust2 bounds/spawns；不调用时保持 de_ash 默认）。
inline void LoadMapFrame(recoil::MapDef& map, float min_x, float min_y, float max_x, float max_y,
                         float t_x, float t_y, float t_z, float ct_x, float ct_y, float ct_z) {
    map.nav_min = {min_x, min_y, 0.0f};
    map.nav_max = {max_x, max_y, 256.0f};
    map.spawn_t = {t_x, t_y, t_z};
    map.spawn_ct = {ct_x, ct_y, ct_z};
    map.custom_spawns = true;
}

} // namespace recoil_host
