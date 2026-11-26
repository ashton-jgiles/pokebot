#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "cards/card/card.hpp"

namespace pokebot::import {

    struct AttackContext {
        std::string name;
        DamageMod damage_mod = DamageMod::None;
    };

    struct Translation {
        bool ok = false;
        nlohmann::json effects = nlohmann::json::array();
        std::string unread;
    };

    std::string normalize_text(const std::string& text);

    std::vector<std::string> split_sentences(const std::string& text);

    Translation translate_attack(const std::string& text, const AttackContext& context);

} // namespace pokebot::import
