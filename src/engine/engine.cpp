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

        bool is_first_turn(const GameState& game) {
            return game.turn <= 2;
        }

        PokemonInPlay new_pokemon(CardIndex card, std::uint16_t turn) {
            PokemonInPlay pokemon;
            pokemon.stack.push_back(card);
            pokemon.turn_played = turn;
            return pokemon;
        }

        PokemonInPlay& pokemon_at(PlayerState& player, Slot slot) {
            if (slot == kActiveSlot) {
                return *player.active;
            }
            return player.bench[static_cast<std::size_t>(slot) - 1];
        }

        const PokemonInPlay& pokemon_at(const PlayerState& player, Slot slot) {
            if (slot == kActiveSlot) {
                return *player.active;
            }
            return player.bench[static_cast<std::size_t>(slot) - 1];
        }

        FixedVector<Slot, kBenchSize + 1> in_play_slots(const PlayerState& player) {
            FixedVector<Slot, kBenchSize + 1> slots;
            if (player.active) {
                slots.push_back(kActiveSlot);
            }
            for (std::size_t i = 0; i < player.bench.size(); ++i) {
                slots.push_back(bench_slot(i));
            }
            return slots;
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
        } else if (kind == DecisionKind::RetreatDiscard) {
            detail::add_retreat_discard_actions(game, actions);
        } else if (kind == DecisionKind::PromoteActive) {
            detail::add_promote_actions(game, actions);
        } else {
            throw std::logic_error("unknown decision");
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
        } else if (kind == DecisionKind::RetreatDiscard) {
            detail::apply_retreat_discard(game, action);
        } else if (kind == DecisionKind::PromoteActive) {
            detail::apply_promote(game, action);
        } else {
            throw std::logic_error("unknown decision");
        }
    }

} // namespace pokebot
