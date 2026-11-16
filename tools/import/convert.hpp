#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "cards/card.hpp"

namespace pokebot::import {

    struct Conversion {
        CardDef card;
        std::vector<std::string> notes;  // things a person should check, e.g. a possible Tera rule
    };

    // Turns a TCGdex card object into our CardDef. our_id is the decklist-style id ("TWM-130").
    // Throws CardDataError on any value it doesn't understand, so a new mechanic is reported
    // instead of silently converted wrong.
    Conversion convert_card(const nlohmann::json& tcgdex, const std::string& our_id);

} // namespace pokebot::import
