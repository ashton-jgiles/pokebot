#include "cards/deck_validator.hpp"

#include <map>

namespace pokebot {

    namespace {

        constexpr int kDeckSize = 60;
        constexpr int kMaxCopies = 4;

        bool is_basic_energy(const CardDef& card) {
            return card.kind == CardKind::Energy && card.energy_kind == EnergyKind::Basic;
        }

        bool is_basic_pokemon(const CardDef& card) {
            return card.kind == CardKind::Pokemon && card.stage == Stage::Basic;
        }

        std::string with_count(const std::string& name, int count) {
            if (count > 1) {
                return name + " x" + std::to_string(count);
            }
            return name;
        }

        std::string join(const std::vector<std::string>& items) {
            std::string out;
            for (const auto& item : items) {
                if (!out.empty()) {
                    out += ", ";
                }
                out += item;
            }
            return out;
        }
    } // namespace

    DeckValidation validate_deck(const Decklist& deck, const CardDB& db) {
        DeckValidation result;

        const int total = deck.total_cards();
        if (total != kDeckSize) {
            result.problems.push_back("deck has " + std::to_string(total) + " cards; it must have exactly " + std::to_string(kDeckSize));
        }

        std::map<std::string, int> copies_by_name;
        std::vector<std::string> ace_specs;
        std::vector<std::string> radiants;
        int ace_spec_count = 0;
        int radiant_count = 0;
        int unknown_cards = 0;
        bool has_basic_pokemon = false;

        for (const auto& entry : deck.entries) {
            const std::string id = card_id(entry);
            const CardDef* card = db.find(id);
            if (card == nullptr) {
                ++unknown_cards;
                result.problems.push_back("unknown card " + id + " (\"" + entry.name + "\"); run pokebot-import on this deck");
                continue;
            }

            if (!is_basic_energy(*card)) {
                copies_by_name[card->name] += entry.count;
            }
            if (is_basic_pokemon(*card)) {
                has_basic_pokemon = true;
            }
            if (card->rules.ace_spec) {
                ace_spec_count += entry.count;
                ace_specs.push_back(with_count(card->name, entry.count));
            }
            if (card->rules.radiant) {
                radiant_count += entry.count;
                radiants.push_back(with_count(card->name, entry.count));
            }
        }

        for (const auto& [name, copies] : copies_by_name) {
            if (copies > kMaxCopies) {
                result.problems.push_back(std::to_string(copies) + " cards named \"" + name + "\"; at most " + std::to_string(kMaxCopies) + " are allowed");
            }
        }

        if (!has_basic_pokemon && unknown_cards == 0) {
            result.problems.push_back("no Basic Pokemon; a deck needs at least 1");
        }

        if (ace_spec_count > 1) {
            result.problems.push_back(std::to_string(ace_spec_count) + " ACE SPEC cards (" + join(ace_specs) + "); at most 1 is allowed");
        }
        if (radiant_count > 1) {
            result.problems.push_back(std::to_string(radiant_count) + " Radiant Pokemon (" + join(radiants) + "); at most 1 is allowed");
        }

        return result;
    }

} // namespace pokebot
