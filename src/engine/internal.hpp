#pragma once

#include <vector>

#include "engine/actions/action.hpp"
#include "engine/events/events.hpp"
#include "engine/game/game_state.hpp"
#include "engine/rules/rules.hpp"

namespace pokebot::detail {

    // check is basic pokemon
    bool is_basic_pokemon(const GameState& game, CardIndex card);

    // a pokemon put into play on turn 0 during setup
    PokemonInPlay new_pokemon(CardIndex card, std::uint16_t turn);

    // slot 0 is the active pokemon slots 1-5 are the bench. the slot must be occupied
    PokemonInPlay& pokemon_at(PlayerState& player, Slot slot);
    const PokemonInPlay& pokemon_at(const PlayerState& player, Slot slot);

    // the Pokemon in play whose id() is this card, or nullptr if it has left play
    PokemonInPlay* find_pokemon(GameState& game, CardIndex id);
    const PokemonInPlay* find_pokemon(const GameState& game, CardIndex id);

    // every occupied slots the active pokemon first then the bench in order left to right
    FixedVector<Slot, kMaxBenchSize + 1> in_play_slots(const PlayerState& player);

    // draw cards to get starting hand and cards in subsequent turns
    int draw_cards(GameState& game, int player, int count);

    // set the winner of the game
    void set_winner(GameState& game, int player);

    // events.cpp: tells the event hook, and later every card that reacts to this event
    void raise(GameState& game, const Event& event);

    // turn.cpp
    void begin_turn(GameState& game, int player);
    void end_turn(GameState& game);
    void continue_game(GameState& game);

    // setup.cpp
    void add_setup_actions(const GameState& game, const Decision& decision, std::vector<Action>& out);
    void apply_setup_action(GameState& game, const Action& action);

    // main_actions.cpp
    void add_main_phase_actions(const GameState& game, std::vector<Action>& out);
    void apply_main_phase_action(GameState& game, const Action& action);
    void add_retreat_discard_actions(const GameState& game, std::vector<Action>& out);
    void apply_retreat_discard(GameState& game, const Action& action);

    // combat.cpp
    bool can_pay_cost(const GameState& game, const PokemonInPlay& pokemon, const rules::EnergyList& cost);
    int attack_damage(const GameState& game, const PokemonInPlay& attacker, const PokemonInPlay& defender, const Attack& attack);
    void add_attack_actions(const GameState& game, std::vector<Action>& out);
    void apply_attack(GameState& game, const Action& action);
    void perform_attack(GameState& game, const EffectContext& who, AttackRef attack);
    void deal_attack_damage(GameState& game, const EffectContext& who, int base, bool ignore_defender = false);
    void finish_attack(GameState& game, const EffectContext& who);
    void handle_knockouts(GameState& game, bool after_attack = false);
    void add_promote_actions(const GameState& game, std::vector<Action>& out);
    void add_extra_attack_actions(const GameState& game, std::vector<Action>& out);
    void apply_promote(GameState& game, const Action& action);

    // conditions.cpp
    void apply_special_condition(GameState& game, PokemonInPlay& pokemon, SpecialCondition condition, int poison_counters);
    void clear_special_conditions(PokemonInPlay& pokemon);
    void special_conditions_checkup(GameState& game);
    bool confusion_stops_attack(GameState& game, PokemonInPlay& attacker);

    // lasting.cpp
    void add_lasting(GameState& game, const EffectRun& run, const Step& step);
    void end_effects_on(GameState& game, CardIndex pokemon); // it moved to the Bench, evolved or left play
    void expire_lasting(GameState& game);

    // effects/interpreter.cpp
    bool is_effect_decision(DecisionKind kind);
    bool condition_holds(const GameState& game, int player, const Condition& condition, CardIndex playing, const EffectRun* run = nullptr);
    bool requirements_met(const GameState& game, int player, CardIndex card);
    void start_effect(GameState& game, const EffectContext& who, ProgramRef program, AfterEffect then);
    // Runs the effect now, or, if one is already running, right after its current step.
    void queue_effect(GameState& game, const EffectContext& who, ProgramRef program, AfterEffect then);

    // effects/custom.cpp
    bool custom_condition(const GameState& game, int player, CustomEffect effect);
    bool run_custom(GameState& game, EffectRun& run, const Step& step); // false while waiting for the choice
    void add_custom_actions(const GameState& game, const EffectRun& run, const Step& step, std::vector<Action>& out);
    void apply_custom(GameState& game, const EffectRun& run, const Step& step, const Action& action);

    // actions/bench_limit.cpp
    void enforce_bench_limits(GameState& game, int first);
    void add_discard_benched_actions(const GameState& game, std::vector<Action>& out);
    void apply_discard_benched(GameState& game, const Action& action);

    // effects/triggers.cpp
    bool ability_allowed(const GameState& game, int player, const PokemonInPlay& pokemon, std::size_t index);
    void run_triggers(GameState& game, const Event& event);
    void add_effect_choice_actions(const GameState& game, std::vector<Action>& out);
    void apply_effect_choice(GameState& game, const Action& action);
} // namespace pokebot::detail
