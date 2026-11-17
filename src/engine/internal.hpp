#pragma once

#include <vector>

#include "engine/action.hpp"
#include "engine/game_state.hpp"

namespace pokebot::detail {

    // check is basic pokemon
    bool is_basic_pokemon(const GameState& game, CardIndex card);

    // turn 1 is the first players turn and turn 2 is the second players turn
    bool is_first_turn(const GameState& game);

    // a pokemon put into play on turn 0 during setup
    PokemonInPlay new_pokemon(CardIndex card, std::uint16_t turn);

    // slot 0 is the active pokemon slots 1-5 are the bench. the slot must be occupied
    PokemonInPlay& pokemon_at(PlayerState& player, Slot slot);
    const PokemonInPlay& pokemon_at(const PlayerState& player, Slot slot);

    // every occupied slots the active pokemon first then the bench in order left to right
    FixedVector<Slot, kBenchSize + 1> in_play_slots(const PlayerState& player);

    // draw cards to get starting hand and cards in subsequent turns
    int draw_cards(GameState& game, int player, int count);

    // set the winner of the game
    void set_winner(GameState& game, int player);

    // turn.cpp
    void begin_turn(GameState& game, int player);
    void end_turn(GameState& game);

    // setup.cpp
    void add_setup_actions(const GameState& game, const Decision& decision, std::vector<Action>& out);
    void apply_setup_action(GameState& game, const Action& action);

    // main_actions.cpp
    void add_main_phase_actions(const GameState& game, std::vector<Action>& out);
    void apply_main_phase_action(GameState& game, const Action& action);
    void add_retreat_discard_actions(const GameState& game, std::vector<Action>& out);
    void apply_retreat_discard(GameState& game, const Action& action);

} // namespace pokebot::detail
