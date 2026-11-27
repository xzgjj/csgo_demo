#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>

#include <vector>

#include "gdextension_interface.h"
#include "recoil/ai/nav.h"
#include "recoil/ai/tactics.h"
#include "recoil/ai/vision.h"
#include "recoil/content/content.h"
#include "recoil/content/weapon_table.h"
#include "recoil/sim/command.h"
#include "recoil/sim/ground.h"
#include "recoil/sim/round.h"
#include "recoil/sim/step.h"
#include "recoil/world/map.h"
#include "recoil_default_map.h"
#include "recoil_host_map.h"
#include "recoil_match_ops.h"

using namespace godot;

class RecoilSim : public RefCounted {
    GDCLASS(RecoilSim, RefCounted)

    recoil::WorldState state_{};
    recoil::Content content_{};
    recoil::MapDef map_{};
    recoil::EventList last_events_{};
    int total_damage_ = 0;
    int total_fire_ = 0;
    // 按射手的累计命中次数（命中归因：玩家自己的击中 FX 只认这个计数）。
    int damage_by_[recoil::kMaxPlayers] = {};
    int total_dry_ = 0;
    int human_player_ = 0;
    recoil::SiteLayout sites_{};
    std::vector<recoil::Command> pending_{};
    std::vector<recoil::Command> bot_commands_{};
    int bot_refresh_ = 0;

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
#undef BIND
    }

public:
    void reset() {
        state_ = recoil::WorldState{};
        state_.player_count = 2;
        content_ = recoil::DefaultContent();
        map_ = recoil_host::MakeDemoArena();
        pending_.clear();
        bot_commands_.clear();
        bot_refresh_ = 0;
        last_events_ = recoil::EventList{};
        total_damage_ = 0;
        total_fire_ = 0;
        for (int& count : damage_by_) {
            count = 0;
        }
        recoil_host::ClearKills();
        total_dry_ = 0;

        recoil::PlayerState& local = state_.players[0];
        local.alive = true;
        local.on_ground = true;
        local.health = 100;
        local.team = static_cast<uint8_t>(recoil::Team::T);
        local.money = content_.round.start_money;
        local.position = {0.0f, 0.0f, 0.0f};
        recoil::GiveDefaultLoadout(local, static_cast<uint8_t>(recoil::Team::T));

        recoil::PlayerState& dummy = state_.players[1];
        dummy.alive = true;
        dummy.on_ground = true;
        dummy.health = 100;
        dummy.armor = 100;
        dummy.team = static_cast<uint8_t>(recoil::Team::CT);
        dummy.money = content_.round.start_money;
        dummy.position = {400.0f, 0.0f, 0.0f};
        recoil::GiveDefaultLoadout(dummy, static_cast<uint8_t>(recoil::Team::CT));

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

    // 购买武器：入队一条 BuyWeapon 命令；返回值=命令能否生效（冻结期/枪存在/钱够），
    // 扣钱与换装在下一个 tick 由内核应用（失败会发 BuyFailed 事件）。
    bool buy_weapon(int player, int weapon_id) {
        if (player < 0 || player >= static_cast<int>(state_.player_count)) {
            return false;
        }
        const recoil::WeaponRow* row =
            recoil::FindWeaponRow(static_cast<recoil::WeaponId>(weapon_id));
        if (row == nullptr || !recoil::CanBuy(state_, content_)) {
            return false;
        }
        if (state_.players[static_cast<size_t>(player)].money < row->price) {
            return false;
        }
        recoil::Command command;
        command.kind = recoil::CommandKind::BuyWeapon;
        command.player = static_cast<uint8_t>(player);
        command.weapon_id = static_cast<recoil::WeaponId>(weapon_id);
        pending_.push_back(command);
        return true;
    }

    // 购买头盔（$1000，冻结期；已有内核 ApplyBuy 语义）。
    void buy_helmet(int player) {
        recoil::Command command;
        command.kind = recoil::CommandKind::Buy;
        command.player = static_cast<uint8_t>(player);
        command.item = recoil::BuyItem::Helmet;
        pending_.push_back(command);
    }

    // 拾取附近地面掉落武器（半径 64u，取最近一件）。
    void pickup_weapon(int player) {
        recoil::Command command;
        command.kind = recoil::CommandKind::Pickup;
        command.player = static_cast<uint8_t>(player);
        pending_.push_back(command);
    }

    // 切换武器槽（0=主武器，1=副武器）；非法槽位由内核发 CommandRejected(InvalidSlot)。
    void switch_weapon(int player, int slot) {
        recoil::Command command;
        command.kind = recoil::CommandKind::SwitchWeapon;
        command.player = static_cast<uint8_t>(player);
        command.slot = static_cast<uint8_t>(slot);
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

    void tick() {
        recoil::StepResult result = recoil::Step(state_, content_, pending_, &map_);
        state_ = result.state;
        last_events_ = result.events;
        total_damage_ += _count(recoil::EventKind::DamageApplied);
        total_fire_ += _count(recoil::EventKind::WeaponFired);
        for (int i = 0; i < result.events.count; ++i) {
            const recoil::Event& event = result.events.items[i];
            if (event.kind == recoil::EventKind::DamageApplied &&
                event.source < recoil::kMaxPlayers) {
                ++damage_by_[event.source];
            }
        }
        recoil_host::RecordKills(result.events);
        pending_.clear();
    }

    // de_ash：宿主读派生盒子（Godot 坐标 → sim AABB），每盒 8 个 float。
    int set_map_boxes(const PackedFloat32Array& boxes) {
        return recoil_host::LoadMapBoxes(map_, boxes);
    }

    void set_sites(float ax, float ay, float bx, float by) {
        sites_.site_a = {ax, ay, 0.0f};
        sites_.site_b = {bx, by, 0.0f};
    }

    void begin_match() {
        state_ = recoil::WorldState{};
        content_ = recoil::DefaultContent();
        recoil::BeginMatch(state_, content_, last_events_);
        // 演示局出生金钱（便于试玩购买 M4/AWP；不改变竞技参考 start_money）。
        const uint16_t demo_money = content_.round.demo_start_money;
        if (demo_money > 0) {
            for (uint8_t i = 0; i < state_.player_count; ++i) {
                state_.players[i].money = demo_money;
            }
        }
        pending_.clear();
        bot_commands_.clear();
        bot_refresh_ = 0;
        total_damage_ = total_fire_ = total_dry_ = 0;
        for (int& count : damage_by_) {
            count = 0;
        }
        recoil_host::ClearKills();
    }

    // 人机由内核战术层驱动：每 4 tick 重算一次决策（导航开销分摊），中间复用命令。
    void tick_auto() {
        if (bot_refresh_ % 4 == 0) { // 人机决策每 4 tick 刷新，导航开销分摊
            bot_commands_.clear();
            for (uint8_t i = 0; i < state_.player_count; ++i) {
                if (i != human_player_) {
                    bot_commands_.push_back(
                        recoil::TacticalBotDecide(state_, content_, i, sites_, &map_));
                }
            }
        }
        ++bot_refresh_;
        pending_.insert(pending_.end(), bot_commands_.begin(), bot_commands_.end());
        tick();
    }

    void set_player_position(int player, float x, float y, float z) {
        recoil_host::SetPlayerPosition(state_, player, x, y, z);
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

    // 只读 getter 组（含 is_visible）拆到独立头文件，保持本文件 <=400 行。
#include "recoil_host_getters.h"
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
