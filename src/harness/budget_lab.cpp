#include "harness/budget_lab.hpp"

#include <chrono>
#include <cstddef>

#include "agents/greedy/greedy_agent.hpp"
#include "agents/planner/and_tree.hpp"
#include "engine/engine.hpp"
#include "harness/match.hpp"

namespace pokebot {

    namespace {

        AndTreeResult search(const GameState& game, int player, int budget, const BudgetLabOptions& options, std::uint64_t seed, BudgetRow& row) {
            PlanScorer scorer(player, seed, options.samples);
            AndTreeOptions and_options;
            and_options.budget = budget;
            and_options.keep = options.plans;
            const auto start = std::chrono::steady_clock::now();
            AndTreeResult result = and_tree_search(game, player, scorer, and_options);
            row.seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            ++row.searches;
            row.plans += static_cast<long long>(result.plans.size());
            return result;
        }

        void measure(const GameState& game, const BudgetLabOptions& options, std::uint64_t seed, BudgetLabResult& result) {
            const int player = game.decision().player;
            ++result.positions;
            const AndTreeResult reference = search(game, player, options.reference, options, seed, result.reference);
            if (reference.plans.empty()) {
                return;
            }
            const double best = reference.plans.front().value;
            ++result.reference.same_best;
            for (std::size_t i = 0; i < options.budgets.size(); ++i) {
                BudgetRow& row = result.rows[i];
                const AndTreeResult cheap = search(game, player, options.budgets[i], options, seed, row);
                if (cheap.plans.empty()) {
                    row.gap += best + 1.0;
                    continue;
                }
                const double value = cheap.plans.front().value;
                row.gap += best - value;
                if (value >= best - 1e-9) {
                    ++row.same_best;
                }
            }
        }

    } // namespace

    BudgetLabResult run_budget_lab(const CardDB& db, const std::vector<ArenaDeck>& decks, const BudgetLabOptions& options) {
        BudgetLabResult result;
        result.reference.budget = options.reference;
        for (const int budget : options.budgets) {
            BudgetRow row;
            row.budget = budget;
            result.rows.push_back(row);
        }
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
