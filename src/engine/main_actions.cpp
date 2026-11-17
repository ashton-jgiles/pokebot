#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

#include "engine/internal.hpp"

namespace pokebot {

    namespace detail {

        namespace {

            using Choice = std::pair<const CardDef*, Slot>;

            bool first_time(std::vector<Choice>& seen, const CardDef* def, Slot slot) {
                const Choice choice{def, slot};
                if (std::find(seen.begin(), seen.end(), choice) != seen.end()) {
                    return false;
                }
                seen.push_back(choice);
                return true;
            }

            bool is_energy(const GameState& game, CardIndex card) {
                return game.def(card).kind == CardKind::Energy;
            }

            bool can_evolve_into(const GameState& game, CardIndex evolution, const PokemonInPlay& pokemon) {
                const CardDef& def = game.def(evolution);
                return def.kind == CardKind::Pokemon && def.stage != Stage::Basic && !def.evolves_from.empty() && def.evolves_from == game.def(pokemon.top()).name;
            }

            std::size_t retreat_cost(const GameState& game, const PokemonInPlay& pokemon) {
                return static_cast<std::size_t>(game.def(pokemon.top()).retreat);
            }

            void add_play_basic_actions(const GameState& game, int player, std::vector<Action>& out) {
                const PlayerState& p = game.player(player);
                if (p.bench.full()) {
                    return;
                }
                std::vector<Choice> seen;
                for (const CardIndex card : p.hand) {
                    if (is_basic_pokemon(game, card) && first_time(seen, &game.def(card), kActiveSlot)) {
                        out.push_back(Action{ActionKind::PlayBasic, card, kActiveSlot, 0});
                    }
                }
            }

            void add_evolve_actions(const GameState& game, int player, std::vector<Action>& out) {
                if (is_first_turn(game)) {
                    return;
                }
                const PlayerState& p = game.player(player);
                std::vector<Choice> seen;
                for (const Slot slot : in_play_slots(p)) {
                    const PokemonInPlay& pokemon = pokemon_at(p, slot);
                    if (pokemon.turn_played == game.turn || pokemon.stack.full()) {
                        continue;
                    }
                    for (const CardIndex card : p.hand) {
                        if (can_evolve_into(game, card, pokemon) && first_time(seen, &game.def(card), slot)) {
                            out.push_back(Action{ActionKind::Evolve, card, slot, 0});
                        }
                    }
                }
            }

            void add_attach_actions(const GameState& game, int player, std::vector<Action>& out) {
                if (game.flags.energy_attached) {
                    return;
                }
                const PlayerState& p = game.player(player);
                std::vector<Choice> seen;
                for (const CardIndex card : p.hand) {
                    if (!is_energy(game, card)) {
                        continue;
                    }
                    for (const Slot slot : in_play_slots(p)) {
                        if (first_time(seen, &game.def(card), slot)) {
                            out.push_back(Action{ActionKind::AttachEnergy, card, slot, 0});
                        }
                    }
                }
            }

            void add_retreat_actions(const GameState& game, int player, std::vector<Action>& out) {
                const PlayerState& p = game.player(player);
                if (game.flags.retreated || !p.active || p.bench.empty()) {
                    return;
                }
                if (p.active->energy.size() < retreat_cost(game, *p.active)) {
                    return;
                }
                for (std::size_t i = 0; i < p.bench.size(); ++i) {
                    out.push_back(Action{ActionKind::Retreat, kNoCard, bench_slot(i), 0});
                }
            }

            void discard_attatched_energy(PlayerState& player, CardIndex card) {
                player.active->energy.remove(card);
                player.discard.push_back(card);
            }

            void swap_with_bench(PlayerState& player, Slot target) {
                std::swap(*player.active, pokemon_at(player, target));
            }

            bool discard_choice_matters(const GameState& game, const PokemonInPlay& pokemon, std::size_t cost) {
                if (cost == 0 || cost == pokemon.energy.size()) {
                    return false;
                }
                for (const CardIndex card : pokemon.energy) {
                    if (&game.def(card) != &game.def(pokemon.energy[0])) {
                        return true;
                    }
                }
                return false;
            }

            void start_retreat(GameState& game, int player, Slot target) {
                PlayerState& p = game.player(player);
                game.flags.retreated = true;
                const std::size_t cost = retreat_cost(game, *p.active);
                if (discard_choice_matters(game, *p.active, cost)) {
                    game.pending.push_back(Decision{DecisionKind::RetreatDiscard, static_cast<std::uint8_t>(player), static_cast<std::uint8_t>(cost), target});
                    return;
                }
                for (std::size_t i = 0; i < cost; ++i) {
                    discard_attatched_energy(p, p.active->energy.back());
                }
                swap_with_bench(p, target);
            }
        } // namespace

        void add_main_phase_actions(const GameState& game, std::vector<Action>& out) {
            const int player = game.current_player;
            add_play_basic_actions(game, player, out);
            add_evolve_actions(game, player, out);
            add_attach_actions(game, player, out);
            add_retreat_actions(game, player, out);
            // more actions to come
            out.push_back(Action{ActionKind::EndTurn, kNoCard, kActiveSlot, 0});
        }

        void apply_main_phase_action(GameState& game, const Action& action) {
            PlayerState& p = game.player(game.current_player);
            switch (action.kind) {
                case ActionKind::PlayBasic:
                    p.hand.remove(action.card);
                    p.bench.push_back(new_pokemon(action.card, game.turn));
                    break;
                case ActionKind::Evolve: {
                    p.hand.remove(action.card);
                    PokemonInPlay& pokemon = pokemon_at(p, action.target);
                    pokemon.stack.push_back(action.card);
                    pokemon.turn_played = game.turn;
                    break;
                }
                case ActionKind::AttachEnergy:
                    p.hand.remove(action.card);
                    pokemon_at(p, action.target).energy.push_back(action.card);
                    game.flags.energy_attached = true;
                    break;
                case ActionKind::Retreat:
                    start_retreat(game, game.current_player, action.target);
                    break;
                case ActionKind::EndTurn:
                    end_turn(game);
                    break;
                default:
                    throw std::logic_error("apply_main_phase_action: not a main-phase action");
            }
        }

        void add_retreat_discard_actions(const GameState& game, std::vector<Action>& out) {
            const PokemonInPlay& active = *game.player(game.decision().player).active;
            std::vector<Choice> seen;
            for (const CardIndex card : active.energy) {
                if (first_time(seen, &game.def(card), kActiveSlot)) {
                    out.push_back(Action{ActionKind::DiscardEnergy, card, kActiveSlot, 0});
                }
            }
        }

        void apply_retreat_discard(GameState& game, const Action& action) {
            Decision& decision = game.pending.back();
            PlayerState& p = game.player(decision.player);
            discard_attatched_energy(p, action.card);
            --decision.amount;
            if (decision.amount == 0) {
                const Slot target = decision.target;
                game.pending.pop_back();
                swap_with_bench(p, target);
            }
        }
    } // namespace detail

} // namespace pokebot
