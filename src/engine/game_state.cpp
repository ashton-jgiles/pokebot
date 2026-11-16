#include "engine/game_state.hpp"

#include <stdexcept>
#include <string>

#include "cards/deck_validator.hpp"

namespace pokebot {

    namespace {
        void fill_deck(std::array<const CardDef*, kMaxCards>& defs, std::size_t first, const CardDB& db, const Decklist& deck, int player) {
            const DeckValidation validation = validate_deck(deck, db);
            if (!validation.legal()) {
                std::string message = "player " + std::to_string(player) + "'s deck is illegal";
                for (const auto& problem : validation.problems) {
                    message += "\n - " + problem;
                }
                throw std::invalid_argument(message);
            }

            std::size_t next = first;
            for (const auto& entry : deck.entries) {
                const CardDef& def = db.at(card_id(entry));
                for (int copy = 0; copy < entry.count; ++copy) {
                    defs[next] = &def;
                    ++next;
                }
            }
        }
    } // namespace

    CardTable::CardTable(const CardDB& db, const Decklist& deck0, const Decklist& deck1) {
        fill_deck(defs_, 0, db, deck0, 0);
        fill_deck(defs_, kDeckSize, db, deck1, 1);
    }

} // namespace pokebot
