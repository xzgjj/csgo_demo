#pragma once

#include "recoil/world/map.h"

namespace recoil_host {

// 2 人 demo 灰盒竞技场（±800，与既有场景/截图 harness 对齐）。
// de_ash 正式对局由宿主调用 RecoilSim::set_map_boxes 注入派生盒，不使用本函数。
inline recoil::MapDef MakeDemoArena() {
    recoil::MapDef map;
    const float half = 800.0f;
    const float thickness = 32.0f;
    const float height = 200.0f;
    map.wall_count = 4;
    map.walls[0] = {{half, -half, 0.0f}, {half + thickness, half, height}};
    map.walls[1] = {{-half - thickness, -half, 0.0f}, {-half, half, height}};
    map.walls[2] = {{-half, half, 0.0f}, {half, half + thickness, height}};
    map.walls[3] = {{-half, -half - thickness, 0.0f}, {half, -half, height}};
    return map;
}

} // namespace recoil_host
