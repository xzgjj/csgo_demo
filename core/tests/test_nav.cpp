#include <cmath>
#include <vector>

#include "doctest/doctest.h"
#include "recoil/ai/nav.h"
#include "recoil/world/map.h"

namespace {

using recoil::MapDef;
using recoil::NavPath;
using recoil::Vec3;
using recoil::Wall;

MapDef OpenMap() {
    MapDef map;
    map.nav_min = {-512.0f, -512.0f, 0.0f};
    map.nav_max = {512.0f, 512.0f, 256.0f};
    return map;
}

void AddWall(MapDef& map, const Vec3& low, const Vec3& high) {
    map.walls[map.wall_count] = Wall{low, high};
    ++map.wall_count;
}

bool InsideWall(const Vec3& point, const Wall& wall) {
    return point.x > wall.min.x && point.x < wall.max.x && point.y > wall.min.y &&
           point.y < wall.max.y;
}

bool SamePoint(const Vec3& a, const Vec3& b) {
    return std::fabs(a.x - b.x) < 1.0e-4f && std::fabs(a.y - b.y) < 1.0e-4f;
}

} // namespace

TEST_CASE("开阔地图上两点之间的路径以目标点结尾") {
    const MapDef map = OpenMap();
    const Vec3 from{0.0f, 0.0f, 0.0f};
    const Vec3 to{0.0f, 300.0f, 0.0f};

    const std::vector<Vec3> path = NavPath(map, from, to);

    REQUIRE(!path.empty());
    CHECK(SamePoint(path.back(), to));
    for (const Vec3& point : path) {
        CHECK(point.x >= map.nav_min.x);
        CHECK(point.x <= map.nav_max.x);
        CHECK(point.y >= map.nav_min.y);
        CHECK(point.y <= map.nav_max.y);
    }
}

TEST_CASE("墙体挡住直线时路径绕行且航点不穿墙") {
    MapDef map = OpenMap();
    AddWall(map, {-96.0f, 128.0f, 0.0f}, {96.0f, 192.0f, 256.0f});
    const Vec3 from{0.0f, 0.0f, 0.0f};
    const Vec3 to{0.0f, 320.0f, 0.0f};

    const std::vector<Vec3> path = NavPath(map, from, to);

    REQUIRE(path.size() >= 2);
    CHECK(SamePoint(path.back(), to));
    bool detoured = false;
    for (const Vec3& point : path) {
        CHECK(!InsideWall(point, map.walls[0]));
        detoured = detoured || std::fabs(point.x) > 100.0f;
    }
    CHECK(detoured);
}

TEST_CASE("目标被墙围死时返回空路径") {
    MapDef map = OpenMap();
    AddWall(map, {-64.0f, 0.0f, 0.0f}, {0.0f, 64.0f, 256.0f});
    AddWall(map, {64.0f, 0.0f, 0.0f}, {128.0f, 64.0f, 256.0f});
    AddWall(map, {0.0f, -64.0f, 0.0f}, {64.0f, 0.0f, 256.0f});
    AddWall(map, {0.0f, 64.0f, 0.0f}, {64.0f, 128.0f, 256.0f});
    const Vec3 from{-400.0f, -400.0f, 0.0f};
    const Vec3 to{32.0f, 32.0f, 0.0f};

    const std::vector<Vec3> path = NavPath(map, from, to);

    CHECK(path.empty());
}

TEST_CASE("起点与终点同格时直接返回目标点") {
    const MapDef map = OpenMap();
    const Vec3 from{0.0f, 0.0f, 0.0f};
    const Vec3 to{20.0f, 20.0f, 0.0f};

    const std::vector<Vec3> path = NavPath(map, from, to);

    REQUIRE(path.size() == 1);
    CHECK(SamePoint(path.front(), to));
}

TEST_CASE("目标格被墙占据时返回空路径") {
    MapDef map = OpenMap();
    AddWall(map, {-128.0f, -128.0f, 0.0f}, {-64.0f, -64.0f, 256.0f});
    const Vec3 from{0.0f, 0.0f, 0.0f};
    const Vec3 to{-100.0f, -100.0f, 0.0f};

    const std::vector<Vec3> path = NavPath(map, from, to);

    CHECK(path.empty());
}

TEST_CASE("导航路径是纯函数：相同输入得到相同结果") {
    MapDef map = OpenMap();
    AddWall(map, {-96.0f, 128.0f, 0.0f}, {96.0f, 192.0f, 256.0f});
    const Vec3 from{0.0f, 0.0f, 0.0f};
    const Vec3 to{0.0f, 320.0f, 0.0f};

    const std::vector<Vec3> first = NavPath(map, from, to);
    const std::vector<Vec3> second = NavPath(map, from, to);

    REQUIRE(first.size() == second.size());
    for (size_t i = 0; i < first.size(); ++i) {
        CHECK(SamePoint(first[i], second[i]));
    }
}

TEST_CASE("分层导航可经楼梯到达 +128 高台") {
    MapDef map = OpenMap();
    // 高台板：x 196..400、y 200..400，板面 112..128（顶面 128 可走，下方 112 以下可通行）。
    AddWall(map, {196.0f, 200.0f, 112.0f}, {400.0f, 400.0f, 128.0f});
    // 4 级楼梯，每级升 32，踏板顶面 32/64/96/128。
    AddWall(map, {68.0f, 200.0f, 0.0f}, {100.0f, 400.0f, 32.0f});
    AddWall(map, {100.0f, 200.0f, 0.0f}, {132.0f, 400.0f, 64.0f});
    AddWall(map, {132.0f, 200.0f, 0.0f}, {164.0f, 400.0f, 96.0f});
    AddWall(map, {164.0f, 200.0f, 0.0f}, {196.0f, 400.0f, 128.0f});
    const Vec3 from{0.0f, 300.0f, 0.0f};
    const Vec3 to{320.0f, 300.0f, 128.0f};

    const std::vector<Vec3> path = NavPath(map, from, to);

    REQUIRE(!path.empty());
    CHECK(path.back().z == doctest::Approx(128.0f).epsilon(0.5));
    bool used_stairs = false;
    for (const Vec3& point : path) {
        used_stairs = used_stairs || (point.x > 60.0f && point.x < 200.0f);
    }
    CHECK(used_stairs);
}

TEST_CASE("没有楼梯时 +128 高台不可达") {
    MapDef map = OpenMap();
    AddWall(map, {196.0f, 200.0f, 112.0f}, {400.0f, 400.0f, 128.0f});
    const Vec3 from{0.0f, 300.0f, 0.0f};
    const Vec3 to{320.0f, 300.0f, 128.0f};

    const std::vector<Vec3> path = NavPath(map, from, to);

    CHECK(path.empty());
}
