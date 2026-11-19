#pragma once

#include <cstdint>
#include <vector>

#include "engine/actions/action.hpp"
#include "engine/game/game_state.hpp"

namespace pokebot {

    GameState new_game(const CardTable& cards, std::uint64_t seed);
    std::vector<Action> legal_actions(const GameState& game);

    void apply(GameState& game, const Action& action);
    // apply without checking legality, for an action the caller just took from legal_actions(game).
    void apply_legal(GameState& game, const Action& action);

    // Whose Pokemon the slots in the pending decision's actions mean. Usually the deciding player's,
    // but an effect such as Boss's Orders has them choose one of the opponent's.
    int choice_owner(const GameState& game);

} // namespace pokebot
