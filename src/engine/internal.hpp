#pragma once

#include <vector>

#include "engine/action.hpp"
#include "engine/game_state.hpp"

namespace pokebot::detail {

    bool is_basic_pokemon(const GameState& game, CardIndex card);

    int draw_cards(GameState& game, int player, int count);

    void set_winner(GameState& game, int player);

    void begin_turn(GameState& game, int player);

    void add_setup_actions(const GameState& game, const Decision& decision, std::vector<Action>& out);

    void apply_setup_action(GameState& game, const Action& action);

    void add_main_phase_actions(const GameState& game, std::vector<Action>& out);

    void apply_main_phase_action(GameState& game, const Action& action);

} // namespace pokebot::detail
