#include "engine/internal.hpp"

namespace pokebot {

    namespace detail {

        namespace {

            void pokemon_checkup(GameState& game) {
                raise(game, Event{EventKind::Checkup, game.current_player, kNoCard, kNoCard});
            }

        } // namespace

        void begin_turn(GameState& game, int player) {
            game.current_player = static_cast<std::uint8_t>(player);
            ++game.turn;
            game.flags = TurnFlags{};
            game.history.start_turn(game.current_player);
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