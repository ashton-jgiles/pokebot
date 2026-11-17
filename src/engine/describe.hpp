#pragma once

#include <string>

#include "engine/action.hpp"
#include "engine/game_state.hpp"

namespace pokebot {

    std::string describe(const GameState& game);
    std::string describe(const GameState& game, const Action& action);

} // namespace pokebot
