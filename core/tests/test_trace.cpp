#include <cmath>

#include "doctest/doctest.h"
#include "recoil/world/trace.h"

namespace {

using recoil::Aabb;
using recoil::AddWall;
using recoil::MapDef;
using recoil::MapError;
using recoil::Surface;
using recoil::TraceFilter;
using recoil::TraceHit;
using recoil::TraceHull;
using recoil::TraceOverlap;
using recoil::TraceRay;
using recoil::Vec3;
using recoil::Wall;

MapDef OneWall(Surface surface = Surface::Concrete) {
    MapDef map;
    Wall wall{{100.0f, -50.0f, 0.0f}, {132.0f, 50.0f, 72.0f}};
    wall.surface = surface;
    (void)AddWall(map, wall);
    return map;
}

} // namespace

TEST_CASE("射线命中墙面：比例/法线/brush/材质正确") {
    const MapDef map = OneWall(Surface::Glass);
    const TraceHit hit = TraceRay(map, {0.0f, 0.0f, 36.0f}, {200.0f, 0.0f, 36.0f});

    CHECK(hit.hit);
    CHECK_FALSE(hit.startsolid);
    CHECK(hit.fraction == doctest::Approx(0.5f));
    CHECK(hit.point.x == doctest::Approx(100.0f));
    CHECK(hit.normal.x == doctest::Approx(-1.0f));
    CHECK(hit.normal.y == doctest::Approx(0.0f));
    CHECK(hit.brush == 0);
    CHECK(hit.surface == Surface::Glass);
}

TEST_CASE("射线平行于墙面且不相交时不命中") {
    const MapDef map = OneWall();
    const TraceHit hit = TraceRay(map, {0.0f, -60.0f, 36.0f}, {200.0f, -60.0f, 36.0f});

    CHECK_FALSE(hit.hit);
    CHECK(hit.fraction == doctest::Approx(1.0f));
}

TEST_CASE("射线起点在实体内部返回 startsolid 且比例归零") {
    const MapDef map = OneWall();
    const TraceHit hit = TraceRay(map, {110.0f, 0.0f, 36.0f}, {200.0f, 0.0f, 36.0f});

    CHECK(hit.hit);
    CHECK(hit.startsolid);
    CHECK(hit.fraction == doctest::Approx(0.0f));
    CHECK(hit.normal.x == doctest::Approx(0.0f));
}

TEST_CASE("零长度射线：外部不命中，内部报 startsolid") {
    const MapDef map = OneWall();
    const TraceHit outside = TraceRay(map, {0.0f, 0.0f, 36.0f}, {0.0f, 0.0f, 36.0f});
    const TraceHit inside = TraceRay(map, {110.0f, 0.0f, 36.0f}, {110.0f, 0.0f, 36.0f});

    CHECK_FALSE(outside.hit);
    CHECK(inside.hit);
    CHECK(inside.startsolid);
}

TEST_CASE("AABB 扫掠按玩家 hull 停在墙前") {
    const MapDef map = OneWall();
    const Aabb hull{{-16.0f, -16.0f, 0.01f}, {16.0f, 16.0f, 72.0f}};
    const TraceHit hit = TraceHull(map, hull, {0.0f, 0.0f, 0.0f}, {200.0f, 0.0f, 0.0f});

    CHECK(hit.hit);
    CHECK_FALSE(hit.startsolid);
    CHECK(hit.fraction == doctest::Approx(0.42f));
    CHECK(hit.point.x == doctest::Approx(84.0f));
    CHECK(hit.normal.x == doctest::Approx(-1.0f));
}

TEST_CASE("扫掠起点重叠时 fraction=0 且 startsolid") {
    const MapDef map = OneWall();
    const Aabb hull{{-16.0f, -16.0f, 0.01f}, {16.0f, 16.0f, 72.0f}};
    const TraceHit hit = TraceHull(map, hull, {90.0f, 0.0f, 0.0f}, {200.0f, 0.0f, 0.0f});

    CHECK(hit.hit);
    CHECK(hit.startsolid);
    CHECK(hit.fraction == doctest::Approx(0.0f));
}

TEST_CASE("重叠查询：贴面不算重叠，进入 1u 算重叠") {
    const MapDef map = OneWall();
    const Aabb hull{{-16.0f, -16.0f, 0.01f}, {16.0f, 16.0f, 72.0f}};

    CHECK_FALSE(TraceOverlap(map, hull, {84.0f, 0.0f, 0.0f}));
    CHECK(TraceOverlap(map, hull, {85.0f, 0.0f, 0.0f}));
}

TEST_CASE("Mask 关闭 World 时不返回任何命中") {
    const MapDef map = OneWall();
    TraceFilter filter;
    filter.mask = 0;
    const TraceHit hit = TraceRay(map, {0.0f, 0.0f, 36.0f}, {200.0f, 0.0f, 36.0f}, filter);

    CHECK_FALSE(hit.hit);
    CHECK_FALSE(TraceOverlap(map, Aabb{}, {110.0f, 0.0f, 36.0f}, filter));
}

TEST_CASE("ignore_brush 只跳过指定盒，不吞掉后续命中") {
    MapDef map;
    REQUIRE(AddWall(map, Wall{{100.0f, -50.0f, 0.0f}, {132.0f, 50.0f, 72.0f}}) == MapError::None);
    REQUIRE(AddWall(map, Wall{{200.0f, -50.0f, 0.0f}, {232.0f, 50.0f, 72.0f}}) == MapError::None);
    TraceFilter filter;
    filter.ignore_brush = 0;

    const TraceHit hit = TraceRay(map, {0.0f, 0.0f, 36.0f}, {300.0f, 0.0f, 36.0f}, filter);

    CHECK(hit.hit);
    CHECK(hit.brush == 1);
    CHECK(hit.fraction == doctest::Approx(200.0f / 300.0f));
}

TEST_CASE("地图 AddWall 拒绝非法尺寸/NaN/旋转/非法材质") {
    MapDef map;
    CHECK(AddWall(map, Wall{{10.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 10.0f}}) ==
          MapError::InvalidExtent);
    CHECK(AddWall(map, Wall{{0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 10.0f}}) == MapError::None);
    Wall rotated{{20.0f, 0.0f, 0.0f}, {30.0f, 10.0f, 10.0f}};
    rotated.yaw_degrees = 5.0f;
    CHECK(AddWall(map, rotated) == MapError::RotatedBrushUnsupported);
    Wall bad_surface{{20.0f, 0.0f, 0.0f}, {30.0f, 10.0f, 10.0f}};
    bad_surface.surface = static_cast<Surface>(200);
    CHECK(AddWall(map, bad_surface) == MapError::InvalidSurface);
    Wall nan{{std::nanf(""), 0.0f, 0.0f}, {30.0f, 10.0f, 10.0f}};
    CHECK(AddWall(map, nan) == MapError::NonFinite);
}

TEST_CASE("贴面水平扫掠不判 startsolid：站在平台顶面可继续走") {
    MapDef map;
    Wall platform{{-50.0f, -50.0f, 112.0f}, {50.0f, 50.0f, 128.0f}};
    REQUIRE(AddWall(map, platform) == MapError::None);
    const Aabb hull{{-16.0f, -16.0f, 0.0f}, {16.0f, 16.0f, 72.0f}};

    const TraceHit hit = TraceHull(map, hull, {0.0f, 0.0f, 128.0f}, {100.0f, 0.0f, 0.0f});

    CHECK_FALSE(hit.hit);
    CHECK_FALSE(TraceOverlap(map, hull, {0.0f, 0.0f, 128.0f}));
}

TEST_CASE("贴面离开不产生 0 比例命中") {
    MapDef map;
    Wall platform{{-50.0f, -50.0f, 112.0f}, {50.0f, 50.0f, 128.0f}};
    REQUIRE(AddWall(map, platform) == MapError::None);
    const Aabb hull{{-4.0f, -4.0f, -4.0f}, {4.0f, 4.0f, 4.0f}};

    const TraceHit hit = TraceHull(map, hull, {0.0f, 0.0f, 132.0f}, {0.0f, 0.0f, 1.0f});

    CHECK_FALSE(hit.hit);
    CHECK(hit.fraction == doctest::Approx(1.0f));
}
