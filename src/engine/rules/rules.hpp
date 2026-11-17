#pragma once

#include <cstddef>
#include <optional>

#include "engine/game/game_state.hpp"

// Every rule a card can change is asked here, never read straight from a CardDef.
// Each function starts from the rulebook answer; card effects will modify it later.
namespace pokebot::rules {

    using EnergyList = FixedVector<EnergyType, 8>;

    // Pokemon in play
    int max_hp(const GameState& game, const PokemonInPlay& pokemon);
    int remaining_hp(const GameState& game, const PokemonInPlay& pokemon);
    bool is_knocked_out(const GameState& game, const PokemonInPlay& pokemon);
    EnergyList types(const GameState& game, const PokemonInPlay& pokemon);
    std::optional<Weakness> weakness(const GameState& game, const PokemonInPlay& pokemon);
    std::optional<Resistance> resistance(const GameState& game, const PokemonInPlay& pokemon);
    int retreat_cost(const GameState& game, const PokemonInPlay& pokemon);
    EnergyList attack_cost(const GameState& game, const PokemonInPlay& pokemon, std::size_t attack);
    EnergyList energy_provided(const GameState& game, const PokemonInPlay& pokemon, CardIndex energy);
    int prize_value(const GameState& game, const PokemonInPlay& pokemon);

    // What a player may do this turn. Turn 1 is the first player's first turn, turn 2 the second player's
    bool is_first_turn(const GameState& game);
    std::size_t bench_size(const GameState& game, int player);
    bool can_attach_energy(const GameState& game, int player);
    bool can_retreat(const GameState& game, int player);
    bool can_attack(const GameState& game, int player);
    bool can_evolve(const GameState& game, const PokemonInPlay& pokemon);
    bool evolves_into(const GameState& game, const PokemonInPlay& pokemon, CardIndex evolution);

    // base damage -> attacker effects -> Weakness -> Resistance -> defender effects
    int attack_damage(const GameState& game, const PokemonInPlay& attacker, const PokemonInPlay& defender, const Attack& attack);

} // namespace pokebot::rules
