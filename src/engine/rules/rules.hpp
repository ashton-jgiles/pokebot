#pragma once

#include <cstddef>
#include <optional>

#include "engine/game/game_state.hpp"

// Every rule a card can change is asked here, never read straight from a CardDef.
// Each function starts from the rulebook answer; card effects will modify it later.
namespace pokebot::rules {

    using EnergyList = FixedVector<EnergyType, 8>;
    using EnergyUnits = FixedVector<EnergyMask, 4>;

    // Pokemon in play
    int max_hp(const GameState& game, const PokemonInPlay& pokemon);
    int remaining_hp(const GameState& game, const PokemonInPlay& pokemon);
    bool is_knocked_out(const GameState& game, const PokemonInPlay& pokemon);
    EnergyList types(const GameState& game, const PokemonInPlay& pokemon);
    std::optional<Weakness> weakness(const GameState& game, const PokemonInPlay& pokemon);
    std::optional<Resistance> resistance(const GameState& game, const PokemonInPlay& pokemon);
    int retreat_cost(const GameState& game, const PokemonInPlay& pokemon);
    const Attack& attack_text(const GameState& game, AttackRef attack);
    EnergyList attack_cost(const GameState& game, const PokemonInPlay& pokemon, std::size_t attack);
    // What one attached Energy card provides, as units (see EnergyMask). Most provide one unit of one type.
    EnergyUnits energy_provided(const GameState& game, const PokemonInPlay& pokemon, CardIndex energy);
    // The total Energy units attached to the Pokemon: what pays its attack costs and Retreat Cost.
    std::size_t energy_units(const GameState& game, const PokemonInPlay& pokemon);
    int prize_value(const GameState& game, const PokemonInPlay& pokemon);
    bool is_active(const GameState& game, const PokemonInPlay& pokemon);
    bool can_get_special_condition(const GameState& game, const PokemonInPlay& pokemon, SpecialCondition condition);
    int poison_counters(const GameState& game, const PokemonInPlay& pokemon);
    int burn_counters(const GameState& game, const PokemonInPlay& pokemon);

    // What a player may do this turn. Turn 1 is the first player's first turn, turn 2 the second player's
    bool is_first_turn(const GameState& game);
    std::size_t bench_size(const GameState& game, int player);
    bool can_attach_energy(const GameState& game, int player);
    bool can_retreat(const GameState& game, int player);
    bool can_attack(const GameState& game, int player);
    bool has_abilities(const GameState& game, const PokemonInPlay& pokemon);
    bool can_use_attack(const GameState& game, const PokemonInPlay& pokemon, std::size_t attack);
    bool can_play_item(const GameState& game, int player);
    bool can_play_supporter(const GameState& game, int player);
    bool can_play_stadium(const GameState& game, int player, CardIndex stadium);
    bool can_attach_tool(const GameState& game, const PokemonInPlay& pokemon);
    CardIndex stadium_in_play(const GameState& game);
    int attacks_per_turn(const GameState& game, const PokemonInPlay& pokemon);
    bool can_evolve(const GameState& game, const PokemonInPlay& pokemon);
    bool evolves_into(const GameState& game, const PokemonInPlay& pokemon, CardIndex evolution);

    // base damage -> attacker effects -> Weakness -> Resistance -> defender effects
    bool damage_prevented(const GameState& game, const PokemonInPlay& attacker, const PokemonInPlay& defender);
    // ignore_defender_effects: "this attack's damage isn't affected by any effects on your opponent's Active Pokemon"
    int attack_damage(const GameState& game, const PokemonInPlay& attacker, const PokemonInPlay& defender, int base,
                      bool ignore_defender_effects = false);

} // namespace pokebot::rules
