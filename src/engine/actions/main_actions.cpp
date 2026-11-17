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

            std::size_t retreat_cost(const GameState& game, const PokemonInPlay& pokemon) {
                return static_cast<std::size_t>(rules::retreat_cost(game, pokemon));
            }

            void add_play_basic_actions(const GameState& game, int player, std::vector<Action>& out) {
                const PlayerState& p = game.player(player);
                if (p.bench.size() >= rules::bench_size(game, player)) {
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
                const PlayerState& p = game.player(player);
                std::vector<Choice> seen;
                for (const Slot slot : in_play_slots(p)) {
                    const PokemonInPlay& pokemon = pokemon_at(p, slot);
                    if (!rules::can_evolve(game, pokemon)) {
                        continue;
                    }
                    for (const CardIndex card : p.hand) {
                        if (rules::evolves_into(game, pokemon, card) && first_time(seen, &game.def(card), slot)) {
                            out.push_back(Action{ActionKind::Evolve, card, slot, 0});
                        }
                    }
                }
            }

            void add_attach_actions(const GameState& game, int player, std::vector<Action>& out) {
                if (!rules::can_attach_energy(game, player)) {
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
                if (!rules::can_retreat(game, player)) {
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

            void finish_retreat(GameState& game, int player, Slot target) {
                PlayerState& p = game.player(player);
                const CardIndex retreated = p.active->id();
                std::swap(*p.active, pokemon_at(p, target));
                const auto owner = static_cast<std::uint8_t>(player);
                raise(game, Event{EventKind::Retreated, owner, retreated, kNoCard});
                raise(game, Event{EventKind::MovedToBench, owner, retreated, kNoCard});
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
                finish_retreat(game, player, target);
            }

                        bool effects_written(const CardDef& def) {
                return def.text.empty() || !def.effects.empty();
            }

            bool can_play_trainer(const GameState& game, int player, CardIndex card) {
                const CardDef& def = game.def(card);
                switch (def.trainer_kind) {
                    case TrainerKind::Item:
                        return effects_written(def) && rules::can_play_item(game, player) && requirements_met(game, player, card);
                    case TrainerKind::Supporter:
                        return effects_written(def) && rules::can_play_supporter(game, player) && requirements_met(game, player, card);
                    case TrainerKind::Stadium:
                        return rules::can_play_stadium(game, player, card);
                    default:
                        return false;
                }
            }

            void add_trainer_actions(const GameState& game, int player, std::vector<Action>& out) {
                const PlayerState& p = game.player(player);
                std::vector<Choice> seen;
                for (const CardIndex card : p.hand) {
                    const CardDef& def = game.def(card);
                    if (def.kind != CardKind::Trainer) {
                        continue;
                    }
                    if (def.trainer_kind == TrainerKind::Tool) {
                        for (const Slot slot : in_play_slots(p)) {
                            if (rules::can_attach_tool(game, pokemon_at(p, slot)) && first_time(seen, &def, slot)) {
                                out.push_back(Action{ActionKind::AttachTool, card, slot, 0});
                            }
                        }
                    } else if (can_play_trainer(game, player, card) && first_time(seen, &def, kActiveSlot)) {
                        out.push_back(Action{ActionKind::PlayTrainer, card, kActiveSlot, 0});
                    }
                }
            }

            void play_stadium(GameState& game, std::uint8_t player, CardIndex card) {
                for (int i = 0; i < kPlayers; ++i) {
                    PlayerState& owner = game.player(i);
                    if (owner.stadium != kNoCard) {
                        owner.discard.push_back(owner.stadium);
                        owner.stadium = kNoCard;
                    }
                }
                game.player(player).stadium = card;
                game.flags.stadium_played = true;
            }

            void play_trainer(GameState& game, std::uint8_t player, CardIndex card) {
                PlayerState& p = game.player(player);
                p.hand.remove(card);
                raise(game, Event{EventKind::TrainerPlayed, player, kNoCard, card});
                if (game.def(card).trainer_kind == TrainerKind::Stadium) {
                    play_stadium(game, player, card);
                    return;
                }
                if (game.def(card).trainer_kind == TrainerKind::Supporter) {
                    game.flags.supporter_played = true;
                }
                p.resolving.push_back(card);
                start_effect(game, EffectContext{player, kNoCard, card}, ProgramRef{card, ProgramKind::Trainer, 0}, AfterEffect::DiscardTrainer);
            }

        } // namespace

        void add_main_phase_actions(const GameState& game, std::vector<Action>& out) {
            const int player = game.current_player;
            add_play_basic_actions(game, player, out);
            add_evolve_actions(game, player, out);
            add_attach_actions(game, player, out);
            add_retreat_actions(game, player, out);
            add_trainer_actions(game, player, out);
            add_attack_actions(game, out);
            out.push_back(Action{ActionKind::EndTurn, kNoCard, kActiveSlot, 0});
        }

        void apply_main_phase_action(GameState& game, const Action& action) {
            const std::uint8_t player = game.current_player;
            PlayerState& p = game.player(player);
            switch (action.kind) {
                case ActionKind::PlayBasic:
                    p.hand.remove(action.card);
                    p.bench.push_back(new_pokemon(action.card, game.turn));
                    raise(game, Event{EventKind::PutOnBench, player, action.card, kNoCard});
                    break;
                case ActionKind::Evolve: {
                    p.hand.remove(action.card);
                    PokemonInPlay& pokemon = pokemon_at(p, action.target);
                    pokemon.stack.push_back(action.card);
                    pokemon.turn_played = game.turn;
                    raise(game, Event{EventKind::Evolved, player, pokemon.id(), action.card});
                    break;
                }
                case ActionKind::AttachEnergy: {
                    p.hand.remove(action.card);
                    PokemonInPlay& pokemon = pokemon_at(p, action.target);
                    pokemon.energy.push_back(action.card);
                    game.flags.energy_attached = true;
                    raise(game, Event{EventKind::EnergyAttached, player, pokemon.id(), action.card});
                    break;
                }
                case ActionKind::Retreat:
                    start_retreat(game, game.current_player, action.target);
                    break;
                case ActionKind::PlayTrainer:
                    play_trainer(game, player, action.card);
                    break;
                case ActionKind::AttachTool: {
                    p.hand.remove(action.card);
                    PokemonInPlay& pokemon = pokemon_at(p, action.target);
                    pokemon.tool = action.card;
                    raise(game, Event{EventKind::TrainerPlayed, player, pokemon.id(), action.card});
                    break;
                }
                case ActionKind::Attack:
                    apply_attack(game, action);
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
                const int player = decision.player;
                game.pending.pop_back();
                finish_retreat(game, player, target);
            }
        }
    } // namespace detail

} // namespace pokebot
