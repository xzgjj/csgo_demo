#include "recoil/movement/movement.h"

#include <algorithm>
#include <cmath>

#include "recoil/base/vec3.h"

namespace recoil {
namespace {

constexpr float kEpsilon = 1.0e-4f;
constexpr float kMaxSubstepDistance = 8.0f;

bool Overlaps(const Vec3& lo, const Vec3& hi, const Wall& wall) {
    return lo.x < wall.max.x && hi.x > wall.min.x && lo.y < wall.max.y && hi.y > wall.min.y &&
           lo.z < wall.max.z && hi.z > wall.min.z;
}

void ApplyFriction(PlayerState& player, const MovementDef& def) {
    const float speed =
        std::sqrt(player.velocity.x * player.velocity.x + player.velocity.y * player.velocity.y);
    if (speed < 0.1f) {
        player.velocity.x = 0.0f;
        player.velocity.y = 0.0f;
        return;
    }
    const float control = std::max(speed, def.stop_speed);
    const float drop = control * def.friction * def.tick_dt;
    const float new_speed = std::max(0.0f, speed - drop);
    const float scale = new_speed / speed;
    player.velocity.x *= scale;
    player.velocity.y *= scale;
}

void Accelerate(PlayerState& player, const MovementDef& def, const Vec3& wish_dir, float wish_speed,
                float accel) {
    const float current = player.velocity.x * wish_dir.x + player.velocity.y * wish_dir.y;
    const float addspeed = wish_speed - current;
    if (addspeed <= 0.0f) {
        return;
    }
    const float accelspeed = std::min(accel * wish_speed * def.tick_dt, addspeed);
    player.velocity.x += wish_dir.x * accelspeed;
    player.velocity.y += wish_dir.y * accelspeed;
}

void MoveAxis(PlayerState& player, const MovementDef& def, const MapDef* map, int axis,
              float delta) {
    if (delta == 0.0f) {
        return;
    }
    if (map == nullptr) {
        if (axis == 0) {
            player.position.x += delta;
        } else if (axis == 1) {
            player.position.y += delta;
        } else {
            player.position.z += delta;
        }
        return;
    }
    const float height = player.ducking ? def.duck_height : def.stand_height;
    const int steps =
        std::max(1, static_cast<int>(std::ceil(std::fabs(delta) / kMaxSubstepDistance)));
    const float step = delta / static_cast<float>(steps);
    for (int i = 0; i < steps; ++i) {
        Vec3 next = player.position;
        if (axis == 0) {
            next.x += step;
        } else if (axis == 1) {
            next.y += step;
        } else {
            next.z += step;
        }
        const Vec3 lo = {next.x - def.half_width, next.y - def.half_width, next.z};
        const Vec3 hi = {next.x + def.half_width, next.y + def.half_width, next.z + height};
        bool blocked = false;
        for (int w = 0; w < map->wall_count; ++w) {
            const Wall& wall = map->walls[w];
            if (!Overlaps(lo, hi, wall)) {
                continue;
            }
            blocked = true;
            if (axis == 0) {
                next.x = step > 0.0f ? wall.min.x - def.half_width : wall.max.x + def.half_width;
                player.velocity.x = 0.0f;
            } else if (axis == 1) {
                next.y = step > 0.0f ? wall.min.y - def.half_width : wall.max.y + def.half_width;
                player.velocity.y = 0.0f;
            } else if (step > 0.0f) {
                next.z = wall.min.z - height;
                player.velocity.z = 0.0f;
            } else {
                next.z = wall.max.z;
                player.velocity.z = 0.0f;
                player.on_ground = true;
            }
        }
        player.position = next;
        if (blocked) {
            return;
        }
    }
}

void ResolveGround(PlayerState& player, const MapDef* map) {
    const float floor_z = (map != nullptr) ? map->floor_z : 0.0f;
    if (player.position.z <= floor_z + kEpsilon) {
        player.position.z = floor_z;
        player.velocity.z = 0.0f;
        player.on_ground = true;
        return;
    }
    player.on_ground = false;
}

bool StandsIntoWorld(const PlayerState& player, const MovementDef& def, const MapDef* map) {
    if (map == nullptr) {
        return false;
    }
    const Vec3 lo = {player.position.x - def.half_width, player.position.y - def.half_width,
                     player.position.z};
    const Vec3 hi = {player.position.x + def.half_width, player.position.y + def.half_width,
                     player.position.z + def.stand_height};
    for (int w = 0; w < map->wall_count; ++w) {
        if (Overlaps(lo, hi, map->walls[w])) {
            return true;
        }
    }
    return false;
}

} // namespace

void StepMovement(PlayerState& player, const MovementDef& def, const MapDef* map) {
    if (!player.alive) {
        return;
    }

    if (!player.ducking && StandsIntoWorld(player, def, map)) {
        player.ducking = true;
    }

    const float wish_len =
        std::sqrt(player.wish_forward * player.wish_forward + player.wish_side * player.wish_side);
    Vec3 wish_dir{};
    float wish_speed = 0.0f;
    if (wish_len > kEpsilon) {
        wish_dir = {player.wish_forward / wish_len, player.wish_side / wish_len, 0.0f};
        wish_speed = def.max_speed * std::min(1.0f, wish_len);
        if (player.ducking) {
            wish_speed = std::min(wish_speed, def.duck_speed);
        }
    }

    if (player.on_ground) {
        ApplyFriction(player, def);
        Accelerate(player, def, wish_dir, wish_speed, def.accelerate);
        if (player.jump_queued) {
            player.velocity.z = def.jump_impulse;
            player.on_ground = false;
            player.jump_queued = false;
        }
    } else {
        Accelerate(player, def, wish_dir, std::min(wish_speed, def.air_speed_cap),
                   def.air_accelerate);
        player.velocity.z -= def.gravity * def.tick_dt;
    }

    MoveAxis(player, def, map, 0, player.velocity.x * def.tick_dt);
    MoveAxis(player, def, map, 1, player.velocity.y * def.tick_dt);
    MoveAxis(player, def, map, 2, player.velocity.z * def.tick_dt);
    ResolveGround(player, map);
}

} // namespace recoil
