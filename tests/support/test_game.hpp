#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine/engine.hpp"
#include "engine/internal.hpp"

namespace test {

    // Every card a player owns must be in exactly one place, so this should always be 60.
    inline std::size_t cards_accounted_for(const pokebot::PlayerState& p) {
        std::size_t n = p.deck.size() + p.hand.size() + p.prizes.size() + p.discard.size() + p.lost_zone.size() + p.resolving.size();
        n += p.stadium != pokebot::kNoCard ? 1 : 0;
        if (p.active) {
            n += p.active->stack.size() + p.active->energy.size() + (p.active->tool != pokebot::kNoCard ? 1 : 0);
        }
        for (const auto& pokemon : p.bench) {
            n += pokemon.stack.size() + pokemon.energy.size() + (p.active->tool != pokebot::kNoCard ? 1 : 0);
        }
        return n;
    }

    // Takes the first legal action at every decision until setup is over and turn 1 has begun.
    inline void play_setup(pokebot::GameState& game) {
        for (int step = 0; step < 100 && game.turn == 0; ++step) {
            pokebot::apply(game, pokebot::legal_actions(game).front());
        }
        REQUIRE(game.turn == 1);
    }

    // Removes the first card with this id from the player's deck, or failing that their hand, and returns it.
    // Setup and draws move cards around, so a card the test needs may already be in the hand.
    inline pokebot::CardIndex take_from_deck(pokebot::GameState& game, int player, const std::string& id) {
        auto& p = game.player(player);
        for (auto* zone : {&p.deck, &p.hand}) {
            for (std::size_t i = 0; i < zone->size(); ++i) {
                const auto card = (*zone)[i];
                if (game.def(card).id == id) {
                    zone->erase_at(i);
                    return card;
                }
            }
        }
        FAIL("no " << id << " left in player " << player << "'s deck or hand");
        return pokebot::kNoCard;
    }

    // True if take_from_deck can find a card with this id.
    inline bool has_card(const pokebot::GameState& game, int player, const std::string& id) {
        const auto& p = game.player(player);
        for (const auto* zone : {&p.deck, &p.hand}) {
            for (const auto card : *zone) {
                if (game.def(card).id == id) {
                    return true;
                }
            }
        }
        return false;
    }

    inline pokebot::CardIndex put_in_hand(pokebot::GameState& game, int player, const std::string& id) {
        const auto card = take_from_deck(game, player, id);
        game.player(player).hand.push_back(card);
        return card;
    }

    // Replaces the Active Pokemon with a fresh one from the deck. The old one's cards go back into the deck.
    inline void set_active(pokebot::GameState& game, int player, const std::string& id) {
        auto& p = game.player(player);
        if (p.active) {
            for (const auto card : p.active->stack) {
                p.deck.push_back(card);
            }
            for (const auto card : p.active->energy) {
                p.deck.push_back(card);
            }
        }
        p.active = pokebot::detail::new_pokemon(take_from_deck(game, player, id), 0);
    }

    // Returns every Benched Pokemon's cards to the deck.
    inline void clear_bench(pokebot::GameState& game, int player) {
        auto& p = game.player(player);
        for (const auto& pokemon : p.bench) {
            for (const auto card : pokemon.stack) {
                p.deck.push_back(card);
            }
            for (const auto card : pokemon.energy) {
                p.deck.push_back(card);
            }
        }
        p.bench.clear();
    }

    inline void add_to_bench(pokebot::GameState& game, int player, const std::string& id) {
        game.player(player).bench.push_back(pokebot::detail::new_pokemon(take_from_deck(game, player, id), 0));
    }

    inline void attach_from_deck(pokebot::GameState& game, int player, pokebot::Slot slot, const std::string& id) {
        auto& p = game.player(player);
        pokebot::detail::pokemon_at(p, slot).energy.push_back(take_from_deck(game, player, id));
    }

    inline void end_turns(pokebot::GameState& game, int count) {
        for (int i = 0; i < count; ++i) {
            pokebot::apply(game, pokebot::Action{pokebot::ActionKind::EndTurn, pokebot::kNoCard, pokebot::kActiveSlot, 0});
        }
    }

    inline bool offers(const std::vector<pokebot::Action>& actions, pokebot::ActionKind kind) {
        for (const auto& action : actions) {
            if (action.kind == kind) {
                return true;
            }
        }
        return false;
    }

    // The first legal action of `kind` whose card has id `card_id` ("" = any) and whose target is `target` (-1 = any).
    inline pokebot::Action find_action(const pokebot::GameState& game, pokebot::ActionKind kind,
                                       const std::string& card_id = "", int target = -1) {
        for (const auto& action : pokebot::legal_actions(game)) {
            if (action.kind != kind) {
                continue;
            }
            if (!card_id.empty() && (action.card == pokebot::kNoCard || game.def(action.card).id != card_id)) {
                continue;
            }
            if (target >= 0 && action.target != target) {
                continue;
            }
            return action;
        }
        FAIL("no legal action of that kind matches");
        return {};
    }

} // namespace test