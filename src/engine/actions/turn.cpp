#include "engine/internal.hpp"

namespace pokebot {

    namespace detail {

        namespace {

            // "If this card is attached to 1 of your Pokemon, discard it at the end of your turn."
            void discard_end_of_turn_energy(GameState& game) {
                PlayerState& p = game.player(game.current_player);
                const auto sweep = [&game](PokemonInPlay& pokemon) {
                    for (std::size_t i = pokemon.energy.size(); i-- > 0;) {
                        const CardIndex card = pokemon.energy[i];
                        if (game.def(card).discard_at_end_of_turn) {
                            pokemon.energy.erase_at(i);
                            game.player(owner_of(card)).discard.push_back(card);
                        }
                    }
                };
                if (p.active) {
                    sweep(*p.active);
                }
                for (PokemonInPlay& pokemon : p.bench) {
                    sweep(pokemon);
                }
            }

            void pokemon_checkup(GameState& game) {
                raise(game, Event{EventKind::Checkup, game.current_player, kNoCard, kNoCard});
                special_conditions_checkup(game);
            }

        } // namespace

        void begin_turn(GameState& game, int player) {
            game.current_player = static_cast<std::uint8_t>(player);
            ++game.turn;
            game.flags = TurnFlags{};
            game.history.start_turn(game.current_player);
            expire_lasting(game);
            game.phase = TurnPhase::Main;

            if (game.player(player).deck.empty()) {
                set_winner(game, opponent_of(player));
                return;
            }
            draw_cards(game, player, 1);
            game.pending.push_back(Decision{DecisionKind::MainPhase, static_cast<std::uint8_t>(player), 0, 0});
        }

        void end_turn(GameState& game) {
            game.pending.pop_back();
            game.phase = TurnPhase::AfterMain;
            continue_game(game);
        }

        void continue_game(GameState& game) {
            while (!game.is_over() && game.pending.empty()) {
                switch (game.phase) {
                    case TurnPhase::AfterMain:
                        discard_end_of_turn_energy(game);
                        raise(game, Event{EventKind::TurnEnding, game.current_player, kNoCard, kNoCard});
                        game.phase = TurnPhase::Checkup;
                        pokemon_checkup(game);
                        handle_knockouts(game);
                        break;
                    case TurnPhase::Checkup:
                        begin_turn(game, opponent_of(game.current_player));
                        break;
                    default:
                        throw std::logic_error("continue_game: no decision is pending, but the turn isn't over");
                }
            }
        }

    } // namespace detail

} // namespace pokebot