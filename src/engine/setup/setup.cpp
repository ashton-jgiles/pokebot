#include <algorithm>
#include <stdexcept>
#include <vector>

#include "engine/engine.hpp"
#include "engine/internal.hpp"

namespace pokebot {

    namespace {

        constexpr int kOpeningHand = 7;

        bool hand_has_basic(const GameState& game, int player) {
            for (const CardIndex card : game.player(player).hand) {
                if (detail::is_basic_pokemon(game, card)) {
                    return true;
                }
            }
            return false;
        }

        void deal_opening_hand(GameState& game, int player) {
            PlayerState& p = game.player(player);
            for (const CardIndex card : p.hand) {
                p.deck.push_back(card);
            }
            p.hand.clear();
            detail::shuffle_deck(game, player);
            detail::draw_cards(game, player, kOpeningHand);
        }

        void add_basic_choices(const GameState& game, int player, ActionKind kind, std::vector<Action>& out) {
            std::vector<const CardDef*> offered;
            for (const CardIndex card : game.player(player).hand) {
                if (!detail::is_basic_pokemon(game, card)) {
                    continue;
                }
                const CardDef* def = &game.def(card);
                if (std::find(offered.begin(), offered.end(), def) != offered.end()) {
                    continue;
                }
                offered.push_back(def);
                out.push_back(Action{kind, card, kActiveSlot, 0});
            }
        }

        void finish_setup(GameState& game) {
            for (int p = 0; p < kPlayers; ++p) {
                PlayerState& player = game.player(p);
                for (std::size_t i = 0; i < kPrizeCount; ++i) {
                    player.prizes.push_back(player.deck.pop_back());
                }
            }
            detail::begin_turn(game, game.first_player);
        }

        Decision decision_for(DecisionKind kind, int player, int amount = 0) {
            return Decision{kind, static_cast<std::uint8_t>(player), static_cast<std::uint8_t>(amount)};
        }

    } // namespace

    GameState new_game(const CardTable& cards, std::uint64_t seed) {
        GameState game;
        game.cards = &cards;
        game.rng = Rng(seed);

        for (int p = 0; p < kPlayers; ++p) {
            const std::size_t first = static_cast<std::size_t>(p) * kDeckSize;
            for (std::size_t i = 0; i < kDeckSize; ++i) {
                game.player(p).deck.push_back(static_cast<CardIndex>(first + i));
            }
            deal_opening_hand(game, p);
        }

        while (true) {
            const bool redraw0 = !hand_has_basic(game, 0);
            const bool redraw1 = !hand_has_basic(game, 1);
            if (!redraw0 && !redraw1) {
                break;
            }
            // A player who mulligans shows their hand to the opponent first.
            for (int p = 0; p < kPlayers; ++p) {
                if (p == 0 ? redraw0 : redraw1) {
                    for (const CardIndex card : game.player(p).hand) {
                        detail::show(game, opponent_of(p), card);
                    }
                }
            }
            if (redraw0) {
                deal_opening_hand(game, 0);
                ++game.player(0).mulligans;
            }
            if (redraw1) {
                deal_opening_hand(game, 1);
                ++game.player(1).mulligans;
            }
        }

        const int coin_winner = game.rng.coin_flip() ? 0 : 1;

        // A player may draw 1 card for each mulligan the opponent took beyond their own (rulebook p. 18).
        for (int p = 0; p < kPlayers; ++p) {
            const int bonus = game.player(opponent_of(p)).mulligans - game.player(p).mulligans;
            if (bonus > 0) {
                game.pending.push_back(decision_for(DecisionKind::MulliganDraws, p, bonus));
            }
        }

        for (int p = kPlayers - 1; p >= 0; --p) {
            game.pending.push_back(decision_for(DecisionKind::SetupBench, p));
            game.pending.push_back(decision_for(DecisionKind::SetupActive, p));
        }
        game.pending.push_back(decision_for(DecisionKind::ChooseTurnOrder, coin_winner));
        detail::note_visible(game);
        return game;
    }

    namespace detail {

        void add_setup_actions(const GameState& game, const Decision& decision, std::vector<Action>& out) {
            const int player = decision.player;
            switch (decision.kind) {
                case DecisionKind::ChooseTurnOrder:
                    out.push_back(Action{ActionKind::ChooseTurnOrder, kNoCard, kActiveSlot, 0});
                    out.push_back(Action{ActionKind::ChooseTurnOrder, kNoCard, kActiveSlot, 1});
                    break;
                case DecisionKind::SetupActive:
                    add_basic_choices(game, player, ActionKind::SetupPlaceActive, out);
                    break;
                case DecisionKind::SetupBench:
                    if (game.player(player).bench.size() < rules::bench_size(game, player)) {
                        add_basic_choices(game, player, ActionKind::SetupPlaceBench, out);
                    }
                    out.push_back(Action{ActionKind::SetupDone, kNoCard, kActiveSlot, 0});
                    break;
                case DecisionKind::MulliganDraws: {
                    const std::size_t most = std::min<std::size_t>(decision.amount, game.player(player).deck.size());
                    for (std::size_t n = 0; n <= most; ++n) {
                        out.push_back(Action{ActionKind::DrawMulliganCards, kNoCard, kActiveSlot, static_cast<std::uint8_t>(n)});
                    }
                    break;
                }
                default:
                    throw std::logic_error("add_setup_actions: not a setup decision");
            }
        }

        void apply_setup_action(GameState& game, const Action& action) {
            const Decision decision = game.decision();
            PlayerState& player = game.player(decision.player);

            switch (action.kind) {
                case ActionKind::ChooseTurnOrder:
                    game.pending.pop_back();
                    game.first_player = static_cast<std::uint8_t>(action.amount == 0 ? decision.player : opponent_of(decision.player));
                    break;
                case ActionKind::SetupPlaceActive:
                    game.pending.pop_back();
                    player.hand.remove(action.card);
                    player.active = new_pokemon(action.card, 0);
                    break;
                case ActionKind::SetupPlaceBench:
                    player.hand.remove(action.card);
                    player.bench.push_back(new_pokemon(action.card, 0));
                    break;
                case ActionKind::SetupDone:
                    game.pending.pop_back();
                    break;
                case ActionKind::DrawMulliganCards:
                    game.pending.pop_back();
                    draw_cards(game, decision.player, action.amount);
                    if (action.amount > 0 && player.bench.size() < rules::bench_size(game, decision.player) && hand_has_basic(game, decision.player)) {
                        game.pending.push_back(decision_for(DecisionKind::SetupBench, decision.player));
                    }
                    break;
                default:
                    throw std::logic_error("apply_setup_action: not a setup action");
            }

            if (game.pending.empty()) {
                finish_setup(game);
            }
        }
    } // namespace detail

} // namespace pokebot
