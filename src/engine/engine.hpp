#pragma once

#include <cstdint>
#include <vector>

#include "engine/actions/action.hpp"
#include "engine/game/game_state.hpp"

namespace pokebot {

    GameState new_game(const CardTable& cards, std::uint64_t seed);
    std::vector<Action> legal_actions(const GameState& game);

    void apply(GameState& game, const Action& action);

} // namespace pokebot
