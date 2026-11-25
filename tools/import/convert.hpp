#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "cards/card/card.hpp"

namespace pokebot::import {

    struct Conversion {
        CardDef card;
        std::vector<std::string> notes; // things a person should check
        std::vector<std::string> problems;
        bool tera_unknown = false; // a non-Mega Pokemon ex: TCGdex doesn't say whether it's Tera
    };

    // Turns a TCGdex card object into our CardDef. our_id is the decklist-style id ("TWM-130").
    // Throws CardDataError on any value it doesn't understand, so a new mechanic is reported
    // instead of silently converted wrong.
    Conversion convert_card(const nlohmann::json& tcgdex, const std::string& our_id);

    nlohmann::json apply_corrections(nlohmann::json tcgdex, const nlohmann::json& corrections);

} // namespace pokebot::import
