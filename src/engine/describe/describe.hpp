#pragma once

#include <string>

#include "engine/actions/action.hpp"
#include "engine/game/game_state.hpp"
#include "engine/observation/observation.hpp"

namespace pokebot {

    std::string describe(const GameState& game);
    std::string describe(const GameState& game, const Action& action);
    // How `viewer` saw `action` taken in `before`, which led to `after`. A card they knew the place of
    // neither before nor after, such as one an opponent's search put in their hand, is "an unseen card".
    std::string describe(const GameState& before, const Action& action, const GameState& after, int viewer);
    // The game as observation.viewer sees it: the opponent's hand as a count, and the cards known in it.
    std::string describe(const Observation& observation);
    // An action as observation.viewer is offered it, named only from what they can see.
    std::string describe(const Observation& observation, const Action& action);
    // What observation.viewer is being asked, such as "choose cards for an effect (Ultra Ball)".
    std::string describe_decision(const Observation& observation);

} // namespace pokebot
