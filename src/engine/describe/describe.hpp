#pragma once

#include <string>

#include "engine/actions/action.hpp"
#include "engine/game/game_state.hpp"
#include "engine/observation/observation.hpp"

namespace pokebot {

    std::string describe(const GameState& game);
    std::string describe(const GameState& game, const Action& action);
    // The game as observation.viewer sees it: the opponent's hand as a count, and the cards known in it.
    std::string describe(const Observation& observation);

} // namespace pokebot
