#pragma once

#include <cstddef>

#include <catch2/catch_test_macros.hpp>

#include "engine/engine.hpp"

namespace test {

    // Every card a player owns must be in exactly one place, so this should always be 60.
    inline std::size_t cards_accounted_for(const pokebot::PlayerState& p) {
        std::size_t n = p.deck.size() + p.hand.size() + p.prizes.size() + p.discard.size() + p.lost_zone.size();
        if (p.active) {
            n += p.active->stack.size() + p.active->energy.size();
        }
        for (const auto& pokemon : p.bench) {
            n += pokemon.stack.size() + pokemon.energy.size();
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

} // namespace test