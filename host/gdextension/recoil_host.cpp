#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>

#include <vector>

#include "gdextension_interface.h"
#include "recoil/ai/nav.h"
#include "recoil/ai/tactics.h"
#include "recoil/content/content.h"
#include "recoil/sim/command.h"
#include "recoil/sim/round.h"
#include "recoil/sim/step.h"
#include "recoil/world/map.h"
#include "recoil_default_map.h"

using namespace godot;

class RecoilSim : public RefCounted {
    GDCLASS(RecoilSim, RefCounted)

    recoil::WorldState state_{};
    recoil::Content content_{};
    recoil::MapDef map_{};
    recoil::EventList last_events_{};
    int total_damage_ = 0;
    int total_fire_ = 0;
    int total_dry_ = 0;
    int human_player_ = 0;
    recoil::SiteLayout sites_{};
    std::vector<recoil::Command> pending_{};

protected:
    static void _bind_methods() {
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
        BIND(get_last_dryfire_events);
        BIND(get_total_damage_events);
        BIND(get_total_fire_events);
        BIND(get_total_dryfire_events);
        BIND(set_map_boxes, "boxes");
        BIND(set_sites, "ax", "ay", "bx", "by");
        BIND(begin_match);
        BIND(tick_auto);
        BIND(get_wall_count);
        BIND(get_round_number);
        BIND(get_alive_count, "team");
        BIND(get_has_bomb, "player");
        BIND(nav_path_points, "fx", "fy", "fz", "tx", "ty", "tz");
#undef BIND
    }

public:
    void reset() {
        state_ = recoil::WorldState{};
        state_.player_count = 2;
        content_ = recoil::DefaultContent();
        map_ = recoil_host::MakeDemoArena();
        pending_.clear();
        last_events_ = recoil::EventList{};
        total_damage_ = 0;
        total_fire_ = 0;
        total_dry_ = 0;

        recoil::PlayerState& local = state_.players[0];
        local.alive = true;
        local.on_ground = true;
        local.health = 100;
        local.team = static_cast<uint8_t>(recoil::Team::T);
        local.money = content_.round.start_money;
        local.position = {0.0f, 0.0f, 0.0f};
        local.weapon.mag = content_.rifle.magazine;
        local.weapon.reserve = content_.rifle.reserve;

        recoil::PlayerState& dummy = state_.players[1];
        dummy.alive = true;
        dummy.on_ground = true;
        dummy.health = 100;
        dummy.armor = 100;
        dummy.team = static_cast<uint8_t>(recoil::Team::CT);
        dummy.money = content_.round.start_money;
        dummy.position = {400.0f, 0.0f, 0.0f};
        dummy.weapon.mag = content_.rifle.magazine;
        dummy.weapon.reserve = content_.rifle.reserve;

        // 与 core ResetRound 的分配一致：C4 给 T 方、拆弹器给 CT 方；
        // 否则首回合任何玩家都没有 C4，E 下包永远不生效（C4 HUD 也就无从显示）。
        local.has_bomb = true;
        dummy.has_defuser = true;

        state_.round.phase = recoil::RoundPhase::Freeze;
        state_.round.phase_ticks = content_.round.freeze_ticks;
    }

    void set_move(int player, float yaw, float pitch, float forward, float side, bool jump,
                  bool duck) {
        recoil::Command command;
        command.kind = recoil::CommandKind::Move;
        command.player = static_cast<uint8_t>(player);
        command.yaw_degrees = yaw;
        command.pitch_degrees = pitch;
        command.move_forward = forward;
        command.move_side = side;
        command.jump = jump;
        command.duck = duck;
        pending_.push_back(command);
    }

    void fire(int player, float yaw, float pitch) {
        recoil::Command command;
        command.kind = recoil::CommandKind::Fire;
        command.player = static_cast<uint8_t>(player);
        command.yaw_degrees = yaw;
        command.pitch_degrees = pitch;
        pending_.push_back(command);
    }

    void reload(int player) {
        recoil::Command command;
        command.kind = recoil::CommandKind::Reload;
        command.player = static_cast<uint8_t>(player);
        pending_.push_back(command);
    }

    void buy_armor(int player) {
        recoil::Command command;
        command.kind = recoil::CommandKind::Buy;
        command.player = static_cast<uint8_t>(player);
        command.item = recoil::BuyItem::Armor;
        pending_.push_back(command);
    }

    void throw_grenade(int player, int kind, float yaw, float pitch) {
        recoil::Command command;
        command.kind = recoil::CommandKind::Throw;
        command.player = static_cast<uint8_t>(player);
        command.grenade = static_cast<recoil::GrenadeKind>(kind);
        command.yaw_degrees = yaw;
        command.pitch_degrees = pitch;
        pending_.push_back(command);
    }

    void plant_bomb(int player) {
        recoil::Command command;
        command.kind = recoil::CommandKind::Plant;
        command.player = static_cast<uint8_t>(player);
        pending_.push_back(command);
    }

    void defuse_bomb(int player) {
        recoil::Command command;
        command.kind = recoil::CommandKind::Defuse;
        command.player = static_cast<uint8_t>(player);
        pending_.push_back(command);
    }

    int get_bomb_planted() const {
        return state_.bomb.planted ? 1 : 0;
    }
    int get_bomb_timer() const {
        return state_.bomb.timer_ticks;
    }

    void tick() {
        recoil::StepResult result = recoil::Step(state_, content_, pending_, &map_);
        state_ = result.state;
        last_events_ = result.events;
        total_damage_ += _count(recoil::EventKind::DamageApplied);
        total_fire_ += _count(recoil::EventKind::WeaponFired);
        total_dry_ += _count(recoil::EventKind::WeaponDryFire);
        pending_.clear();
    }

    // de_ash：宿主读派生盒子（Godot 坐标 → sim AABB），每盒 6 个 float。
    void set_map_boxes(const PackedFloat32Array& boxes) {
        map_ = recoil::MapDef{};
        const int count = boxes.size() / 6;
        for (int i = 0; i < count && map_.wall_count < recoil::kMaxWalls; ++i) {
            const int base = i * 6;
            recoil::Wall& wall = map_.walls[map_.wall_count++];
            wall.min = {static_cast<float>(boxes[base + 0]), static_cast<float>(boxes[base + 1]),
                        static_cast<float>(boxes[base + 2])};
            wall.max = {static_cast<float>(boxes[base + 3]), static_cast<float>(boxes[base + 4]),
                        static_cast<float>(boxes[base + 5])};
        }
    }

    void set_sites(float ax, float ay, float bx, float by) {
        sites_.site_a = {ax, ay, 0.0f};
        sites_.site_b = {bx, by, 0.0f};
    }

    void begin_match() {
        state_ = recoil::WorldState{};
        content_ = recoil::DefaultContent();
        recoil::EventList events{};
        recoil::BeginMatch(state_, content_, events);
        pending_.clear();
        last_events_ = events;
        total_damage_ = 0;
        total_fire_ = 0;
        total_dry_ = 0;
    }

    // 人机由内核战术层驱动：玩家 1..N-1 生成 Command，再统一 Step。
    void tick_auto() {
        for (int i = 0; i < static_cast<int>(state_.player_count); ++i) {
            if (i == human_player_) {
                continue;
            }
            pending_.push_back(recoil::TacticalBotDecide(state_, content_, static_cast<uint8_t>(i),
                                                         sites_, &map_));
        }
        tick();
    }

    int get_wall_count() const {
        return map_.wall_count;
    }
    int get_round_number() const {
        return state_.round.number;
    }
    int get_alive_count(int team) const {
        int count = 0;
        for (uint8_t i = 0; i < state_.player_count; ++i) {
            if (state_.players[i].alive && static_cast<int>(state_.players[i].team) == team) {
                ++count;
            }
        }
        return count;
    }
    int get_has_bomb(int player) const {
        return state_.players[player].has_bomb ? 1 : 0;
    }
    int nav_path_points(float fx, float fy, float fz, float tx, float ty, float tz) const {
        return static_cast<int>(recoil::NavPath(map_, {fx, fy, fz}, {tx, ty, tz}).size());
    }

    float get_x(int player) const {
        return state_.players[player].position.x;
    }
    float get_y(int player) const {
        return state_.players[player].position.y;
    }
    float get_z(int player) const {
        return state_.players[player].position.z;
    }
    int get_health(int player) const {
        return state_.players[player].health;
    }
    int get_armor(int player) const {
        return state_.players[player].armor;
    }
    int get_helmet(int player) const {
        return state_.players[player].helmet ? 1 : 0;
    }
    int get_team(int player) const {
        return static_cast<int>(state_.players[player].team);
    }
    int get_player_count() const {
        return state_.player_count;
    }
    int get_mag(int player) const {
        return state_.players[player].weapon.mag;
    }
    int get_reserve(int player) const {
        return state_.players[player].weapon.reserve;
    }
    bool is_alive(int player) const {
        return state_.players[player].alive;
    }
    int get_kills(int player) const {
        return state_.players[player].kills;
    }
    int get_money(int player) const {
        return state_.players[player].money;
    }
    int get_smoke_active() const {
        return state_.smoke.active ? 1 : 0;
    }
    float get_smoke_x() const {
        return state_.smoke.position.x;
    }
    float get_smoke_y() const {
        return state_.smoke.position.y;
    }
    int get_blind_ticks(int player) const {
        return state_.players[player].blind_ticks;
    }
    int get_round_phase() const {
        return static_cast<int>(state_.round.phase);
    }
    int get_round_ticks() const {
        return state_.round.phase_ticks;
    }
    int get_score_t() const {
        return state_.round.score_t;
    }
    int get_score_ct() const {
        return state_.round.score_ct;
    }
    int get_tick() const {
        return static_cast<int>(state_.tick);
    }

    int _count(recoil::EventKind kind) const {
        int total = 0;
        for (int i = 0; i < last_events_.count; ++i) {
            if (last_events_.items[i].kind == kind) {
                ++total;
            }
        }
        return total;
    }
    int get_last_damage_events() const {
        return _count(recoil::EventKind::DamageApplied);
    }
    int get_last_fire_events() const {
        return _count(recoil::EventKind::WeaponFired);
    }
    int get_last_dryfire_events() const {
        return _count(recoil::EventKind::WeaponDryFire);
    }
    int get_total_damage_events() const {
        return total_damage_;
    }
    int get_total_fire_events() const {
        return total_fire_;
    }
    int get_total_dryfire_events() const {
        return total_dry_;
    }
};

namespace {

void initialize_recoil_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    ClassDB::register_class<RecoilSim>();
}

void uninitialize_recoil_module(ModuleInitializationLevel p_level) {
    (void)p_level;
}

} // namespace

extern "C" {
GDE_EXPORT GDExtensionBool recoil_library_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization* r_initialization) {
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(initialize_recoil_module);
    init_obj.register_terminator(uninitialize_recoil_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
}
