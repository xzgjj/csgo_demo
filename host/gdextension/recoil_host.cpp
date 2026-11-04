#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include <vector>

#include "gdextension_interface.h"
#include "recoil/content/content.h"
#include "recoil/sim/command.h"
#include "recoil/sim/step.h"
#include "recoil/world/map.h"

using namespace godot;

namespace {

recoil::MapDef MakeArena() {
    recoil::MapDef map;
    const float half = 800.0f;
    const float thickness = 32.0f;
    const float height = 200.0f;
    map.wall_count = 4;
    map.walls[0] = {{half, -half, 0.0f}, {half + thickness, half, height}};
    map.walls[1] = {{-half - thickness, -half, 0.0f}, {-half, half, height}};
    map.walls[2] = {{-half, half, 0.0f}, {half, half + thickness, height}};
    map.walls[3] = {{-half, -half - thickness, 0.0f}, {half, -half, height}};
    return map;
}

} // namespace

class RecoilSim : public RefCounted {
    GDCLASS(RecoilSim, RefCounted)

    recoil::WorldState state_{};
    recoil::Content content_{};
    recoil::MapDef map_{};
    recoil::EventList last_events_{};
    int total_damage_ = 0;
    int total_fire_ = 0;
    int total_dry_ = 0;
    std::vector<recoil::Command> pending_{};

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("reset"), &RecoilSim::reset);
        ClassDB::bind_method(
            D_METHOD("set_move", "player", "yaw", "pitch", "forward", "side", "jump", "duck"),
            &RecoilSim::set_move);
        ClassDB::bind_method(D_METHOD("fire", "player", "yaw", "pitch"), &RecoilSim::fire);
        ClassDB::bind_method(D_METHOD("reload", "player"), &RecoilSim::reload);
        ClassDB::bind_method(D_METHOD("buy_armor", "player"), &RecoilSim::buy_armor);
        ClassDB::bind_method(D_METHOD("plant_bomb", "player"), &RecoilSim::plant_bomb);
        ClassDB::bind_method(D_METHOD("defuse_bomb", "player"), &RecoilSim::defuse_bomb);
        ClassDB::bind_method(D_METHOD("get_bomb_planted"), &RecoilSim::get_bomb_planted);
        ClassDB::bind_method(D_METHOD("get_bomb_timer"), &RecoilSim::get_bomb_timer);
        ClassDB::bind_method(D_METHOD("get_helmet", "player"), &RecoilSim::get_helmet);
        ClassDB::bind_method(D_METHOD("get_team", "player"), &RecoilSim::get_team);
        ClassDB::bind_method(D_METHOD("get_player_count"), &RecoilSim::get_player_count);
        ClassDB::bind_method(D_METHOD("get_round_phase"), &RecoilSim::get_round_phase);
        ClassDB::bind_method(D_METHOD("get_round_ticks"), &RecoilSim::get_round_ticks);
        ClassDB::bind_method(D_METHOD("get_score_t"), &RecoilSim::get_score_t);
        ClassDB::bind_method(D_METHOD("get_score_ct"), &RecoilSim::get_score_ct);
        ClassDB::bind_method(D_METHOD("get_money", "player"), &RecoilSim::get_money);
        ClassDB::bind_method(D_METHOD("throw_grenade", "player", "kind", "yaw", "pitch"),
                             &RecoilSim::throw_grenade);
        ClassDB::bind_method(D_METHOD("get_smoke_active"), &RecoilSim::get_smoke_active);
        ClassDB::bind_method(D_METHOD("get_smoke_x"), &RecoilSim::get_smoke_x);
        ClassDB::bind_method(D_METHOD("get_smoke_y"), &RecoilSim::get_smoke_y);
        ClassDB::bind_method(D_METHOD("get_blind_ticks", "player"), &RecoilSim::get_blind_ticks);
        ClassDB::bind_method(D_METHOD("tick"), &RecoilSim::tick);
        ClassDB::bind_method(D_METHOD("get_x", "player"), &RecoilSim::get_x);
        ClassDB::bind_method(D_METHOD("get_y", "player"), &RecoilSim::get_y);
        ClassDB::bind_method(D_METHOD("get_z", "player"), &RecoilSim::get_z);
        ClassDB::bind_method(D_METHOD("get_health", "player"), &RecoilSim::get_health);
        ClassDB::bind_method(D_METHOD("get_armor", "player"), &RecoilSim::get_armor);
        ClassDB::bind_method(D_METHOD("get_mag", "player"), &RecoilSim::get_mag);
        ClassDB::bind_method(D_METHOD("get_reserve", "player"), &RecoilSim::get_reserve);
        ClassDB::bind_method(D_METHOD("is_alive", "player"), &RecoilSim::is_alive);
        ClassDB::bind_method(D_METHOD("get_kills", "player"), &RecoilSim::get_kills);
        ClassDB::bind_method(D_METHOD("get_tick"), &RecoilSim::get_tick);
        ClassDB::bind_method(D_METHOD("get_last_damage_events"),
                             &RecoilSim::get_last_damage_events);
        ClassDB::bind_method(D_METHOD("get_last_fire_events"), &RecoilSim::get_last_fire_events);
        ClassDB::bind_method(D_METHOD("get_last_dryfire_events"),
                             &RecoilSim::get_last_dryfire_events);
        ClassDB::bind_method(D_METHOD("get_total_damage_events"),
                             &RecoilSim::get_total_damage_events);
        ClassDB::bind_method(D_METHOD("get_total_fire_events"), &RecoilSim::get_total_fire_events);
        ClassDB::bind_method(D_METHOD("get_total_dryfire_events"),
                             &RecoilSim::get_total_dryfire_events);
    }

public:
    void reset() {
        state_ = recoil::WorldState{};
        state_.player_count = 2;
        content_ = recoil::DefaultContent();
        map_ = MakeArena();
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
