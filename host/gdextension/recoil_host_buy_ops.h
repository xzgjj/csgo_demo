// RecoilSim 的购买相关方法组（SPEC-06）：在 recoil_host.cpp 类体内 #include。
// 抽出后主文件保持在 CI 单文件 400 行预算内；仍直接访问 RecoilSim 私有成员。
// clang-format off
    void set_buy_enabled(bool enabled) { content_.buy.enabled = enabled; }
    bool buy_enabled() const { return content_.buy.enabled; }

    bool buy_entry(int player, int entry) {
        if (player < 0 || entry < 0 || entry >= static_cast<int>(content_.buy.count)) {
            return false;
        }
        recoil::Command command;
        command.kind = recoil::CommandKind::BuyEntry;
        command.player = static_cast<uint8_t>(player);
        command.entry = static_cast<uint8_t>(entry);
        last_buy_failure_ = 0;
        pending_.push_back(command);
        return true;
    }

    bool refund_entry(int player, int entry) {
        if (player < 0 || entry < 0 || entry >= static_cast<int>(content_.buy.count)) {
            return false;
        }
        recoil::Command command;
        command.kind = recoil::CommandKind::Refund;
        command.player = static_cast<uint8_t>(player);
        command.entry = static_cast<uint8_t>(entry);
        pending_.push_back(command);
        return true;
    }

    bool rebuy(int player) {
        if (player < 0 || player >= static_cast<int>(state_.player_count)) {
            return false;
        }
        recoil::Command command;
        command.kind = recoil::CommandKind::Rebuy;
        command.player = static_cast<uint8_t>(player);
        pending_.push_back(command);
        return true;
    }

    godot::Array buy_catalog_entries() const {
        godot::Array out;
        for (uint8_t i = 0; i < content_.buy.count; ++i) {
            const recoil::BuyEntry& entry = content_.buy.entries[i];
            godot::Dictionary row;
            row["index"] = static_cast<int>(i);
            row["id"] = entry.id;
            row["category"] = static_cast<int>(entry.category);
            row["category_name"] = recoil::kBuyCategoryNames[entry.category];
            row["kind"] = static_cast<int>(entry.kind);
            row["slot"] = static_cast<int>(entry.slot);
            row["price"] = static_cast<int>(entry.price);
            row["limit"] = static_cast<int>(entry.limit);
            row["sides"] = static_cast<int>(entry.sides);
            row["weapon_id"] = static_cast<int>(entry.weapon);
            out.push_back(row);
        }
        return out;
    }

    // SPEC-06 BUY-08：冻结期首 tick 为每个 bot 按经济策略入队一次购买（与玩家同一条 BuyEntry 校验）。
    void _queue_bot_buys() {
        if (!content_.buy.enabled || state_.round.phase != recoil::RoundPhase::Freeze) {
            return;
        }
        if (bot_buy_round_ == static_cast<int>(state_.round.number)) {
            return;
        }
        bot_buy_round_ = static_cast<int>(state_.round.number);
        for (uint8_t i = 0; i < state_.player_count; ++i) {
            if (i == human_player_) {
                continue;
            }
            uint8_t entries[8] = {};
            const int count = recoil::PlanBotBuys(state_, content_, i, entries, 8);
            for (int k = 0; k < count; ++k) {
                recoil::Command command;
                command.kind = recoil::CommandKind::BuyEntry;
                command.player = i;
                command.entry = entries[k];
                pending_.push_back(command);
            }
        }
    }
