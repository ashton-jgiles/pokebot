#pragma once

#include <cstdint>
#include <string>
#include <utility>
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
        bool first_turn = false;
    };

    std::string normalize_text(const std::string& text);

    std::vector<std::string> split_sentences(const std::string& text);

    Translation translate_attack(const std::string& text, const AttackContext& context);

    enum class TextPart : std::uint8_t {
        Trainer,
        Stadium,
        Tool,
        Ability,
        SpecialEnergy,
    };

    Translation translate_text(const std::string& text, TextPart part, const std::string& name);

    struct CardTranslation {
        nlohmann::json overlay = nlohmann::json::object();
        std::vector<std::pair<std::string, std::string>> unread;
        int parts = 0;
    };

    CardTranslation translate_card(const CardDef& card);

    std::vector<std::string> translation_differences(const CardDef& written, const CardDef& translated, const nlohmann::json& overlay);

} // namespace pokebot::import
