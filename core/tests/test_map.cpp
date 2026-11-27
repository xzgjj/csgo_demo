#include "doctest/doctest.h"
#include "recoil/world/map.h"

TEST_CASE("地图盒数达到上限后拒绝新盒且不改 wall_count") {
    recoil::MapDef map;
    for (int i = 0; i < recoil::kMaxWalls; ++i) {
        const float x = static_cast<float>(i) * 2.0f;
        REQUIRE(recoil::AddWall(map, recoil::Wall{{x, 0.0f, 0.0f}, {x + 1.0f, 1.0f, 1.0f}}) ==
                recoil::MapError::None);
    }
    CHECK(map.wall_count == recoil::kMaxWalls);
    const recoil::Wall overflow{{0.0f, 5.0f, 0.0f}, {1.0f, 6.0f, 1.0f}};
    CHECK(recoil::AddWall(map, overflow) == recoil::MapError::TooManyWalls);
    CHECK(map.wall_count == recoil::kMaxWalls);
}
