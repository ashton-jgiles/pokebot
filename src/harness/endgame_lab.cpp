#include "harness/endgame_lab.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <map>
#include <utility>

#include "agents/evaluation/evaluation.hpp"
#include "agents/greedy/greedy_agent.hpp"
#include "agents/planner/and_tree.hpp"
#include "agents/planner/plan.hpp"
#include "engine/engine.hpp"
#include "engine/observation/observation.hpp"
#include "harness/match.hpp"

namespace pokebot {

    namespace {

        constexpr int kMaxSteps = 64;

        std::vector<int> copies_by_card(const GameState& game, const std::vector<CardIndex>& cards) {
            std::map<const CardDef*, int> copies;
            for (const CardIndex card : cards) {
                ++copies[&game.def(card)];
            }
            std::vector<int> out;
            for (const auto& [def, n] : copies) {
                out.push_back(n);
            }
            return out;
        }

        std::vector<std::pair<CardIndex, int>> one_of_each(const GameState& game, int player) {
            std::map<const CardDef*, std::pair<CardIndex, int>> first;
            for (const CardIndex card : game.player(player).deck) {
                ++first.try_emplace(&game.def(card), card, 0).first->second.second;
            }
            std::vector<std::pair<CardIndex, int>> out;
            for (const auto& [def, card] : first) {
                out.push_back(card);
            }
            return out;
        }

        void put_on_top(GameState& game, int player, CardIndex card) {
            auto& deck = game.player(player).deck;
            const auto it = std::find(deck.begin(), deck.end(), card);
            std::iter_swap(it, deck.end() - 1);
        }

        bool planning(const GameState& game) {
            const DecisionKind kind = game.decision().kind;
            return kind == DecisionKind::MainPhase || kind == DecisionKind::ExtraAttack;
        }

        void resolve(GameState& game, GreedyAgent& greedy, EndgameTree& tree) {
            for (int step = 0; step < kMaxSteps && !game.is_over() && !planning(game); ++step) {
                const std::vector<Action> actions = legal_actions(game);
                if (actions.size() == 1) {
                    apply_legal(game, actions.front());
                } else {
                    apply_legal(game, greedy.choose(observe(game, game.decision().player), actions));
                    ++tree.greedy_steps;
                }
            }
        }

        double leaf(const GameState& game, int me) {
            return game.is_over() ? utility(game, me) : evaluate(game, me);
        }

        std::vector<RankedPlan> top_plans(const GameState& game, int player, int budget, int keep, const EndgameLabOptions& options, std::uint64_t seed) {
            PlanScorer scorer(player, seed, options.samples);
            AndTreeOptions and_options;
            and_options.budget = budget;
            and_options.keep = keep;
            return and_tree_search(game, player, scorer, and_options).plans;
        }

        EndgameTree measure_tree(const GameState& game, const EndgameLabOptions& options, std::uint64_t seed) {
            const auto start = std::chrono::steady_clock::now();
            EndgameTree tree;
            const int me = game.decision().player;
            const int opponent = opponent_of(me);
            GreedyAgent greedy(seed, 1);
            const auto draws = one_of_each(game, opponent);
            const double deck = static_cast<double>(game.player(opponent).deck.size());
            tree.value = -1.0;
            for (const RankedPlan& ranked : top_plans(game, me, options.and_budget, options.plans, options, seed)) {
                ++tree.plans;
                GameState once = game;
                if (!apply_plan_checked(once, ranked.plan)) {
                    ++tree.unplayable;
                    continue;
                }
                if (!(once.rng == game.rng)) {
                    ++tree.random;
                }
                resolve(once, greedy, tree);
                if (once.is_over() || once.decision().player == me) {
                    ++tree.leaves;
                    tree.value = std::max(tree.value, leaf(once, me));
                    if (!once.is_over()) {
                        ++tree.mid_turn;
                        const auto drawn = static_cast<long long>(game.player(me).deck.size()) - static_cast<long long>(once.player(me).deck.size());
                        ++tree.mid_turn_draws[drawn <= 1 ? 0 : (drawn <= 3 ? 1 : 2)];
                    }
                    continue;
                }
                double expected = 0.0;
                const std::size_t outcomes = std::max<std::size_t>(draws.size(), 1);
                for (std::size_t d = 0; d < outcomes; ++d) {
                    ++tree.chance;
                    GameState state = game;
                    double weight = 1.0;
                    if (!draws.empty()) {
                        put_on_top(state, opponent, draws[d].first);
                        weight = static_cast<double>(draws[d].second) / deck;
                    }
                    apply_plan_checked(state, ranked.plan);
                    resolve(state, greedy, tree);
                    if (state.is_over() || state.decision().player != opponent) {
                        ++tree.leaves;
                        expected += weight * leaf(state, me);
                        continue;
                    }
                    ++tree.opponent_searches;
                    double worst = 1.0;
                    for (const RankedPlan& reply : top_plans(state, opponent, options.opponent_budget, options.opponent_plans, options, seed + d + 1)) {
                        ++tree.opponent_plans;
                        GameState after = state;
                        apply_plan_checked(after, reply.plan);
                        ++tree.leaves;
                        worst = std::min(worst, leaf(after, me));
                    }
                    expected += weight * worst;
                }
                tree.value = std::max(tree.value, expected);
            }
            tree.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            return tree;
        }

        EndgamePosition measure(const GameState& game, const EndgameLabOptions& options) {
            const int me = game.decision().player;
            const int opponent = opponent_of(me);
            const Observation observation = observe(game, me);
            EndgamePosition position;
            position.min_prizes = static_cast<int>(std::min(game.player(0).prizes.size(), game.player(1).prizes.size()));
            const PlayerState& mine = game.player(me);
            std::vector<CardIndex> own(mine.deck.begin(), mine.deck.end());
            own.insert(own.end(), mine.prizes.begin(), mine.prizes.end());
            position.own_worlds = count_placements(copies_by_card(game, own), static_cast<int>(mine.prizes.size()));
            const PlayerState& theirs = game.player(opponent);
            const auto& known = observation.opponent().hand;
            std::vector<CardIndex> pool;
            for (const CardIndex card : theirs.hand) {
                if (std::find(known.begin(), known.end(), card) == known.end()) {
                    pool.push_back(card);
                }
            }
            pool.insert(pool.end(), theirs.deck.begin(), theirs.deck.end());
            pool.insert(pool.end(), theirs.prizes.begin(), theirs.prizes.end());
            position.hidden_hand = static_cast<int>(theirs.hand.size() - known.size());
            position.opponent_worlds = count_placements(copies_by_card(game, pool), position.hidden_hand, static_cast<int>(theirs.prizes.size()));
            position.own_deck_cards = static_cast<int>(one_of_each(game, me).size());
            position.opponent_deck_cards = static_cast<int>(one_of_each(game, opponent).size());
            if (options.model != nullptr) {
                const std::vector<double> belief = options.model->belief(observation);
                position.lists = static_cast<int>(std::count_if(belief.begin(), belief.end(), [&](double p) { return p >= options.list_threshold; }));
            }
            return position;
        }

        double median(std::vector<double> values, double at = 0.5) {
            if (values.empty()) {
                return 0.0;
            }
            std::sort(values.begin(), values.end());
            return values[static_cast<std::size_t>(at * static_cast<double>(values.size() - 1))];
        }

        void add(EndgameTree& total, const EndgameTree& more) {
            total.plans += more.plans;
            total.unplayable += more.unplayable;
            total.random += more.random;
            total.mid_turn += more.mid_turn;
            for (std::size_t i = 0; i < total.mid_turn_draws.size(); ++i) {
                total.mid_turn_draws[i] += more.mid_turn_draws[i];
            }
            total.chance += more.chance;
            total.opponent_searches += more.opponent_searches;
            total.opponent_plans += more.opponent_plans;
            total.leaves += more.leaves;
            total.greedy_steps += more.greedy_steps;
            total.seconds += more.seconds;
        }

    } // namespace

    double count_placements(const std::vector<int>& copies, int first, int second) {
        if (first < 0 || second < 0) {
            return 0.0;
        }
        const auto rows = static_cast<std::size_t>(first) + 1;
        const auto cols = static_cast<std::size_t>(second) + 1;
        std::vector<double> ways(rows * cols, 0.0);
        ways[0] = 1.0;
        for (const int n : copies) {
            std::vector<double> next(ways.size(), 0.0);
            for (std::size_t a = 0; a < rows; ++a) {
                for (std::size_t b = 0; b < cols; ++b) {
                    const double here = ways[a * cols + b];
                    if (here == 0.0) {
                        continue;
                    }
                    for (std::size_t x = 0; x <= static_cast<std::size_t>(n) && a + x < rows; ++x) {
                        for (std::size_t y = 0; x + y <= static_cast<std::size_t>(n) && b + y < cols; ++y) {
                            next[(a + x) * cols + b + y] += here;
                        }
                    }
                }
            }
            ways = std::move(next);
        }
        return ways.back();
    }

    EndgameLabResult run_endgame_lab(const CardDB& db, const std::vector<ArenaDeck>& decks, const EndgameLabOptions& options) {
        EndgameLabResult result;
        const auto start = std::chrono::steady_clock::now();
        const std::size_t n = decks.size();
        long long qualifying = 0;
        for (int game = 0; game < options.games; ++game) {
            const auto index = static_cast<std::size_t>(game);
            const std::uint64_t seed = options.seed + static_cast<std::uint64_t>(game);
            const CardTable table(db, decks[index % n].list, decks[(index / n) % n].list);
            GreedyAgent first(seed * 2);
            GreedyAgent second(seed * 2 + 1);
            MatchOptions match;
            match.check_invariants = false;
            int lowest = 6;
            match.on_action = [&](const GameState& before, const Action&, const GameState&) {
                if (before.turn == 0 || before.decision().kind != DecisionKind::MainPhase) {
                    return;
                }
                EndgamePosition position = measure(before, options);
                lowest = std::min(lowest, position.min_prizes);
                if (position.min_prizes <= options.tree_prizes && ++qualifying % options.every == 0) {
                    position.tree = measure_tree(before, options, seed ^ static_cast<std::uint64_t>(qualifying));
                    position.measured = true;
                }
                result.positions.push_back(position);
            };
            play_game(table, seed, first, second, match);
            ++result.lowest_prizes[static_cast<std::size_t>(lowest)];
            ++result.games;
            if (options.on_progress) {
                options.on_progress(result.games, options.games);
            }
        }
        result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        return result;
    }

    EndgameSummary summarize(const EndgameLabResult& result, int min_prizes) {
        EndgameSummary summary;
        std::vector<double> own;
        std::vector<double> opponent;
        std::vector<double> total;
        std::vector<double> hidden;
        std::vector<double> own_deck;
        std::vector<double> opponent_deck;
        std::vector<double> lists;
        std::vector<double> seconds;
        for (const EndgamePosition& p : result.positions) {
            if (p.min_prizes != min_prizes) {
                continue;
            }
            ++summary.positions;
            own.push_back(std::log10(p.own_worlds));
            opponent.push_back(std::log10(p.opponent_worlds));
            total.push_back(own.back() + opponent.back());
            hidden.push_back(p.hidden_hand);
            own_deck.push_back(p.own_deck_cards);
            opponent_deck.push_back(p.opponent_deck_cards);
            lists.push_back(p.lists);
            if (p.measured) {
                ++summary.trees;
                add(summary.total, p.tree);
                seconds.push_back(p.tree.seconds);
            }
        }
        if (summary.positions == 0) {
            return summary;
        }
        const auto share = [](const std::vector<double>& values, double limit) {
            return 100.0 * static_cast<double>(std::count_if(values.begin(), values.end(), [&](double v) { return v <= limit + 1e-9; })) /
                   static_cast<double>(values.size());
        };
        summary.median_own = median(own);
        summary.median_opponent = median(opponent);
        summary.median_total = median(total);
        for (std::size_t i = 0; i < summary.total_at_most.size(); ++i) {
            summary.total_at_most[i] = share(total, 2.0 * static_cast<double>(i + 1));
        }
        summary.median_hidden_hand = median(hidden);
        summary.hidden_hand_at_most_2 = share(hidden, 2.0);
        summary.median_own_deck_cards = median(own_deck);
        summary.median_opponent_deck_cards = median(opponent_deck);
        summary.median_lists = median(lists);
        summary.median_seconds = median(seconds);
        summary.p90_seconds = median(seconds, 0.9);
        return summary;
    }

} // namespace pokebot
