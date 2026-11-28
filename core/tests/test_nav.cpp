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

TEST_CASE("导航越界点返回空路径而不是贴边钳制") {
    const MapDef map = OpenMap();
    CHECK(NavPath(map, {600.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}).empty());
    CHECK(NavPath(map, {0.0f, 0.0f, 0.0f}, {0.0f, -600.0f, 0.0f}).empty());
    CHECK(NavPath(map, {-600.0f, 0.0f, 0.0f}, {0.0f, 600.0f, 0.0f}).empty());
    // 边界上仍视为图内（闭区间）。
    CHECK(!NavPath(map, {512.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}).empty());
}

TEST_CASE("导航范围随地图 bounds 收缩后越界点被拒绝") {
    MapDef map = OpenMap();
    map.nav_min = {-256.0f, -256.0f, 0.0f};
    map.nav_max = {256.0f, 256.0f, 256.0f};
    const Vec3 from{0.0f, 0.0f, 0.0f};
    const Vec3 to{320.0f, 0.0f, 0.0f};

    CHECK(NavPath(map, from, to).empty());
}

// 多槽路径缓存：交替目标不能互相串味（旧单槽实现会把前一条挤掉，这里锁定语义）。
TEST_CASE("交替两个目标点时各自返回正确路线") {
    MapDef map = OpenMap();
    AddWall(map, {-96.0f, 128.0f, 0.0f}, {96.0f, 192.0f, 256.0f});
    const Vec3 from{0.0f, -300.0f, 0.0f};
    const Vec3 goal_a{0.0f, 300.0f, 0.0f};
    const Vec3 goal_b{-300.0f, 300.0f, 0.0f};

    const std::vector<Vec3> a_first = NavPath(map, from, goal_a);
    const std::vector<Vec3> b_first = NavPath(map, from, goal_b);
    REQUIRE(!a_first.empty());
    REQUIRE(!b_first.empty());
    CHECK(SamePoint(a_first.back(), goal_a));
    CHECK(SamePoint(b_first.back(), goal_b));

    // 第二次查询分别命中两个不同缓存槽，结果必须与首次逐点一致。
    const std::vector<Vec3> a_second = NavPath(map, from, goal_a);
    const std::vector<Vec3> b_second = NavPath(map, from, goal_b);
    REQUIRE(a_second.size() == a_first.size());
    REQUIRE(b_second.size() == b_first.size());
    for (size_t i = 0; i < a_first.size(); ++i) {
        CHECK(SamePoint(a_first[i], a_second[i]));
    }
    for (size_t i = 0; i < b_first.size(); ++i) {
        CHECK(SamePoint(b_first[i], b_second[i]));
    }
    CHECK(!SamePoint(a_second.back(), goal_b));
}

// 地图内容变化（可破坏墙被移除）后缓存必须失效，否则会沿旧绕行路线走。
TEST_CASE("移除墙体后缓存失效并改走更短路线") {
    MapDef map = OpenMap();
    AddWall(map, {-96.0f, 128.0f, 0.0f}, {96.0f, 192.0f, 256.0f});
    const Vec3 from{0.0f, -300.0f, 0.0f};
    const Vec3 to{0.0f, 300.0f, 0.0f};

    const std::vector<Vec3> detour = NavPath(map, from, to);
    REQUIRE(!detour.empty());
    REQUIRE(recoil::RemoveWall(map, 0));
    const std::vector<Vec3> straight = NavPath(map, from, to);

    REQUIRE(!straight.empty());
    CHECK(SamePoint(straight.back(), to));
    CHECK(straight.size() < detour.size());
}

// 空结果也进缓存：围死/无解的目标连续查询必须保持为空，不能被上一次成功路径污染。
TEST_CASE("空路径进缓存后重复查询仍为空") {
    MapDef map = OpenMap();
    AddWall(map, {-64.0f, 0.0f, 0.0f}, {0.0f, 64.0f, 256.0f});
    AddWall(map, {64.0f, 0.0f, 0.0f}, {128.0f, 64.0f, 256.0f});
    AddWall(map, {0.0f, -64.0f, 0.0f}, {64.0f, 0.0f, 256.0f});
    AddWall(map, {0.0f, 64.0f, 0.0f}, {64.0f, 128.0f, 256.0f});
    const Vec3 from{-400.0f, -400.0f, 0.0f};
    const Vec3 sealed{32.0f, 32.0f, 0.0f};

    CHECK(NavPath(map, from, sealed).empty());
    CHECK(NavPath(map, from, sealed).empty());
    CHECK(NavPath(map, {600.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}).empty());
    CHECK(NavPath(map, {600.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}).empty());
}

// 缓存命中条件沿用旧语义：同起终点格、两端 z 差 < 1u 视为同一条路径。
TEST_CASE("同格不同 z 在容差内复用缓存且点位一致") {
    MapDef map = OpenMap();
    AddWall(map, {-96.0f, 128.0f, 0.0f}, {96.0f, 192.0f, 256.0f});
    const Vec3 from{0.0f, -300.0f, 0.0f};
    const Vec3 to{0.0f, 300.0f, 0.0f};

    const std::vector<Vec3> ground = NavPath(map, from, to);
    const std::vector<Vec3> offset = NavPath(map, {0.0f, -300.0f, 0.5f}, {0.0f, 300.0f, 0.5f});

    REQUIRE(!ground.empty());
    REQUIRE(ground.size() == offset.size());
    for (size_t i = 0; i < ground.size(); ++i) {
        CHECK(SamePoint(ground[i], offset[i]));
    }
}
