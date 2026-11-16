#include "engine/engine.hpp"

#include <algorithm>
#include <stdexcept>

#include "engine/internal.hpp"

namespace pokebot {

    namespace detail {

        bool is_basic_pokemon(const GameState& game, CardIndex card) {
            const CardDef& def = game.def(card);
            return def.kind == CardKind::Pokemon && def.stage == Stage::Basic;
        }

        int draw_cards(GameState& game, int player, int count) {
            PlayerState& p = game.player(player);
            int drawn = 0;
            while (drawn < count && !p.deck.empty()) {
                p.hand.push_back(p.deck.pop_back());
                ++drawn;
            }
            return drawn;
        }

        void set_winner(GameState& game, int player) {
            game.result = player == 0 ? GameResult::Player0Wins : GameResult::Player1Wins;
            game.pending.clear();
        }

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
    } // namespace detail

    std::vector<Action> legal_actions(const GameState& game) {
        std::vector<Action> actions;
        if (game.is_over()) {
            return actions;
        }

        const Decision& decision = game.decision();
        switch (decision.kind) {
            case DecisionKind::ChooseTurnOrder:
            case DecisionKind::SetupActive:
            case DecisionKind::SetupBench:
            case DecisionKind::MulliganDraws:
                detail::add_setup_actions(game, decision, actions);
                break;
            default:
                throw std::logic_error("this decision isn't implemented yet (steps 2.3-2.5)");
        }
        return actions;
    }

    void apply(GameState& game, const Action& action) {
        if (game.is_over()) {
            throw std::logic_error("apply: the game is already over");
        }

        const auto legal = legal_actions(game);
        if (std::find(legal.begin(), legal.end(), action) == legal.end()) {
            throw std::invalid_argument("apply: that action isn't legal right now");
        }

        switch (game.decision().kind) {
            case DecisionKind::ChooseTurnOrder:
            case DecisionKind::SetupActive:
            case DecisionKind::SetupBench:
            case DecisionKind::MulliganDraws:
                detail::apply_setup_action(game, action);
                break;
            default:
                throw std::logic_error("this decision isn't implemented yet (steps 2.3-2.5)");
        }
    }

} // namespace pokebot
