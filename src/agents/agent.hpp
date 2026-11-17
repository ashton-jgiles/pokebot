#pragma once

#include <string>
#include <vector>

#include "engine/action.hpp"
#include "engine/game_state.hpp"

namespace pokebot {

    // Something that makes the decisions in a game. Until the observation layer (Milestone 4),
    // agents are handed the whole game state, hidden cards included.
    class Agent {
    public:
        virtual ~Agent() = default;

        virtual std::string name() const = 0;

        // Picks one of `actions`, the legal choices for game.decision(). `actions` is never empty.
        virtual Action choose(const GameState& game, const std::vector<Action>& actions) = 0;
    };

} // namespace pokebot
