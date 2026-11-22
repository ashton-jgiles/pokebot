#include "harness/planner_lab.hpp"

#include <chrono>
#include <cstddef>

#include "agents/planner/and_tree.hpp"
#include "agents/greedy/greedy_agent.hpp"
#include "agents/planner/or_tree.hpp"
#include "agents/planner/plan.hpp"
#include "engine/engine.hpp"
#include "harness/match.hpp"

namespace pokebot {

    namespace {

        bool wins(const GameState& game, int player, const Plan& plan) {
            const GameState after = apply_plan(game, plan);
            return after.result == (player == 0 ? GameResult::Player0Wins : GameResult::Player1Wins);
        }

        void measure(const GameState& game, const PlannerLabOptions& options, std::uint64_t seed, PlannerLabResult& result) {
            const int player = game.decision().player;
            ++result.positions;

            OrTreeOptions planner_budget;
            planner_budget.budget = options.or_budget;
            const OrTreeResult quick = or_tree_search(game, player, Goal{GoalKind::Win, 1}, planner_budget);
            if (quick.found && !wins(game, player, quick.plan)) {
                ++result.false_wins;
            }

            const auto and_run = [&](int i) {
                AndTreeOptions and_options;
                and_options.budget = options.limit;
                and_options.closed_set = i < 2;
                and_options.bound = i % 2 == 0;
                PlanScorer scorer(player, seed, options.samples);
                return and_tree_search(game, player, scorer, and_options);
            };

            OrTreeOptions closed;
            closed.budget = options.limit;
            const OrTreeResult full = or_tree_search(game, player, Goal{GoalKind::Win, 1}, closed);
            if (full.out_of_budget) {
                return;
            }
            AndTreeResult and_runs[4];
            and_runs[0] = and_run(0);
            if (and_runs[0].out_of_budget) {
                return;
            }
            OrTreeOptions open = closed;
            open.closed_set = false;
            const OrTreeResult unpruned = or_tree_search(game, player, Goal{GoalKind::Win, 1}, open);
            if (unpruned.out_of_budget) {
                return;
            }
            for (int i = 1; i < 4; ++i) {
                and_runs[i] = and_run(i);
                if (and_runs[i].out_of_budget) {
                    return;
                }
            }

            ++result.finished;
            if (full.found) {
                ++result.wins;
                result.win_nodes += full.expanded;
                if (full.expanded > result.win_nodes_max) {
                    result.win_nodes_max = full.expanded;
                }
                if (quick.found) {
                    ++result.wins_found;
                }
            }
            result.or_closed += full.expanded;
            result.or_open += unpruned.expanded;
            result.and_both += and_runs[0].expanded;
            result.and_closed += and_runs[1].expanded;
            result.and_bound += and_runs[2].expanded;
            result.and_neither += and_runs[3].expanded;
            result.pruned += and_runs[0].pruned;
            if (!and_runs[0].plans.empty() && !and_runs[1].plans.empty() && and_runs[0].plans.front().value != and_runs[1].plans.front().value) {
                ++result.bound_changed;
            }
        }

    } // namespace

    PlannerLabResult run_planner_lab(const CardDB& db, const std::vector<ArenaDeck>& decks, const PlannerLabOptions& options) {
        PlannerLabResult result;
        const auto start = std::chrono::steady_clock::now();
        const std::size_t n = decks.size();
        long long main_phases = 0;
        for (int game = 0; game < options.games; ++game) {
            const auto index = static_cast<std::size_t>(game);
            const std::uint64_t seed = options.seed + static_cast<std::uint64_t>(game);
            const CardTable table(db, decks[index % n].list, decks[(index / n) % n].list);
            GreedyAgent first(seed * 2);
            GreedyAgent second(seed * 2 + 1);
            MatchOptions match;
            match.check_invariants = false;
            match.on_action = [&](const GameState& before, const Action&, const GameState&) {
                if (before.turn == 0 || before.decision().kind != DecisionKind::MainPhase) {
                    return;
                }
                if (++main_phases % options.every == 0) {
                    measure(before, options, seed ^ static_cast<std::uint64_t>(main_phases), result);
                }
            };
            play_game(table, seed, first, second, match);
            ++result.games;
            if (options.on_progress) {
                options.on_progress(result.games, options.games);
            }
        }
        result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        return result;
    }

} // namespace pokebot
