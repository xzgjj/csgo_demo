#pragma once

// 宿主读取用的平面快照（SimSnapshot）。
// 目的：把每帧要用的标量一次性拷出内核，替代表现层对 40+ 个 getter 的逐字段拉取。
// 布局固定、只读投影：快照不参与模拟，也不进 HashState。

#include <cstdint>

#include "recoil/sim/state.h"

namespace recoil {

// 每玩家步长（float 槽位）。新增字段只能追加在尾部，已有下标不可改（GDScript 依赖）。
constexpr int kSnapshotPlayerStride = 24;

namespace snapshot_field {

constexpr int kX = 0;
constexpr int kY = 1;
constexpr int kZ = 2;
constexpr int kVx = 3;
constexpr int kVy = 4;
constexpr int kVz = 5;
constexpr int kYaw = 6;
constexpr int kPitch = 7;
constexpr int kRecoilPitch = 8;
constexpr int kRecoilYaw = 9;
constexpr int kHealth = 10;
constexpr int kArmor = 11;
constexpr int kAlive = 12;
constexpr int kDucking = 13;
constexpr int kTeam = 14;
constexpr int kActiveWeapon = 15;
constexpr int kPrimaryWeapon = 16;
constexpr int kSecondaryWeapon = 17;
constexpr int kMag = 18;
constexpr int kReserve = 19;
constexpr int kMoney = 20;
constexpr int kBlindTicks = 21;
constexpr int kInaccuracy = 22;
constexpr int kReserved = 23;

} // namespace snapshot_field

struct SimSnapshot {
    uint64_t tick = 0;
    uint16_t round_number = 0;
    uint16_t phase_ticks = 0;
    uint16_t score_t = 0;
    uint16_t score_ct = 0;
    uint16_t bomb_timer = 0;
    uint16_t smoke_ticks_left = 0;
    float smoke_x = 0.0f;
    float smoke_y = 0.0f;
    float smoke_radius = 0.0f;
    uint8_t player_count = 0;
    uint8_t round_phase = 0;
    uint8_t smoke_active = 0;
    // 0=未下包 1=已下包 2=拆包中 3=已拆除 4=已爆炸
    uint8_t bomb_phase = 0;
    float players[kMaxPlayers][kSnapshotPlayerStride]{};
};

// 激活槽武器：与 core 的 ActiveWeaponId 语义一致（0 号槽=主武器，1 号槽=副武器）。
inline uint8_t SnapshotActiveWeapon(const PlayerState& player) {
    return player.active_slot != 0 ? player.secondary_id : player.primary_id;
}

inline void BuildSnapshot(const WorldState& state, SimSnapshot& out) {
    out.tick = state.tick;
    out.player_count = state.player_count;
    out.round_number = state.round.number;
    out.round_phase = static_cast<uint8_t>(state.round.phase);
    out.phase_ticks = state.round.phase_ticks;
    out.score_t = state.round.score_t;
    out.score_ct = state.round.score_ct;
    out.bomb_timer = state.bomb.timer_ticks;
    if (state.bomb.exploded) {
        out.bomb_phase = 4;
    } else if (state.bomb.defused) {
        out.bomb_phase = 3;
    } else if (state.bomb.defusing) {
        out.bomb_phase = 2;
    } else if (state.bomb.planted) {
        out.bomb_phase = 1;
    } else {
        out.bomb_phase = 0;
    }
    out.smoke_active = state.smoke.active ? 1 : 0;
    out.smoke_x = state.smoke.position.x;
    out.smoke_y = state.smoke.position.y;
    out.smoke_radius = state.smoke.radius;
    out.smoke_ticks_left = state.smoke.ticks_left;
    for (uint8_t i = 0; i < kMaxPlayers; ++i) {
        const PlayerState& player = state.players[i];
        float* row = out.players[i];
        row[snapshot_field::kX] = player.position.x;
        row[snapshot_field::kY] = player.position.y;
        row[snapshot_field::kZ] = player.position.z;
        row[snapshot_field::kVx] = player.velocity.x;
        row[snapshot_field::kVy] = player.velocity.y;
        row[snapshot_field::kVz] = player.velocity.z;
        row[snapshot_field::kYaw] = player.view_yaw;
        row[snapshot_field::kPitch] = 0.0f; // 保留槽位：视图俯仰由宿主命令携带，不在状态里
        row[snapshot_field::kRecoilPitch] = player.recoil_pitch;
        row[snapshot_field::kRecoilYaw] = player.recoil_yaw;
        row[snapshot_field::kHealth] = static_cast<float>(player.health);
        row[snapshot_field::kArmor] = static_cast<float>(player.armor);
        row[snapshot_field::kAlive] = player.alive ? 1.0f : 0.0f;
        row[snapshot_field::kDucking] = player.ducking ? 1.0f : 0.0f;
        row[snapshot_field::kTeam] = static_cast<float>(player.team);
        row[snapshot_field::kActiveWeapon] = static_cast<float>(SnapshotActiveWeapon(player));
        row[snapshot_field::kPrimaryWeapon] = static_cast<float>(player.primary_id);
        row[snapshot_field::kSecondaryWeapon] = static_cast<float>(player.secondary_id);
        row[snapshot_field::kMag] = static_cast<float>(player.weapon.mag);
        row[snapshot_field::kReserve] = static_cast<float>(player.weapon.reserve);
        row[snapshot_field::kMoney] = static_cast<float>(player.money);
        row[snapshot_field::kBlindTicks] = static_cast<float>(player.blind_ticks);
        row[snapshot_field::kInaccuracy] = player.inaccuracy;
        row[snapshot_field::kReserved] = 0.0f;
    }
}

} // namespace recoil
