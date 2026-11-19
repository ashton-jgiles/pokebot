#pragma once

#include <string>
#include <utility>
#include <vector>

#include "engine/actions/action.hpp"
#include "engine/observation/observation.hpp"

namespace pokebot {

    // Something that makes the decisions in a game. It sees only what its player could see at the
    // table, never the game state itself.
    class Agent {
    public:
        virtual ~Agent() = default;

        virtual std::string name() const = 0;

        // Picks one of `actions`, the legal choices for observation.decision, which is always this
        // agent's player's to make. `actions` is never empty.
        virtual Action choose(const Observation& observation, const std::vector<Action>& actions) = 0;

        virtual std::vector<std::pair<std::string, double>> counters() const { return {}; }
    };

} // namespace pokebot
