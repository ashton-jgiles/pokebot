#pragma once

#include <string>
#include <vector>

#include "cards/card_db.hpp"
#include "core/decklist.hpp"

namespace pokebot {

    struct DeckValidation {
        std::vector<std::string> problems;

        bool legal() const { return problems.empty(); }
    };

    DeckValidation validate_deck(const Decklist& deck, const CardDB& db);

} // namespace pokebot