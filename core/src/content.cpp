#include "recoil/content/content.h"

namespace recoil {

Content DefaultContent() {
    Content content;
    content.rifle.damage = 36;
    content.rifle.magazine = 30;
    content.rifle.reserve = 90;
    content.rifle.fire_interval_ticks = 6;
    content.rifle.reload_ticks = 156;
    content.rifle.armor_ratio = 0.775f;
    content.rifle.range = 8192.0f;
    return content;
}

} // namespace recoil
