#include "engine/game/game_state.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

#include "cards/validator/deck_validator.hpp"

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
        find_damage_passives();
    }

    void CardTable::find_damage_passives() {
        const auto damage_passive = [](const std::vector<Modifier>& modifiers) {
            return std::any_of(modifiers.begin(), modifiers.end(), [](const Modifier& m) {
                return m.what == Modifies::DamageTaken || m.what == Modifies::PreventDamage || m.what == Modifies::PreventEffects;
            });
        };
        damage_passives_ = std::any_of(defs_.begin(), defs_.end(), [&](const CardDef* def) {
            return def != nullptr && (damage_passive(def->modifiers) || std::any_of(def->abilities.begin(), def->abilities.end(), [&](const Ability& ability) {
                                          return damage_passive(ability.modifiers);
                                      }));
        });
    }

    CardTable CardTable::shuffled(std::uint64_t seed) const {
        CardTable copy = *this;
        Rng rng(seed);
        for (std::size_t first = 0; first < kMaxCards; first += kDeckSize) {
            FixedVector<const CardDef*, kDeckSize> defs;
            for (std::size_t i = 0; i < kDeckSize; ++i) {
                defs.push_back(defs_[first + i]);
            }
            rng.shuffle(defs);
            for (std::size_t i = 0; i < kDeckSize; ++i) {
                copy.defs_[first + i] = defs[i];
            }
        }
        return copy;
    }

} // namespace pokebot
