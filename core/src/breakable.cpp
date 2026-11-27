#include "recoil/world/breakable.h"

namespace recoil {

BreakableReport DamageWall(MapDef& map, int wall_index, int damage) {
    BreakableReport report;
    report.wall_index = wall_index;
    if (wall_index < 0 || wall_index >= map.wall_count) {
        report.result = BreakableResult::InvalidIndex;
        return report;
    }
    if (damage <= 0) {
        report.result = BreakableResult::InvalidDamage;
        return report;
    }
    const uint16_t hp = map.wall_hp[wall_index];
    if (hp == 0) {
        report.result = BreakableResult::Indestructible;
        return report;
    }
    if (static_cast<uint32_t>(damage) >= hp) {
        RemoveWall(map, wall_index);
        report.result = BreakableResult::Destroyed;
        return report;
    }
    map.wall_hp[wall_index] = static_cast<uint16_t>(hp - static_cast<uint16_t>(damage));
    report.result = BreakableResult::Damaged;
    report.remaining_hp = map.wall_hp[wall_index];
    return report;
}

bool IsBreakable(const MapDef& map, int wall_index) {
    return wall_index >= 0 && wall_index < map.wall_count && map.wall_hp[wall_index] > 0;
}

} // namespace recoil
