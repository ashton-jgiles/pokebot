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

    } // namespace detail

    namespace {

        bool is_setup_decision(DecisionKind kind) {
            return kind == DecisionKind::ChooseTurnOrder || kind == DecisionKind::SetupActive ||
                   kind == DecisionKind::SetupBench || kind == DecisionKind::MulliganDraws;
        }

    } // namespace

    std::vector<Action> legal_actions(const GameState& game) {
        std::vector<Action> actions;
        if (game.is_over()) {
            return actions;
        }

        const DecisionKind kind = game.decision().kind;
        if (is_setup_decision(kind)) {
            detail::add_setup_actions(game, game.decision(), actions);
        } else if (kind == DecisionKind::MainPhase) {
            detail::add_main_phase_actions(game, actions);
        } else {
            throw std::logic_error("this decision isn't implemented yet (steps 2.4-2.5)");
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

        const DecisionKind kind = game.decision().kind;
        if (is_setup_decision(kind)) {
            detail::apply_setup_action(game, action);
        } else if (kind == DecisionKind::MainPhase) {
            detail::apply_main_phase_action(game, action);
        } else {
            throw std::logic_error("this decision isn't implemented yet (steps 2.4-2.5)");
        }
    }

} // namespace pokebot
