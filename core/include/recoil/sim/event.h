#pragma once

#include <cstdint>

namespace recoil {

enum class EventKind : uint8_t {
    WeaponFired,
    WeaponDryFire,
    ShotMissed,
    DamageApplied,
    PlayerDied,
    ReloadStarted,
    ReloadFinished,
    CommandRejected,
    RoundLive,
    RoundEnded,
    RoundReset,
    BuySucceeded,
    BuyFailed,
    GrenadeThrown,
    GrenadeExploded,
    FlashBlinded,
    SmokeStarted,
    SmokeEnded,
    BombPlanted,
    BombDefused,
    BombExploded,
    WeaponSwitched,
    WeaponDropped,
    WeaponPickedUp,
    WallDamaged,
    // v6 追加在末尾，保持既有数值稳定（场景/脚本按名字调用）。
    AssistAwarded,  // source=助攻者，target=被击杀者，f0=助攻伤害
    PlayerHitTaken, // target=受害者，source=射手，f0=命中方向相对视角（度）
    MatchStarted,
    RoundHalfStarted,
    SideSwapped,
    OvertimeStarted,
    TimeoutStarted,
    TimeoutEnded,
    SurrenderPassed,
    MatchEnded,
    MatchDrawn,
    BuyWindowClosed,
};

enum class RejectReason : int32_t {
    None = 0,
    InvalidPlayer = 1,
    Dead = 2,
    Cooldown = 3,
    Reloading = 4,
    EmptyMagazine = 5,
    NoReserve = 6,
    MagazineFull = 7,
    NoBombCarrier = 8,
    BombNotPlanted = 9,
    AlreadyPlanted = 10,
    InvalidPhase = 11,
    NotDefuser = 12,
    NoGrenadeSlot = 13,
    MatchEnded = 14,
    InvalidSlot = 15,
    NoGroundWeapon = 16,
    // v6 追加：行为许可与赛制错误码（SPEC-03 §3.13）。
    ActionNotAllowed = 17,
    BuyClosed = 18,
};

// 事件附加标记位（flags 字段）。
enum EventFlags : uint8_t {
    kEventNone = 0,
    kEventHeadshot = 1u << 0,
    kEventThroughWall = 1u << 1,
    kEventNoScope = 1u << 2,
    kEventAssist = 1u << 3,
    kEventArmorBlocked = 1u << 4,
};

// 事件统一载体：既覆盖既有 (kind, source, target, value) 语义，也承载
// SPEC-02/04 的弹道与击杀明细（aux=武器 id，flags=标记，f0..f2=数值）。
struct Event {
    EventKind kind = EventKind::CommandRejected;
    uint8_t source = 0;
    uint8_t target = 0;
    int32_t value = 0;
    uint8_t aux = 0; // 武器 id / 槽位等
    uint8_t flags = 0;
    uint8_t shot_index = 0;
    uint8_t group = 0; // DamageApplied：命中部位（HitGroup 枚举值）
    float f0 = 0.0f;   // WeaponFired:pitch；DamageApplied:距离；PlayerHitTaken:方向角
    float f1 = 0.0f;   // WeaponFired:yaw；DamageApplied:剩余 HP；PlayerDied:距离
    float f2 = 0.0f;   // WeaponFired:inaccuracy；DamageApplied:原始伤害
};

struct EventList {
    // 5v5 一 tick 内可能同时出现多次命中/死亡/助攻，32 会丢事件；提到 64。
    static constexpr int kMaxEvents = 64;
    Event items[kMaxEvents]{};
    int count = 0;
    int dropped = 0;

    void Push(Event event) {
        if (count < kMaxEvents) {
            items[count++] = event;
            return;
        }
        ++dropped;
    }
};

} // namespace recoil
