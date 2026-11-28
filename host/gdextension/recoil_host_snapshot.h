// RecoilSim 的只读快照接口（整帧一次取走）。
// 本文件**只允许**在 recoil_host.cpp 的类体内 #include（直接访问 state_）。
// 目的：替代表现层每帧 40+ 次 get_* 跨语言调用；字段布局见 core/include/recoil/sim/snapshot.h。
// clang-format off
    // 返回 {tick, player_count, stride, round_*, score_*, bomb_*, smoke_*, players}。
    // players 为 PackedFloat32Array（player_count × stride 的平面数组，下标用 gd 侧 SimSnapshot）。
    godot::Dictionary get_snapshot() const {
        recoil::SimSnapshot snap;
        recoil::BuildSnapshot(state_, snap);
        godot::Dictionary out;
        out["tick"] = static_cast<int64_t>(snap.tick);
        out["player_count"] = static_cast<int>(snap.player_count);
        out["stride"] = recoil::kSnapshotPlayerStride;
        out["round_number"] = static_cast<int>(snap.round_number);
        out["round_phase"] = static_cast<int>(snap.round_phase);
        out["phase_ticks"] = static_cast<int>(snap.phase_ticks);
        out["score_t"] = static_cast<int>(snap.score_t);
        out["score_ct"] = static_cast<int>(snap.score_ct);
        out["bomb_phase"] = static_cast<int>(snap.bomb_phase);
        out["bomb_timer"] = static_cast<int>(snap.bomb_timer);
        out["smoke_active"] = static_cast<int>(snap.smoke_active);
        out["smoke_x"] = snap.smoke_x;
        out["smoke_y"] = snap.smoke_y;
        out["smoke_radius"] = snap.smoke_radius;
        out["smoke_ticks_left"] = static_cast<int>(snap.smoke_ticks_left);
        const int count = static_cast<int>(snap.player_count);
        godot::PackedFloat32Array players;
        players.resize(count * recoil::kSnapshotPlayerStride);
        float* dst = players.ptrw();
        for (int i = 0; i < count; ++i) {
            for (int f = 0; f < recoil::kSnapshotPlayerStride; ++f) {
                dst[i * recoil::kSnapshotPlayerStride + f] = snap.players[i][f];
            }
        }
        out["players"] = players;
        return out;
    }
// clang-format on
