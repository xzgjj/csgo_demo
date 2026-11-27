#pragma once

#include <cstdint>

namespace recoil {

// DEP-03：购买状态的经济阈值（SPEC-07 §7.4，钱为唯一触发条件）。
// 与 AI 行为策略（AiBuyPolicy）解耦：这里只做“推荐状态”，不决定具体下单清单。
enum class BuyState : uint8_t {
    Full,  // ≥ 4000：步枪+甲+雷
    Half,  // 2500–3999：步枪/少雷，视队伍经济
    Force, // 1500–2499：手枪+甲或冲锋枪
    Eco,   // < 1500：存钱，下回合全买
};

inline BuyState RecommendBuyState(uint16_t money) {
    if (money >= 4000) {
        return BuyState::Full;
    }
    if (money >= 2500) {
        return BuyState::Half;
    }
    if (money >= 1500) {
        return BuyState::Force;
    }
    return BuyState::Eco;
}

inline const char* BuyStateName(BuyState state) {
    switch (state) {
    case BuyState::Full:
        return "full";
    case BuyState::Half:
        return "half";
    case BuyState::Force:
        return "force";
    default:
        return "eco";
    }
}

} // namespace recoil
