#include "engine/internal.hpp"

namespace pokebot {

    namespace detail {

        namespace {

            // will handle later
            void pokemon_checkup(GameState& /*game*/) {
            }

        } // namespace

        void begin_turn(GameState& game, int player) {
            game.current_player = static_cast<std::uint8_t>(player);
            ++game.turn;
            game.flags = TurnFlags{};

            if (game.player(player).deck.empty()) {
                set_winner(game, opponent_of(player));
                return;
            }
            draw_cards(game, player, 1);
            game.pending.push_back(Decision{DecisionKind::MainPhase, static_cast<std::uint8_t>(player), 0});
        }

        void end_turn(GameState& game) {
            game.pending.pop_back();
            pokemon_checkup(game);
            if (game.is_over()) {
                return;
            }
            begin_turn(game, opponent_of(game.current_player));
        }

    } // namespace detail

} // namespace pokebot