#include <algorithm>
#include <cmath>

#include "doctest/doctest.h"
#include "recoil/world/map.h"
#include "recoil/world/placement.h"

namespace {

// 造一个 128u 见方的实心障碍，底面贴地、高 96u（比跳跃 72 高，必须绕开）。
void AddBlock(recoil::MapDef& map, float cx, float cy, float half) {
    using recoil::Aabb;
    using recoil::AddWall;
    using recoil::Wall;
    const Wall wall{{cx - half, cy - half, 0.0f}, {cx + half, cy + half, 96.0f}};
    REQUIRE(AddWall(map, wall) == recoil::MapError::None);
}

} // namespace

TEST_CASE("落位检测：空地图中央为空、方块内部为占用") {
    recoil::MapDef map;
    const recoil::Vec3 free_spot{0.0f, 0.0f, 0.0f};
    CHECK(recoil::SpotFree(map, free_spot));

    AddBlock(map, 0.0f, 0.0f, 64.0f);
    CHECK_FALSE(recoil::SpotFree(map, free_spot));
}

TEST_CASE("落位检测：脚下顶面不算阻挡，头顶 73u 起才算") {
    recoil::MapDef map;
    // 一块悬空板：底 72.5u，站 72u 身高刚好擦过下沿而不相交。
    const recoil::Wall slab{{-64.0f, -64.0f, 72.5f}, {64.0f, 64.0f, 200.0f}};
    REQUIRE(recoil::AddWall(map, slab) == recoil::MapError::None);
    CHECK(recoil::SpotFree(map, recoil::Vec3{0.0f, 0.0f, 0.0f}));

    // 压到 71.0 就会重叠（0+72 > 71），不能站。
    recoil::MapDef lowered;
    const recoil::Wall low_slab{{-64.0f, -64.0f, 71.0f}, {64.0f, 64.0f, 200.0f}};
    REQUIRE(recoil::AddWall(lowered, low_slab) == recoil::MapError::None);
    CHECK_FALSE(recoil::SpotFree(lowered, recoil::Vec3{0.0f, 0.0f, 0.0f}));
}

TEST_CASE("落位搜索：被占时贴到最近空位且结果确定") {
    recoil::MapDef map;
    AddBlock(map, 0.0f, 0.0f, 48.0f); // 半宽 48 + 身宽 16 → 需要离中心至少 64u
    const recoil::Vec3 desired{0.0f, 0.0f, 0.0f};
    const recoil::Vec3 spot = recoil::FindFreeSpot(map, desired);

    CHECK(spot.z == doctest::Approx(desired.z));
    CHECK(recoil::SpotFree(map, spot));
    const float distance = std::max(std::fabs(spot.x), std::fabs(spot.y));
    CHECK(distance >= 32.0f);
    CHECK(distance <= recoil::kPlacementMaxRadius);

    // 确定性：同样的输入两次结果完全一致。
    const recoil::Vec3 again = recoil::FindFreeSpot(map, desired);
    CHECK(again.x == spot.x);
    CHECK(again.y == spot.y);
}

TEST_CASE("落位搜索：空地原样返回，不被无谓挪动") {
    recoil::MapDef map;
    const recoil::Vec3 desired{120.0f, -88.0f, 0.0f};
    const recoil::Vec3 spot = recoil::FindFreeSpot(map, desired);
    CHECK(spot.x == desired.x);
    CHECK(spot.y == desired.y);
    CHECK(spot.z == desired.z);
}

TEST_CASE("落位搜索：整圈被围住时退回设计点，不越界乱跑") {
    recoil::MapDef map;
    // 环形围墙：内半径 0..400 全是实体，192u 预算内无空位。
    AddBlock(map, 0.0f, 0.0f, 256.0f);
    const recoil::Vec3 desired{0.0f, 0.0f, 0.0f};
    const recoil::Vec3 spot = recoil::FindFreeSpot(map, desired);
    CHECK(spot.x == desired.x);
    CHECK(spot.y == desired.y);
    CHECK(spot.z == desired.z);
}
