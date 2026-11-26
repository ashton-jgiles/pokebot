#include <algorithm>

#include "engine/internal.hpp"

namespace pokebot {

    namespace detail {

        namespace {

            void add_damage_counters(PokemonInPlay& pokemon, int counters) {
                pokemon.damage_counters = static_cast<std::uint16_t>(pokemon.damage_counters + counters);
            }

            // Rulebook order: Poisoned, Burned, Asleep, Paralyzed.
            void checkup_one(GameState& game, int owner, PokemonInPlay& pokemon) {
                SpecialConditions& special = pokemon.special;
                if (special.any() && rules::special_conditions_immune(game, pokemon)) {
                    special = SpecialConditions{};
                    return;
                }
                if (special.poison > 0) {
                    add_damage_counters(pokemon, rules::poison_counters(game, pokemon));
                }
                if (special.burned) {
                    add_damage_counters(pokemon, rules::burn_counters(game, pokemon));
                    if (game.rng.coin_flip()) {
                        special.burned = false;
                    }
                }
                if (special.asleep && game.rng.coin_flip()) {
                    special.asleep = false;
                }
                if (special.paralyzed && owner == game.current_player && game.turn > special.paralyzed_turn) {
                    special.paralyzed = false;
                }
            }

        } // namespace

        void apply_special_condition(GameState& game, PokemonInPlay& pokemon, SpecialCondition condition, int poison_counters) {
            if (!rules::can_get_special_condition(game, pokemon, condition)) {
                return;
            }
            SpecialConditions& special = pokemon.special;
            switch (condition) {
                case SpecialCondition::Asleep:
                case SpecialCondition::Confused:
                case SpecialCondition::Paralyzed:
                    special.asleep = condition == SpecialCondition::Asleep;
                    special.confused = condition == SpecialCondition::Confused;
                    special.paralyzed = condition == SpecialCondition::Paralyzed;
                    if (special.paralyzed) {
                        special.paralyzed_turn = game.turn;
                    }
                    break;
                case SpecialCondition::Burned:
                    special.burned = true;
                    break;
                case SpecialCondition::Poisoned:
                    special.poison = static_cast<std::uint8_t>(std::max(poison_counters, 1));
                    break;
            }
        }

        void clear_special_conditions(PokemonInPlay& pokemon) {
            pokemon.special = SpecialConditions{};
        }

        void special_conditions_checkup(GameState& game) {
            const int current = game.current_player;
            for (const int player : {current, opponent_of(current)}) {
                PlayerState& p = game.player(player);
                if (p.active) {
                    checkup_one(game, player, *p.active);
                }
            }
        }

        bool confusion_stops_attack(GameState& game, PokemonInPlay& attacker) {
            if (!attacker.special.confused || game.rng.coin_flip()) {
                return false;
            }
            add_damage_counters(attacker, 3);
            return true;
        }

    } // namespace detail

} // namespace pokebot