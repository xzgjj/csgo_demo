#pragma once

#include <array>
#include <cstdint>

#include "recoil/base/vec3.h"

namespace recoil {

constexpr int kMaxPlayers = 10;

enum class Team : uint8_t {
    None = 0,
    T = 1,
    CT = 2,
};

enum class RoundPhase : uint8_t {
    Freeze = 0,
    Live = 1,
    RoundEnd = 2,
    MatchEnd = 3,
};

struct RoundState {
    RoundPhase phase = RoundPhase::Freeze;
    uint16_t phase_ticks = 0;
    uint16_t number = 1;
    uint16_t score_t = 0;
    uint16_t score_ct = 0;
    uint8_t winner = 0; // 0=无, 1=T, 2=CT
};

struct WeaponState {
    uint16_t mag = 0;
    uint16_t reserve = 0;
    uint16_t cooldown_ticks = 0;
    uint16_t reload_ticks_left = 0;
    bool reloading = false;
};

struct PlayerState {
    bool alive = false;
    bool helmet = false;
    bool on_ground = false;
    bool ducking = false;
    bool jump_queued = false;
    bool jump_held = false;
    uint8_t team = 0; // Team
    uint16_t health = 0;
    uint16_t armor = 0;
    uint16_t money = 0;
    uint16_t kills = 0;
    float wish_forward = 0.0f;
    float wish_side = 0.0f;
    Vec3 position{};
    Vec3 velocity{};
    WeaponState weapon{};
};

struct WorldState {
    uint64_t tick = 0;
    uint64_t rng = 0x9E3779B97F4A7C15ull;
    uint8_t player_count = 0;
    RoundState round{};
    std::array<PlayerState, kMaxPlayers> players{};
};

} // namespace recoil
