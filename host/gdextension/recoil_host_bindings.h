// RecoilSim 的方法绑定表（Godot ClassDB）。
// 本文件**只允许**在 recoil_host.cpp 的 _bind_methods() 类体内 #include：
// 它展开为 BIND 语句，依赖 BIND 宏与 RecoilSim 成员，不能在其它翻译单元单独包含。
// 拆出目的：把 recoil_host.cpp 稳定压在 CI 单文件 400 行上限内。
// clang-format off
#define BIND(name, ...)                                                                            \
    ClassDB::bind_method(D_METHOD(#name __VA_OPT__(, ) __VA_ARGS__), &RecoilSim::name)
        BIND(reset);
        BIND(set_move, "player", "yaw", "pitch", "forward", "side", "jump", "duck");
        BIND(fire, "player", "yaw", "pitch");
        BIND(reload, "player");
        BIND(buy_armor, "player");
        BIND(plant_bomb, "player");
        BIND(defuse_bomb, "player");
        BIND(get_bomb_planted);
        BIND(get_bomb_timer);
        BIND(get_helmet, "player");
        BIND(get_team, "player");
        BIND(get_player_count);
        BIND(get_kill_count);
        BIND(get_kill_shooter, "index");
        BIND(get_kill_victim, "index");
        BIND(get_round_phase);
        BIND(get_round_ticks);
        BIND(get_score_t);
        BIND(get_score_ct);
        BIND(get_money, "player");
        BIND(throw_grenade, "player", "kind", "yaw", "pitch");
        BIND(get_smoke_active);
        BIND(get_smoke_x);
        BIND(get_smoke_y);
        BIND(get_blind_ticks, "player");
        BIND(tick);
        BIND(get_x, "player");
        BIND(get_y, "player");
        BIND(get_z, "player");
        BIND(get_health, "player");
        BIND(get_armor, "player");
        BIND(get_mag, "player");
        BIND(get_reserve, "player");
        BIND(is_alive, "player");
        BIND(get_kills, "player");
        BIND(get_tick);
        BIND(get_last_damage_events);
        BIND(get_last_fire_events);
        BIND(get_total_damage_events);
        BIND(get_total_fire_events);
        BIND(set_map_boxes, "boxes");
        BIND(set_sites, "ax", "ay", "bx", "by");
BIND(set_spawn_centers, "tx", "ty", "cx", "cy");
        BIND(begin_match);
        BIND(tick_auto);
        BIND(get_wall_count);
        BIND(get_round_number);
        BIND(get_alive_count, "team");
        BIND(get_has_bomb, "player");
        BIND(nav_path_points, "fx", "fy", "fz", "tx", "ty", "tz");
        BIND(nav_points, "fx", "fy", "fz", "tx", "ty", "tz");
        BIND(set_player_position, "player", "x", "y", "z");
        BIND(is_visible, "viewer", "target");
        BIND(buy_weapon, "player", "weapon_id");
        BIND(switch_weapon, "player", "slot");
        BIND(get_weapon_id, "player");
        BIND(get_weapon_scope, "player");
        BIND(get_weapon_mag_capacity, "player");
        BIND(get_weapon_price, "weapon_id");
        BIND(get_weapon_name, "weapon_id");
        BIND(pickup_weapon, "player");
        BIND(buy_helmet, "player");
        BIND(get_ground_weapon_count);
        BIND(get_ground_weapon_id, "index");
        BIND(get_ground_weapon_x, "index");
        BIND(get_ground_weapon_y, "index");
        BIND(get_ground_weapon_z, "index");
        BIND(get_ground_weapon_mag, "index");
        BIND(get_nearest_ground_weapon, "player");
        BIND(get_fire_count, "player");
        BIND(get_last_fire_tick, "player");
        BIND(get_view_yaw, "player");
        BIND(get_damage_count_by, "player");
        BIND(can_buy, "player");
        BIND(is_reloading, "player");
        // SPEC-02 弹道只读接口：HUD 准星扩散 / 相机抬枪 / 验收断言共用。
        BIND(get_shot_index, "player");
        BIND(get_inaccuracy, "player");
        BIND(get_recoil_pitch, "player");
        BIND(get_recoil_yaw, "player");
        BIND(get_effective_inaccuracy, "player");
        // SPEC-04 命中明细（hitmarker/死亡面板）。
        BIND(get_last_hit_group, "player");
        BIND(get_last_hit_flags, "player");
        BIND(get_last_hit_distance, "player");
        // SPEC-02 回滚/验收开关：false = 旧直线弹道（脚本化点射用例用）。
        BIND(set_ballistics_enabled, "enabled");
#undef BIND
// clang-format on
