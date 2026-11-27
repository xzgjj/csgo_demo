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

TEST_CASE("容量预算 512 时 762 盒被跳过 250 个并报告首个错误") {
    recoil::Wall walls[762]{};
    for (int i = 0; i < 762; ++i) {
        const float x = static_cast<float>(i) * 2.0f;
        walls[i] = recoil::Wall{{x, 0.0f, 0.0f}, {x + 1.0f, 1.0f, 1.0f}};
    }
    recoil::MapDef map;
    const recoil::Aabb nav{{-2300.0f, -2300.0f, 0.0f}, {2300.0f, 2300.0f, 288.0f}};
    const recoil::MapLoadReport report =
        recoil::LoadWalls(map, std::span<const recoil::Wall>(walls, 762), nav, 512);

    CHECK(report.written == 512);
    CHECK(report.rejected == 250);
    CHECK(report.first_error == recoil::MapError::TooManyWalls);
    CHECK(map.wall_count == 512);
    CHECK(map.nav_min.x == -2300.0f);
    CHECK(map.nav_max.y == 2300.0f);
}

TEST_CASE("yaw 90 度的盒归一成等价 AABB（宽深互换）") {
    recoil::MapDef map;
    recoil::Wall wall{{0.0f, 0.0f, 0.0f}, {100.0f, 200.0f, 50.0f}};
    wall.yaw_degrees = 90.0f;
    CHECK(recoil::AddWall(map, wall) == recoil::MapError::None);
    REQUIRE(map.wall_count == 1);
    CHECK(map.walls[0].min.x == doctest::Approx(-50.0f));
    CHECK(map.walls[0].min.y == doctest::Approx(50.0f));
    CHECK(map.walls[0].max.x == doctest::Approx(150.0f));
    CHECK(map.walls[0].max.y == doctest::Approx(150.0f));
    CHECK(map.walls[0].yaw_degrees == 0.0f);
}

TEST_CASE("yaw 180 与 yaw 270 归一后仍是轴对齐盒") {
    recoil::MapDef map;
    recoil::Wall wall_180{{0.0f, 0.0f, 0.0f}, {64.0f, 32.0f, 64.0f}};
    wall_180.yaw_degrees = 180.0f;
    CHECK(recoil::AddWall(map, wall_180) == recoil::MapError::None);
    CHECK(map.walls[0].min.x == doctest::Approx(0.0f));
    CHECK(map.walls[0].max.x == doctest::Approx(64.0f));

    recoil::Wall wall_270{{0.0f, 0.0f, 0.0f}, {100.0f, 40.0f, 64.0f}};
    wall_270.yaw_degrees = 270.0f;
    CHECK(recoil::AddWall(map, wall_270) == recoil::MapError::None);
    REQUIRE(map.wall_count == 2);
    CHECK(map.walls[1].min.x == doctest::Approx(30.0f));
    CHECK(map.walls[1].min.y == doctest::Approx(-30.0f));
    CHECK(map.walls[1].max.x == doctest::Approx(70.0f));
    CHECK(map.walls[1].max.y == doctest::Approx(70.0f));
}

TEST_CASE("非 90 度旋转只跳过该盒，其余盒照常写入") {
    recoil::Wall walls[3]{};
    walls[0] = recoil::Wall{{0.0f, 0.0f, 0.0f}, {16.0f, 16.0f, 16.0f}};
    walls[1] = recoil::Wall{{32.0f, 0.0f, 0.0f}, {48.0f, 16.0f, 16.0f}};
    walls[1].yaw_degrees = 45.0f;
    walls[2] = recoil::Wall{{64.0f, 0.0f, 0.0f}, {80.0f, 16.0f, 16.0f}};
    recoil::MapDef map;
    const recoil::Aabb nav{{-512.0f, -512.0f, 0.0f}, {512.0f, 512.0f, 256.0f}};
    const recoil::MapLoadReport report =
        recoil::LoadWalls(map, std::span<const recoil::Wall>(walls, 3), nav);

    CHECK(report.written == 2);
    CHECK(report.rejected == 1);
    CHECK(report.first_error == recoil::MapError::RotatedBrushUnsupported);
    CHECK(map.wall_count == 2);
}

TEST_CASE("非法盒被跳过而不是中断整图") {
    recoil::Wall walls[4]{};
    walls[0] = recoil::Wall{{0.0f, 0.0f, 0.0f}, {16.0f, 16.0f, 16.0f}};
    walls[1] = recoil::Wall{{20.0f, 20.0f, 20.0f}, {20.0f, 20.0f, 20.0f}}; // 零厚度
    walls[2] = recoil::Wall{{-40.0f, 0.0f, 0.0f}, {40.0f, 16.0f, 16.0f}};
    walls[3] = recoil::Wall{{80.0f, 0.0f, 0.0f}, {96.0f, 16.0f, 16.0f}};
    walls[3].surface = static_cast<recoil::Surface>(200);
    recoil::MapDef map;
    const recoil::Aabb nav{{-512.0f, -512.0f, 0.0f}, {512.0f, 512.0f, 256.0f}};
    const recoil::MapLoadReport report =
        recoil::LoadWalls(map, std::span<const recoil::Wall>(walls, 4), nav);

    CHECK(report.written == 2);
    CHECK(report.rejected == 2);
    CHECK(report.first_error == recoil::MapError::InvalidExtent);
    CHECK(map.wall_count == 2);
}

TEST_CASE("移除墙体时用尾盒回填并保持计数一致") {
    recoil::MapDef map;
    CHECK(recoil::AddWall(map, recoil::Wall{{0.0f, 0.0f, 0.0f}, {16.0f, 16.0f, 16.0f}}) ==
          recoil::MapError::None);
    CHECK(recoil::AddWall(map, recoil::Wall{{32.0f, 0.0f, 0.0f}, {48.0f, 16.0f, 16.0f}}) ==
          recoil::MapError::None);
    CHECK(recoil::RemoveWall(map, 0));
    REQUIRE(map.wall_count == 1);
    CHECK(map.walls[0].min.x == doctest::Approx(32.0f));
    CHECK_FALSE(recoil::RemoveWall(map, 5));
}
