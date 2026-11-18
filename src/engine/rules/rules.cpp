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

        // A lasting effect of this kind on this player (and this Pokemon, unless it's player-wide) this turn.
        bool lasting_applies(const GameState& game, LastingKind kind, int player, CardIndex pokemon) {
            for (const LastingEffect& effect : game.lasting) {
                if (effect.kind == kind && effect.turn == game.turn && effect.player == player &&
                    (effect.pokemon == kNoCard || effect.pokemon == pokemon)) {
                    return true;
                }
            }
            return false;
        }

        // Calls visit(modifier, owner, holder) for every passive modifier in play: the Stadium's, each Tool's and each
        // Ability's. holder is the Pokemon a Tool is attached to or the Pokemon with the Ability, or nullptr for a Stadium.
        template <class Visit>
        void each_modifier(const GameState& game, Visit visit) {
            const CardIndex stadium = stadium_in_play(game);
            if (stadium != kNoCard) {
                for (const Modifier& m : game.def(stadium).modifiers) {
                    visit(m, owner_of(stadium), nullptr);
                }
            }
            for (int p = 0; p < kPlayers; ++p) {
                const PlayerState& player = game.player(p);
                const auto each_pokemon = [&](const PokemonInPlay& pokemon) {
                    if (pokemon.tool != kNoCard) {
                        for (const Modifier& m : game.def(pokemon.tool).modifiers) {
                            visit(m, p, &pokemon);
                        }
                    }
                    for (const Ability& ability : game.def(pokemon.top()).abilities) {
                        for (const Modifier& m : ability.modifiers) {
                            visit(m, p, &pokemon);
                        }
                    }
                };
                if (player.active) {
                    each_pokemon(*player.active);
                }
                for (const PokemonInPlay& pokemon : player.bench) {
                    each_pokemon(pokemon);
                }
            }
        }

        bool applies(const GameState& game, const Modifier& m, int owner, const PokemonInPlay* holder, const PokemonInPlay& target) {
            bool in_reach = false;
            switch (m.to) {
                case AppliesTo::Holder:
                    in_reach = holder != nullptr && holder->id() == target.id();
                    break;
                case AppliesTo::Yours:
                    in_reach = owner_of(target.id()) == owner;
                    break;
                case AppliesTo::Opponents:
                    in_reach = owner_of(target.id()) != owner;
                    break;
                case AppliesTo::All:
                    in_reach = true;
                    break;
            }
            return in_reach && matches(m.filter, game.def(target.top())) && (!m.only_if_poisoned || target.special.poison > 0);
        }

        int attacker_side(const GameState& game, const PokemonInPlay& attacker, const PokemonInPlay& defender, int damage) {
            if (is_active(game, defender)) {
                each_modifier(game, [&](const Modifier& m, int owner, const PokemonInPlay* holder) {
                    if (m.what == Modifies::DamageDealt && applies(game, m, owner, holder, attacker)) {
                        damage += m.add;
                    }
                });
            }
            const int player = owner_of(attacker.id());
            const bool defender_ex = game.def(defender.top()).rules.ex;
            for (const LastingEffect& effect : game.lasting) {
                if (effect.kind == LastingKind::DamageBonus && effect.turn == game.turn && effect.player == player &&
                    is_active(game, defender) && (!effect.only_ex || defender_ex)) {
                    damage += effect.amount;
                }
            }
            return damage;
        }

        int apply_weakness(const GameState& game, const PokemonInPlay& attacker, const PokemonInPlay& defender, int damage, bool printed) {
            const auto w = printed ? game.def(defender.top()).weakness : weakness(game, defender);
            if (w && attacker_has_type(game, attacker, w->type)) {
                damage *= w->multiplier;
            }
            return damage;
        }

        int apply_resistance(const GameState& game, const PokemonInPlay& attacker, const PokemonInPlay& defender, int damage, bool printed) {
            const auto r = printed ? game.def(defender.top()).resistance : resistance(game, defender);
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
        int hp = game.def(pokemon.top()).hp;
        each_modifier(game, [&](const Modifier& m, int owner, const PokemonInPlay* holder) {
            if (m.what == Modifies::MaxHp && applies(game, m, owner, holder, pokemon)) {
                hp += m.add;
            }
        });
        return hp;
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
        std::optional<Weakness> w = game.def(pokemon.top()).weakness;
        each_modifier(game, [&](const Modifier& m, int owner, const PokemonInPlay* holder) {
            if (m.what == Modifies::WeaknessType && applies(game, m, owner, holder, pokemon)) {
                w = Weakness{*m.type, w ? w->multiplier : 2};
            }
        });
        return w;
    }

    std::optional<Resistance> resistance(const GameState& game, const PokemonInPlay& pokemon) {
        return game.def(pokemon.top()).resistance;
    }

    int retreat_cost(const GameState& game, const PokemonInPlay& pokemon) {
        int cost = game.def(pokemon.top()).retreat;
        std::optional<int> set;
        each_modifier(game, [&](const Modifier& m, int owner, const PokemonInPlay* holder) {
            if (m.what == Modifies::RetreatCost && applies(game, m, owner, holder, pokemon)) {
                cost += m.add;
                if (m.set) {
                    set = set ? std::min(*set, *m.set) : *m.set;
                }
            }
        });
        return std::max(set ? *set : cost, 0);
    }

    const Attack& attack_text(const GameState& game, AttackRef attack) {
        return game.def(attack.text_from).attacks[attack.index];
    }

    EnergyList attack_cost(const GameState& game, const PokemonInPlay& pokemon, std::size_t attack) {
        return to_list(game.def(pokemon.top()).attacks[attack].cost);
    }

    EnergyUnits energy_provided(const GameState& game, const PokemonInPlay& pokemon, CardIndex energy) {
        const CardDef& def = game.def(energy);
        EnergyUnits units;
        for (const ConditionalProvides& when : def.conditional_provides) {
            if (matches(when.holder, game.def(pokemon.top()))) {
                for (const EnergyMask unit : when.provides) {
                    units.push_back(unit);
                }
                return units;
            }
        }
        for (const EnergyType type : def.provides) {
            units.push_back(energy_mask(type));
        }
        return units;
    }

    std::size_t energy_units(const GameState& game, const PokemonInPlay& pokemon) {
        std::size_t total = 0;
        for (const CardIndex card : pokemon.energy) {
            total += energy_provided(game, pokemon, card).size();
        }
        return total;
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

    std::size_t bench_size(const GameState& game, int player) {
        std::size_t size = kBenchSize;
        bool has_tera = false;
        const PlayerState& p = game.player(player);
        if (p.active && game.def(p.active->top()).rules.tera) {
            has_tera = true;
        }
        for (const PokemonInPlay& pokemon : p.bench) {
            has_tera = has_tera || game.def(pokemon.top()).rules.tera;
        }
        each_modifier(game, [&](const Modifier& m, int, const PokemonInPlay*) {
            if (m.what == Modifies::BenchSize && m.set && (!m.only_with_tera || has_tera)) {
                size = std::max(size, static_cast<std::size_t>(*m.set));
            }
        });
        return std::min(size, kMaxBenchSize);
    }

    bool can_attach_energy(const GameState& game, int) {
        return !game.flags.energy_attached;
    }

    bool can_retreat(const GameState& game, int player) {
        const PlayerState& p = game.player(player);
        return !game.flags.retreated && p.active && !p.bench.empty() && !p.active->special.asleep && !p.active->special.paralyzed &&
               !lasting_applies(game, LastingKind::CantRetreat, player, p.active->id());
    }

    bool can_attack(const GameState& game, int player) {
        const PlayerState& p = game.player(player);
        return game.turn != 1 && game.player(player).active.has_value() && !p.active->special.asleep && !p.active->special.paralyzed &&
               !lasting_applies(game, LastingKind::CantAttack, player, p.active->id());
    }

    bool has_abilities(const GameState&, const PokemonInPlay&) {
        return true;
    }

    bool can_use_attack(const GameState& game, const PokemonInPlay& pokemon, std::size_t attack) {
        const AttackRef ref{pokemon.top(), static_cast<std::uint8_t>(attack)};
        for (const LastingEffect& effect : game.lasting) {
            if (effect.kind == LastingKind::CantUseAttack && effect.turn == game.turn && effect.pokemon == pokemon.id() && effect.attack == ref) {
                return false;
            }
        }
        return true;
    }

    bool can_play_item(const GameState& game, int player) {
        return !lasting_applies(game, LastingKind::NoItems, player, kNoCard);
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

    bool damage_prevented(const GameState& game, const PokemonInPlay&, const PokemonInPlay& defender) {
        return game.def(defender.top()).rules.tera && !is_active(game, defender);
    }

    int attack_damage(const GameState& game, const PokemonInPlay& attacker, const PokemonInPlay& defender, int base, bool ignore_defender_effects) {
        if (base <= 0 || damage_prevented(game, attacker, defender)) {
            return 0;
        }
        int damage = base;
        damage = attacker_side(game, attacker, defender, damage);
        if (is_active(game, defender)) {
            damage = apply_weakness(game, attacker, defender, damage, ignore_defender_effects);
            damage = apply_resistance(game, attacker, defender, damage, ignore_defender_effects);
        }
        if (!ignore_defender_effects) {
            damage = defender_side(game, attacker, defender, damage);
        }
        return std::max(damage, 0);
    }

} // namespace pokebot::rules
