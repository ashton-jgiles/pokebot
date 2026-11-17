#include "engine/rules/rules.hpp"

#include <algorithm>

namespace pokebot::rules {

    namespace {

        EnergyList to_list(const std::vector<EnergyType>& energy) {
            EnergyList list;
            for (const EnergyType type : energy) {
                list.push_back(type);
            }
            return list;
        }

        bool attacker_has_type(const GameState& game, const PokemonInPlay& attacker, EnergyType type) {
            return types(game, attacker).contains(type);
        }

        int attacker_side(const GameState&, const PokemonInPlay&, const PokemonInPlay&, int damage) {
            return damage;
        }

        int apply_weakness(const GameState& game, const PokemonInPlay& attacker, const PokemonInPlay& defender, int damage) {
            const auto w = weakness(game, defender);
            if (w && attacker_has_type(game, attacker, w->type)) {
                damage *= w->multiplier;
            }
            return damage;
        }

        int apply_resistance(const GameState& game, const PokemonInPlay& attacker, const PokemonInPlay& defender, int damage) {
            const auto r = resistance(game, defender);
            if (r && attacker_has_type(game, attacker, r->type)) {
                damage -= r->amount;
            }
            return damage;
        }

        int defender_side(const GameState&, const PokemonInPlay&, const PokemonInPlay&, int damage) {
            return damage;
        }

    } // namespace

    int max_hp(const GameState& game, const PokemonInPlay& pokemon) {
        return game.def(pokemon.top()).hp;
    }

    int remaining_hp(const GameState& game, const PokemonInPlay& pokemon) {
        return max_hp(game, pokemon) - 10 * static_cast<int>(pokemon.damage_counters);
    }

    bool is_knocked_out(const GameState& game, const PokemonInPlay& pokemon) {
        return remaining_hp(game, pokemon) <= 0;
    }

    EnergyList types(const GameState& game, const PokemonInPlay& pokemon) {
        return to_list(game.def(pokemon.top()).types);
    }

    std::optional<Weakness> weakness(const GameState& game, const PokemonInPlay& pokemon) {
        return game.def(pokemon.top()).weakness;
    }

    std::optional<Resistance> resistance(const GameState& game, const PokemonInPlay& pokemon) {
        return game.def(pokemon.top()).resistance;
    }

    int retreat_cost(const GameState& game, const PokemonInPlay& pokemon) {
        return std::max(game.def(pokemon.top()).retreat, 0);
    }

    const Attack& attack_text(const GameState& game, AttackRef attack) {
        return game.def(attack.text_from).attacks[attack.index];
    }

    EnergyList attack_cost(const GameState& game, const PokemonInPlay& pokemon, std::size_t attack) {
        return to_list(game.def(pokemon.top()).attacks[attack].cost);
    }

    EnergyList energy_provided(const GameState& game, const PokemonInPlay&, CardIndex energy) {
        return to_list(game.def(energy).provides);
    }

    int prize_value(const GameState& game, const PokemonInPlay& pokemon) {
        return game.def(pokemon.top()).prize_value();
    }

    bool is_active(const GameState& game, const PokemonInPlay& pokemon) {
        const PlayerState& p = game.player(owner_of(pokemon.id()));
        return p.active && p.active->id() == pokemon.id();
    }

    bool can_get_special_condition(const GameState& game, const PokemonInPlay& pokemon, SpecialCondition) {
        return is_active(game, pokemon);
    }

    int poison_counters(const GameState&, const PokemonInPlay& pokemon) {
        return pokemon.special.poison;
    }

    int burn_counters(const GameState&, const PokemonInPlay&) {
        return 2;
    }

    bool is_first_turn(const GameState& game) {
        return game.turn <= 2;
    }

    std::size_t bench_size(const GameState&, int) {
        return kBenchSize;
    }

    bool can_attach_energy(const GameState& game, int) {
        return !game.flags.energy_attached;
    }

    bool can_retreat(const GameState& game, int player) {
        const PlayerState& p = game.player(player);
        return !game.flags.retreated && p.active && !p.bench.empty() && !p.active->special.asleep && !p.active->special.paralyzed;
    }

    bool can_attack(const GameState& game, int player) {
        const PlayerState& p = game.player(player);
        return game.turn != 1 && game.player(player).active.has_value() && !p.active->special.asleep && !p.active->special.paralyzed;
    }

    bool can_play_item(const GameState&, int) {
        return true;
    }

    bool can_play_supporter(const GameState& game, int) {
        return !game.flags.supporter_played && game.turn != 1;
    }

    bool can_play_stadium(const GameState& game, int, CardIndex stadium) {
        const CardIndex in_play = stadium_in_play(game);
        return !game.flags.stadium_played && (in_play == kNoCard || game.def(in_play).name != game.def(stadium).name);
    }

    bool can_attach_tool(const GameState&, const PokemonInPlay& pokemon) {
        return pokemon.tool == kNoCard;
    }

    CardIndex stadium_in_play(const GameState& game) {
        for (int p = 0; p < kPlayers; ++p) {
            if (game.player(p).stadium != kNoCard) {
                return game.player(p).stadium;
            }
        }
        return kNoCard;
    }

    int attacks_per_turn(const GameState&, const PokemonInPlay&) {
        return 1;
    }

    bool can_evolve(const GameState& game, const PokemonInPlay& pokemon) {
        return !is_first_turn(game) && pokemon.turn_played != game.turn && !pokemon.stack.full();
    }

    bool evolves_into(const GameState& game, const PokemonInPlay& pokemon, CardIndex evolution) {
        const CardDef& def = game.def(evolution);
        return def.kind == CardKind::Pokemon && def.stage != Stage::Basic && !def.evolves_from.empty() &&
               def.evolves_from == game.def(pokemon.top()).name;
    }

    int attack_damage(const GameState& game, const PokemonInPlay& attacker, const PokemonInPlay& defender, const Attack& attack) {
        if (attack.damage <= 0) {
            return 0;
        }
        int damage = attack.damage;
        damage = attacker_side(game, attacker, defender, damage);
        damage = apply_weakness(game, attacker, defender, damage);
        damage = apply_resistance(game, attacker, defender, damage);
        damage = defender_side(game, attacker, defender, damage);
        return std::max(damage, 0);
    }

} // namespace pokebot::rules
