// RecoilSim 的只读 getter 组。
// 本文件**只允许**在 recoil_host.cpp 的类体内 #include（它直接访问 RecoilSim 成员，
// 没有独立的命名空间/类壳），目的是把 recoil_host.cpp 控制在 CI 单文件 400 行上限内。
// 不要在其它翻译单元单独包含本文件。
// clang-format off
    int get_bomb_planted() const { return state_.bomb.planted ? 1 : 0; }
    int get_bomb_timer() const { return state_.bomb.timer_ticks; }
    int get_wall_count() const { return recoil_host::WallCount(map_); }
    int get_round_number() const { return state_.round.number; }
    int get_alive_count(int team) const { return recoil_host::AliveCount(state_, team); }
    int get_has_bomb(int player) const { return recoil_host::HasBomb(state_, player); }

    // SPEC-06：队伍经济（该阵营全部玩家余额之和），购买 UI 显示团队经济条。
    int team_money(int team) const {
        int total = 0;
        for (uint8_t i = 0; i < state_.player_count; ++i) {
            if (static_cast<int>(state_.players[i].team) == team) {
                total += state_.players[i].money;
            }
        }
        return total;
    }
    int nav_path_points(float fx, float fy, float fz, float tx, float ty, float tz) const {
        return recoil_host::NavPointCount(map_, fx, fy, fz, tx, ty, tz);
    }
    PackedVector3Array nav_points(float fx, float fy, float fz, float tx, float ty,
                                  float tz) const {
        return recoil_host::NavPoints(map_, fx, fy, fz, tx, ty, tz);
    }

    // 供雷达/HUD：viewer → target 的几何视线（eye→chest，墙体 + 烟雾遮挡）。
    // 任一方阵亡、索引越界或自己看自己返回 false；不做阵营过滤（队友也返回 true），
    // "只显示敌人"由调用方按 get_team 过滤。
    bool is_visible(int viewer, int target) const {
        if (viewer < 0 || target < 0) {
            return false;
        }
        return recoil::HasLineOfSight(state_, &map_, static_cast<uint8_t>(viewer),
                                      static_cast<uint8_t>(target));
    }

    // 当前手上武器 id（WeaponId 编号，见 content/defs/weapons.json）。
    int get_weapon_id(int player) const {
        return static_cast<int>(recoil::ActiveWeaponId(state_.players[player]));
    }
    // 当前武器倍率（AK/M4=1.0、AWP=4.0）；副武器为 1.0。
    float get_weapon_scope(int player) const {
        return recoil::ActiveWeaponDef(state_.players[player], content_).scope_magnification;
    }
    // 当前武器弹匣容量（AWP=10、步枪=30）。
    int get_weapon_mag_capacity(int player) const {
        return recoil::ActiveWeaponDef(state_.players[player], content_).magazine;
    }
    // 模块 6：地面掉落武器查询（index < get_ground_weapon_count()；越界返回 0/-1）。
    int get_ground_weapon_count() const { return recoil::GroundWeaponCount(state_); }
    int get_ground_weapon_id(int index) const {
        if (index < 0 || index >= recoil::kMaxDrops) {
            return 0;
        }
        return static_cast<int>(state_.ground_weapons[index].weapon_id);
    }
    float get_ground_weapon_x(int index) const {
        if (index < 0 || index >= recoil::kMaxDrops) {
            return 0.0f;
        }
        return state_.ground_weapons[index].position.x;
    }
    float get_ground_weapon_y(int index) const {
        if (index < 0 || index >= recoil::kMaxDrops) {
            return 0.0f;
        }
        return state_.ground_weapons[index].position.y;
    }
    float get_ground_weapon_z(int index) const {
        if (index < 0 || index >= recoil::kMaxDrops) {
            return 0.0f;
        }
        return state_.ground_weapons[index].position.z;
    }
    int get_ground_weapon_mag(int index) const {
        if (index < 0 || index >= recoil::kMaxDrops) {
            return 0;
        }
        return state_.ground_weapons[index].mag;
    }
    // 玩家附近最近的可拾取掉落物索引；无则 -1。
    int get_nearest_ground_weapon(int player) const {
        if (player < 0 || player >= static_cast<int>(state_.player_count)) {
            return -1;
        }
        return recoil::NearestGroundWeapon(state_, state_.players[player].position,
                                           recoil::kPickupRadius);
    }
    // 模块 5：开火统计与朝向（表现层枪口/角色朝向一致）。
    int get_fire_count(int player) const { return state_.players[player].fire_count; }
    int get_last_fire_tick(int player) const {
        return static_cast<int>(state_.players[player].last_fire_tick);
    }
    float get_view_yaw(int player) const { return state_.players[player].view_yaw; }

    // 命中归因（模块 2 回归）：该玩家作为射手累计造成的 DamageApplied 次数；
    // 表现层只按它触发"自己的击中特效"，不再用全局伤害计数（否则 bot 交火会误触发玩家 FX）。
    int get_damage_count_by(int player) const {
        if (player < 0 || player >= recoil::kMaxPlayers) {
            return 0;
        }
        return damage_by_[player];
    }

    // 购买窗口（冻结期 + Live 开局宽限）：菜单灰显与点击前置校验用。
    bool can_buy(int player) const {
        if (player < 0 || player >= static_cast<int>(state_.player_count)) {
            return false;
        }
        if (content_.buy.enabled && !recoil::IsBuyWindowOpen(state_, content_)) {
            return false;
        }
        return recoil::CanBuy(state_, content_);
    }

    // SPEC-06：最近一次 BuyFailed 的 BuyResult 原因码（0=无失败，排错/UI 提示用）。
    int last_buy_failure() const {
        return last_buy_failure_;
    }

    // SPEC-06：目录条目能否购买（0=Ok，其余为 BuyResult 原因码），供 UI 显示精确灰显原因。
    int can_buy_entry(int player, int entry) const {
        recoil::BuyResult reason = recoil::BuyResult::Ok;
        if (player < 0 || entry < 0 ||
            !recoil::CanBuyEntry(state_, content_, static_cast<uint8_t>(player),
                                 static_cast<uint8_t>(entry), reason)) {
            return static_cast<int>(reason);
        }
        return 0;
    }

    // 换弹状态（HUD 换弹提示 + 换弹回归用例）。
    bool is_reloading(int player) const {
        if (player < 0 || player >= static_cast<int>(state_.player_count)) {
            return false;
        }
        return state_.players[player].weapon.reloading;
    }

    // SPEC-02 弹道只读：连发序号、当前额外扩散（度）、当前图案偏移（度）。
    int get_shot_index(int player) const {
        if (player < 0 || player >= static_cast<int>(state_.player_count)) {
            return 0;
        }
        return state_.players[player].shot_index;
    }
    float get_inaccuracy(int player) const {
        if (player < 0 || player >= static_cast<int>(state_.player_count)) {
            return 0.0f;
        }
        return state_.players[player].inaccuracy;
    }
    float get_recoil_pitch(int player) const {
        if (player < 0 || player >= static_cast<int>(state_.player_count)) {
            return 0.0f;
        }
        return state_.players[player].recoil_pitch;
    }
    float get_recoil_yaw(int player) const {
        if (player < 0 || player >= static_cast<int>(state_.player_count)) {
            return 0.0f;
        }
        return state_.players[player].recoil_yaw;
    }

    // 当前实际扩散（含 base 与姿态倍率，度）：HUD 准星张合用这一项。
    float get_effective_inaccuracy(int player) const {
        if (player < 0 || player >= static_cast<int>(state_.player_count)) {
            return 0.0f;
        }
        const recoil::PlayerState& owner = state_.players[player];
        const uint8_t weapon_id = owner.active_slot == 0 ? owner.primary_id : owner.secondary_id;
        const recoil::BallisticsDef& def =
            recoil::BallisticsFor(static_cast<recoil::WeaponId>(weapon_id));
        return recoil::EffectiveInaccuracy(owner, def.inaccuracy);
    }

    // 最近一次命中明细（hitmarker：部位/爆头/穿墙/距离）。
    int get_last_hit_group(int player) const {
        if (player < 0 || player >= static_cast<int>(state_.player_count)) {
            return 0;
        }
        return last_hit_by_[player].group;
    }
    int get_last_hit_flags(int player) const {
        if (player < 0 || player >= static_cast<int>(state_.player_count)) {
            return 0;
        }
        return last_hit_by_[player].flags;
    }
    // DEP-02：最近一次受击方向（相对视角，-180..180）与累计受击次数。
    float get_last_hit_direction(int player) const {
        if (player < 0 || player >= recoil::kMaxPlayers) {
            return 0.0f;
        }
        return hit_direction_[player];
    }

    int get_damage_taken(int player) const {
        if (player < 0 || player >= recoil::kMaxPlayers) {
            return 0;
        }
        return damage_taken_[player];
    }

    // DEP-02 声音层：上一 tick 的可听事件（枪声/换弹/投掷/爆炸）→ 位置、距离、相对朝向方位角。
    // 只报敌方：自己的枪声（distance=0）与队友脚步不构成“敌人方位”信息，否则 HUD 会闪出“声源 0 m”。
    godot::Array sound_events() const {
        godot::Array out;
        const recoil::PlayerState& human = state_.players[human_player_];
        for (int i = 0; i < last_events_.count; ++i) {
            const recoil::Event& event = last_events_.items[i];
            if (event.kind != recoil::EventKind::WeaponFired &&
                event.kind != recoil::EventKind::ReloadStarted &&
                event.kind != recoil::EventKind::GrenadeThrown &&
                event.kind != recoil::EventKind::GrenadeExploded) {
                continue;
            }
            if (event.source >= state_.player_count || event.source == human_player_) {
                continue;
            }
            const recoil::PlayerState& owner = state_.players[event.source];
            if (owner.team == human.team) {
                continue;
            }
            godot::Dictionary row;
            row["kind"] = static_cast<int>(event.kind);
            row["x"] = owner.position.x;
            row["y"] = owner.position.y;
            const float dx = owner.position.x - human.position.x;
            const float dy = owner.position.y - human.position.y;
            row["distance"] = std::sqrt(dx * dx + dy * dy);
            // 世界 bearing（0°=+y 前方）→ 相对视线 yaw，归一化到 [-180,180]；正数在右侧。
            const float bearing = std::atan2(dy, dx) * 57.2957795f;
            float relative = bearing - static_cast<float>(human.view_yaw);
            while (relative > 180.0f) {
                relative -= 360.0f;
            }
            while (relative < -180.0f) {
                relative += 360.0f;
            }
            row["angle"] = relative;
            row["tick"] = static_cast<int>(state_.tick);
            out.push_back(row);
        }
        return out;
    }

    float get_last_hit_distance(int player) const {
        if (player < 0 || player >= static_cast<int>(state_.player_count)) {
            return 0.0f;
        }
        return last_hit_by_[player].distance;
    }

    // 武器价格/名字（购买菜单用；未知 id 返回 0 / "-"）。
    int get_weapon_price(int weapon_id) const {
        return static_cast<int>(recoil::WeaponPrice(static_cast<recoil::WeaponId>(weapon_id)));
    }
    godot::String get_weapon_name(int weapon_id) const {
        return godot::String(recoil::WeaponName(static_cast<recoil::WeaponId>(weapon_id)));
    }

#define RECOIL_FLOAT_GETTER(name, expr)                                                            \
    float name(int player) const {                                                                 \
        return expr;                                                                               \
    }
#define RECOIL_INT_GETTER(name, expr)                                                              \
    int name(int player) const {                                                                   \
        return expr;                                                                               \
    }
#define RECOIL_GETTER0(name, expr) int name() const { return expr; }
#define RECOIL_KILL_GETTER(name, expr)                                                             \
    int name(int index) const {                                                                    \
        return expr;                                                                               \
    }
    RECOIL_FLOAT_GETTER(get_x, state_.players[player].position.x)
    RECOIL_FLOAT_GETTER(get_y, state_.players[player].position.y)
    RECOIL_FLOAT_GETTER(get_z, state_.players[player].position.z)
    RECOIL_INT_GETTER(get_health, state_.players[player].health)
    RECOIL_INT_GETTER(get_armor, state_.players[player].armor)
    RECOIL_INT_GETTER(get_helmet, state_.players[player].helmet ? 1 : 0)
    RECOIL_INT_GETTER(get_team, static_cast<int>(state_.players[player].team))
    RECOIL_GETTER0(get_kill_count, recoil_host::KillCount())
    RECOIL_KILL_GETTER(get_kill_shooter, recoil_host::KillShooter(index))
    RECOIL_KILL_GETTER(get_kill_victim, recoil_host::KillVictim(index))
    RECOIL_GETTER0(get_player_count, state_.player_count)
    RECOIL_INT_GETTER(get_mag, state_.players[player].weapon.mag)
    RECOIL_INT_GETTER(get_reserve, state_.players[player].weapon.reserve)
    RECOIL_INT_GETTER(get_kills, state_.players[player].kills)
    RECOIL_INT_GETTER(get_money, state_.players[player].money)
    RECOIL_INT_GETTER(get_blind_ticks, state_.players[player].blind_ticks)
    // 注意：以下三个是 0 参数 getter，不能用带 player 的宏（否则绑定参数个数与调用不符，
    // godot-cpp 在默认参数越界时 trap）。
    int get_smoke_active() const { return state_.smoke.active ? 1 : 0; }
    float get_smoke_x() const { return state_.smoke.position.x; }
    float get_smoke_y() const { return state_.smoke.position.y; }
    RECOIL_GETTER0(get_round_phase, static_cast<int>(state_.round.phase))
    RECOIL_GETTER0(get_round_ticks, state_.round.phase_ticks)
    RECOIL_GETTER0(get_score_t, state_.round.score_t)
    RECOIL_GETTER0(get_score_ct, state_.round.score_ct)
    RECOIL_GETTER0(get_tick, static_cast<int>(state_.tick))
    RECOIL_GETTER0(get_last_damage_events, _count(recoil::EventKind::DamageApplied))
    RECOIL_GETTER0(get_last_fire_events, _count(recoil::EventKind::WeaponFired))
    RECOIL_GETTER0(get_total_damage_events, total_damage_)
    RECOIL_GETTER0(get_total_fire_events, total_fire_)
    bool is_alive(int player) const { return state_.players[player].alive; }
#undef RECOIL_FLOAT_GETTER
#undef RECOIL_INT_GETTER
#undef RECOIL_GETTER0
#undef RECOIL_KILL_GETTER
// clang-format on
