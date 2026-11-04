#include "recoil/content/weapon_table.h"

namespace recoil {

const WeaponRow* FindWeaponRow(WeaponId id) {
    for (const WeaponRow& row : kWeaponTable) {
        if (row.id == id) {
            return &row;
        }
    }
    return nullptr;
}

WeaponId ActiveWeaponId(const PlayerState& player) {
    if (player.active_slot == 0) {
        return static_cast<WeaponId>(player.primary_id);
    }
    return static_cast<WeaponId>(player.secondary_id);
}

WeaponDef ActiveWeaponDef(const PlayerState& player, const Content& content) {
    const WeaponRow* row = FindWeaponRow(ActiveWeaponId(player));
    return row != nullptr ? row->def : content.rifle;
}

uint16_t WeaponPrice(WeaponId id) {
    const WeaponRow* row = FindWeaponRow(id);
    return row != nullptr ? row->price : 0;
}

const char* WeaponName(WeaponId id) {
    const WeaponRow* row = FindWeaponRow(id);
    return row != nullptr ? row->name : "-";
}

} // namespace recoil
