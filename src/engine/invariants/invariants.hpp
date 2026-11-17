#pragma once

#include <string>
#include <vector>

#include "engine/game/game_state.hpp"

namespace pokebot {

    std::vector<std::string> check_invariants(const GameState& game);

} // namespace pokebot
