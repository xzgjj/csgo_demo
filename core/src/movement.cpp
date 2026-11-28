#include "recoil/movement/movement.h"

#include <algorithm>
#include <cmath>

#include "recoil/base/vec3.h"

namespace recoil {
namespace {

constexpr float kEpsilon = 1.0e-4f;
constexpr float kMaxSubstepDistance = 8.0f;
// 可直接迈上的台阶高度（与地图/导航规则一致：step 32 / jump 72，≥73 的台面设计上不可达）。
constexpr float kStepHeight = 32.0f;
constexpr float kSupportEpsilon = 0.6f;

Aabb BodyHull(const MovementDef& def, float height) {
    // 底部贴 0：贴面由 TraceHull 的“平行/离开”规则排除，避免站在平台上下沉 0.01u。
    return Aabb{{-def.half_width, -def.half_width, 0.0f}, {def.half_width, def.half_width, height}};
}

bool OverlapsBox(const Vec3& position, const Aabb& hull, const Aabb& box) {
    const Vec3 lo{position.x + hull.min.x, position.y + hull.min.y, position.z + hull.min.z};
    const Vec3 hi{position.x + hull.max.x, position.y + hull.max.y, position.z + hull.max.z};
    return lo.x < box.max.x - kEpsilon && hi.x > box.min.x + kEpsilon &&
           lo.y < box.max.y - kEpsilon && hi.y > box.min.y + kEpsilon &&
           lo.z < box.max.z - kEpsilon && hi.z > box.min.z + kEpsilon;
}

bool BodyOverlaps(const MapDef& map, const Vec3& position, const MovementDef& def, float height) {
    const Aabb hull = BodyHull(def, height);
    return TraceOverlap(map, hull, position);
}

// 重叠时沿最小穿透轴推出；最多两轮，避免出生重叠/被挤进箱子后卡死。
void Depenetrate(PlayerState& player, const MovementDef& def, const MapDef* map,
                 std::span<const Aabb> blockers) {
    const float height = player.ducking ? def.duck_height : def.stand_height;
    const Aabb hull = BodyHull(def, height);
    for (int iteration = 0; iteration < 2; ++iteration) {
        bool moved = false;
        const int wall_count = (map != nullptr) ? map->wall_count : 0;
        const int total = wall_count + static_cast<int>(blockers.size());
        for (int index = 0; index < total; ++index) {
            Aabb box{};
            if (index < wall_count) {
                box = {map->walls[index].min, map->walls[index].max};
            } else {
                box = blockers[static_cast<size_t>(index - wall_count)];
            }
            if (!OverlapsBox(player.position, hull, box)) {
                continue;
            }
            const Vec3 lo{player.position.x + hull.min.x, player.position.y + hull.min.y,
                          player.position.z + hull.min.z};
            const Vec3 hi{player.position.x + hull.max.x, player.position.y + hull.max.y,
                          player.position.z + hull.max.z};
            const float pushes[6] = {
                box.max.x - lo.x, hi.x - box.min.x, box.max.y - lo.y,
                hi.y - box.min.y, box.max.z - lo.z, hi.z - box.min.z,
            };
            int best_axis = 0;
            float best_push = 0.0f;
            for (int i = 0; i < 6; ++i) {
                if (pushes[i] <= 0.0f) {
                    continue;
                }
                if (best_push == 0.0f || pushes[i] < best_push) {
                    best_push = pushes[i];
                    best_axis = i;
                }
            }
            if (best_push <= 0.0f) {
                continue;
            }
            if (best_axis == 0) {
                player.position.x += best_push;
            } else if (best_axis == 1) {
                player.position.x -= best_push;
            } else if (best_axis == 2) {
                player.position.y += best_push;
            } else if (best_axis == 3) {
                player.position.y -= best_push;
            } else if (best_axis == 4) {
                player.position.z += best_push;
            } else {
                player.position.z -= best_push;
            }
            moved = true;
        }
        if (!moved) {
            break;
        }
    }
}

// 可 step-up 时返回最高可迈上的台阶顶面，否则返回当前位置 z。
float StepUpHeight(const MapDef& map, const PlayerState& player, const MovementDef& def,
                   const Vec3& next, float height) {
    const Vec3 lo = {next.x - def.half_width - 0.01f, next.y - def.half_width - 0.01f,
                     next.z + 0.01f};
    const Vec3 hi = {next.x + def.half_width + 0.01f, next.y + def.half_width + 0.01f,
                     next.z + height};
    float best = player.position.z;
    for (int w = 0; w < map.wall_count; ++w) {
        const Wall& wall = map.walls[w];
        if (lo.x >= wall.max.x || hi.x <= wall.min.x || lo.y >= wall.max.y || hi.y <= wall.min.y ||
            lo.z >= wall.max.z || hi.z <= wall.min.z) {
            continue;
        }
        const float rise = wall.max.z - player.position.z;
        if (rise > 0.01f && rise <= kStepHeight && wall.max.z > best) {
            best = wall.max.z;
        }
    }
    return best;
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

void MoveAxis(PlayerState& player, const MovementDef& def, const MapDef* map,
              std::span<const Aabb> blockers, int axis, float delta) {
    if (delta == 0.0f) {
        return;
    }
    if (map == nullptr && blockers.empty()) {
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
    const Aabb hull = BodyHull(def, height);
    const int steps =
        std::max(1, static_cast<int>(std::ceil(std::fabs(delta) / kMaxSubstepDistance)));
    const float step = delta / static_cast<float>(steps);
    for (int i = 0; i < steps; ++i) {
        const Vec3 from = player.position;
        Vec3 next = from;
        if (axis == 0) {
            next.x += step;
        } else if (axis == 1) {
            next.y += step;
        } else {
            next.z += step;
        }
        bool blocked = false;
        if (map != nullptr) {
            const Vec3 axis_delta = (axis == 0)   ? Vec3{step, 0.0f, 0.0f}
                                    : (axis == 1) ? Vec3{0.0f, step, 0.0f}
                                                  : Vec3{0.0f, 0.0f, step};
            const TraceHit hit = TraceHull(*map, hull, from, axis_delta);
            if (hit.hit && hit.fraction < 1.0f) {
                next = hit.point;
                blocked = true;
                if (axis == 0) {
                    player.velocity.x = 0.0f;
                } else if (axis == 1) {
                    player.velocity.y = 0.0f;
                } else {
                    player.velocity.z = 0.0f;
                    if (hit.normal.z > 0.5f) {
                        player.on_ground = true;
                    }
                }
            }
        }
        if (axis != 2) {
            for (const Aabb& blocker : blockers) {
                if (!OverlapsBox(next, hull, blocker)) {
                    continue;
                }
                blocked = true;
                if (axis == 0) {
                    next.x = step > 0.0f ? blocker.min.x - def.half_width
                                         : blocker.max.x + def.half_width;
                    player.velocity.x = 0.0f;
                } else {
                    next.y = step > 0.0f ? blocker.min.y - def.half_width
                                         : blocker.max.y + def.half_width;
                    player.velocity.y = 0.0f;
                }
            }
        }
        if (blocked && axis != 2 && player.on_ground && map != nullptr) {
            // 台阶：抬升到 ≤32u 的支撑面后重试同一水平位移。
            const float step_height = StepUpHeight(*map, player, def, next, height);
            if (step_height > player.position.z + 0.01f) {
                Vec3 raised = next;
                raised.z = step_height;
                if (!BodyOverlaps(*map, raised, def, height)) {
                    player.position = raised;
                    continue;
                }
            }
        }
        player.position = next;
        if (blocked) {
            return;
        }
    }
}

// 落地：取脚下（水平投影内）最高支撑面；无支撑则离地。
void ResolveGround(PlayerState& player, const MovementDef& def, const MapDef* map) {
    const float floor_z = (map != nullptr) ? map->floor_z : 0.0f;
    float support = floor_z;
    if (map != nullptr) {
        const Aabb hull = BodyHull(def, 0.0f);
        for (int w = 0; w < map->wall_count; ++w) {
            const Wall& wall = map->walls[w];
            if (wall.max.z < floor_z || wall.max.z > player.position.z + kSupportEpsilon) {
                continue;
            }
            if (wall.min.x < player.position.x + hull.max.x &&
                wall.max.x > player.position.x + hull.min.x &&
                wall.min.y < player.position.y + hull.max.y &&
                wall.max.y > player.position.y + hull.min.y) {
                support = std::max(support, wall.max.z);
            }
        }
    }
    if (player.position.z <= support + kSupportEpsilon) {
        player.position.z = support;
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
    return BodyOverlaps(*map, player.position, def, def.stand_height);
}

} // namespace

void StepMovement(PlayerState& player, const MovementDef& def, const MapDef* map,
                  std::span<const Aabb> blockers) {
    if (!player.alive) {
        return;
    }
    // 先判“能否站起”，再推出：低矮天花板下不能靠竖直推出把玩家压到地下。
    if (!player.ducking && StandsIntoWorld(player, def, map)) {
        player.ducking = true;
    }
    Depenetrate(player, def, map, blockers);

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

    MoveAxis(player, def, map, blockers, 0, player.velocity.x * def.tick_dt);
    MoveAxis(player, def, map, blockers, 1, player.velocity.y * def.tick_dt);
    MoveAxis(player, def, map, blockers, 2, player.velocity.z * def.tick_dt);
    ResolveGround(player, def, map);
    Depenetrate(player, def, map, blockers);
}

} // namespace recoil
