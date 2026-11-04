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
};

struct Event {
    EventKind kind = EventKind::CommandRejected;
    uint8_t source = 0;
    uint8_t target = 0;
    int32_t value = 0;
};

struct EventList {
    static constexpr int kMaxEvents = 32;
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
