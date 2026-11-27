// RecoilSim 的地图注入/查询组（盒子、HP、导航范围、出生中心、包点）。
// 只能在 recoil_host.cpp 类体内 #include；目的与 getters 头一致：守住 400 行上限。
// clang-format off
    int set_map_boxes(const PackedFloat32Array& boxes) {
        return recoil_host::SetMapBoxes(map_, map_report_, boxes);
    }
    void set_map_wall_hp(const PackedInt32Array& hp) { recoil_host::LoadWallHp(map_, hp); }
    void set_map_frame(float min_x, float min_y, float max_x, float max_y, float tx, float ty,
                       float tz, float cx, float cy, float cz) {
        recoil_host::LoadMapFrame(map_, min_x, min_y, max_x, max_y, tx, ty, tz, cx, cy, cz);
    }
    int get_map_rejected() const { return map_report_.rejected; }
    int get_map_first_error() const { return static_cast<int>(map_report_.first_error); }
    void set_sites(float ax, float ay, float bx, float by) {
        sites_.site_a = {ax, ay, 0.0f};
        sites_.site_b = {bx, by, 0.0f};
    }
    void set_spawn_centers(float tx, float ty, float cx, float cy) {
        state_.spawn_t = {tx, ty, 0.0f};
        state_.spawn_ct = {cx, cy, 0.0f};
        state_.spawn_from_map = true;
    }
    int damage_wall(int index, int damage) {
        return static_cast<int>(recoil::DamageWall(map_, index, damage).result);
    }
    int get_wall_hp(int index) const {
        if (index < 0 || index >= map_.wall_count) {
            return -1;
        }
        return map_.wall_hp[index];
    }
// clang-format on
