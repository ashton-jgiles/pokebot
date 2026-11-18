#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

#include "engine/internal.hpp"

// One-of-a-kind cards whose rules don't fit the effect language. Each is a `custom` step (and, if it
// needs one, a `custom` requirement) that the interpreter hands to the functions here.
namespace pokebot {

    namespace detail {

        namespace {

            using Pair = std::pair<CardIndex, Slot>; // a card and the Pokemon in play it goes with

            bool unevolved(const PokemonInPlay& pokemon) {
                return pokemon.stack.size() == 1;
            }

            // Rare Candy: a Stage 2 in hand whose Stage 1 evolves from this Basic. The Stage 1's name is looked
            // up among every card in the game, since only its "evolves from" is needed.
            bool candy_evolves(const GameState& game, const PokemonInPlay& basic, CardIndex stage2) {
                const CardDef& top = game.def(stage2);
                if (top.kind != CardKind::Pokemon || top.stage != Stage::Stage2) {
                    return false;
                }
                const std::string& basic_name = game.def(basic.top()).name;
                for (std::size_t i = 0; i < kMaxCards; ++i) {
                    const CardDef& def = game.def(static_cast<CardIndex>(i));
                    if (def.stage == Stage::Stage1 && def.name == top.evolves_from && def.evolves_from == basic_name) {
                        return true;
                    }
                }
                return false;
            }

            std::vector<Pair> candy_pairs(const GameState& game, int player, bool first_only) {
                std::vector<Pair> pairs;
                std::vector<std::pair<const CardDef*, Slot>> seen;
                const PlayerState& p = game.player(player);
                for (const Slot slot : in_play_slots(p)) {
                    const PokemonInPlay& pokemon = pokemon_at(p, slot);
                    if (!unevolved(pokemon) || pokemon.turn_played == game.turn) {
                        continue;
                    }
                    for (const CardIndex card : p.hand) {
                        const auto key = std::make_pair(&game.def(card), slot);
                        if (candy_evolves(game, pokemon, card) && std::find(seen.begin(), seen.end(), key) == seen.end()) {
                            seen.push_back(key);
                            pairs.emplace_back(card, slot);
                            if (first_only) {
                                return pairs;
                            }
                        }
                    }
                }
                return pairs;
            }

            // Transformation Tome: a Basic Pokemon in the discard pile and one of your unevolved Basic Pokemon in play.
            std::vector<Pair> tome_pairs(const GameState& game, int player, bool first_only) {
                std::vector<Pair> pairs;
                std::vector<std::pair<const CardDef*, Slot>> seen;
                const PlayerState& p = game.player(player);
                for (const CardIndex card : p.discard) {
                    if (!is_basic_pokemon(game, card)) {
                        continue;
                    }
                    for (const Slot slot : in_play_slots(p)) {
                        const auto key = std::make_pair(&game.def(card), slot);
                        if (unevolved(pokemon_at(p, slot)) && std::find(seen.begin(), seen.end(), key) == seen.end()) {
                            seen.push_back(key);
                            pairs.emplace_back(card, slot);
                            if (first_only) {
                                return pairs;
                            }
                        }
                    }
                }
                return pairs;
            }

            std::vector<Pair> pairs_for(const GameState& game, int player, CustomEffect effect, bool first_only = false) {
                switch (effect) {
                    case CustomEffect::RareCandy:
                        return candy_pairs(game, player, first_only);
                    case CustomEffect::TransformationTome:
                        return tome_pairs(game, player, first_only);
                }
                return {};
            }

            void rare_candy(GameState& game, const EffectRun& run, CardIndex stage2, Slot slot) {
                PlayerState& p = game.player(run.who.player);
                PokemonInPlay& pokemon = pokemon_at(p, slot);
                p.hand.remove(stage2);
                pokemon.stack.push_back(stage2);
                pokemon.turn_played = game.turn;
                clear_special_conditions(pokemon);
                end_effects_on(game, pokemon.id());
                raise(game, Event{EventKind::Evolved, run.who.player, pokemon.id(), stage2});
            }

            // "Any attached cards, damage counters, Special Conditions, turns in play, and any other effects remain on the new Pokemon."
            void transformation_tome(GameState& game, const EffectRun& run, CardIndex incoming, Slot slot) {
                PlayerState& p = game.player(run.who.player);
                PokemonInPlay& pokemon = pokemon_at(p, slot);
                const CardIndex outgoing = pokemon.stack[0];
                p.discard.remove(incoming);
                pokemon.stack[0] = incoming;
                p.discard.push_back(outgoing);
                for (LastingEffect& effect : game.lasting) {
                    if (effect.pokemon == outgoing) {
                        effect.pokemon = incoming;
                    }
                }
            }

            // "You must play 2 Transformation Tome cards at once": the second one goes to the discard pile with it.
            void discard_second_tome(GameState& game, const EffectRun& run) {
                PlayerState& p = game.player(run.who.player);
                const std::string& name = game.def(run.who.source).name;
                for (const CardIndex card : p.hand) {
                    if (card != run.who.source && game.def(card).name == name) {
                        p.hand.remove(card);
                        p.discard.push_back(card);
                        return;
                    }
                }
            }

        } // namespace

        bool custom_condition(const GameState& game, int player, CustomEffect effect) {
            if (effect == CustomEffect::RareCandy && rules::is_first_turn(game)) {
                return false;
            }
            return !pairs_for(game, player, effect, true).empty();
        }

        bool run_custom(GameState& game, EffectRun& run, const Step& step) {
            if (step.custom == CustomEffect::TransformationTome && run.value == 0) {
                discard_second_tome(game, run);
                run.value = 1; // once, even if the step is asked again after its choice
            }
            if (pairs_for(game, run.who.player, step.custom, true).empty()) {
                return true;
            }
            game.pending.push_back(Decision{DecisionKind::CustomChoice, run.who.player, 1, 1});
            return false;
        }

        void add_custom_actions(const GameState& game, const EffectRun& run, const Step& step, std::vector<Action>& out) {
            for (const auto& [card, slot] : pairs_for(game, run.who.player, step.custom)) {
                out.push_back(Action{ActionKind::ChooseCard, card, slot, 0});
            }
        }

        void apply_custom(GameState& game, const EffectRun& run, const Step& step, const Action& action) {
            switch (step.custom) {
                case CustomEffect::RareCandy:
                    rare_candy(game, run, action.card, action.target);
                    break;
                case CustomEffect::TransformationTome:
                    transformation_tome(game, run, action.card, action.target);
                    break;
            }
        }

    } // namespace detail

} // namespace pokebot
