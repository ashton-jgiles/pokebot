#include "agents/endgame/expectiminimax.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include "agents/planner/and_tree.hpp"
#include "agents/planner/state_hash.hpp"
#include "engine/engine.hpp"

namespace pokebot {

    namespace {

        constexpr double kLow = -1.0;
        constexpr double kHigh = 1.0;
        constexpr double kInfinity = 2.0;

        std::uint64_t mix(std::uint64_t z) {
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return z ^ (z >> 31);
        }

        std::uint64_t action_key(const Action& a) {
            return static_cast<std::uint64_t>(a.kind) | static_cast<std::uint64_t>(a.card) << 8 | static_cast<std::uint64_t>(a.target) << 16 |
                   static_cast<std::uint64_t>(a.amount) << 24;
        }

        bool planning(const GameState& game) {
            const DecisionKind kind = game.decision().kind;
            return kind == DecisionKind::MainPhase || kind == DecisionKind::ExtraAttack;
        }

        class Solver {
        public:
            Solver(int me, const ExpectiminimaxOptions& options, ExpectiminimaxStats& stats) : me_(me), options_(options), stats_(stats) {}

            bool aborted() const { return aborted_; }

            std::vector<Plan> moves(const GameState& game, bool root) {
                std::vector<Plan> out;
                if (!planning(game)) {
                    for (const Action& action : legal_actions(game)) {
                        out.push_back(Plan{{action}, ActionClass::Final});
                    }
                    return out;
                }
                const int actor = game.decision().player;
                PlanScorer scorer(actor, hash_state(game), options_.samples, options_.weights);
                AndTreeOptions and_options;
                and_options.budget = root ? options_.root_budget : options_.budget;
                and_options.keep = root ? options_.root_plans : options_.plans;
                ++stats_.and_trees;
                for (const RankedPlan& ranked : and_tree_search(game, actor, scorer, and_options).plans) {
                    out.push_back(ranked.plan);
                }
                return out;
            }

            double move_value(const GameState& game, const Plan& plan, int depth, double alpha, double beta) {
                const int next = planning(game) ? depth - 1 : depth;
                GameState state = game;
                for (std::size_t i = 0; i + 1 < plan.actions.size(); ++i) {
                    apply_legal(state, plan.actions[i]);
                }
                if (state.is_over()) {
                    ++stats_.leaves;
                    return utility(state, me_);
                }
                return chance(state, plan.actions.back(), next, alpha, beta);
            }

            double value(const GameState& game, int depth, double alpha, double beta) {
                if (aborted_ || (options_.max_nodes > 0 && visited(stats_) >= options_.max_nodes)) {
                    aborted_ = true;
                    return 0.0;
                }
                if (game.is_over()) {
                    ++stats_.leaves;
                    return utility(game, me_);
                }
                if (depth <= 0) {
                    ++stats_.leaves;
                    return evaluate(game, me_, options_.weights);
                }
                const std::vector<Plan> list = moves(game, false);
                if (list.empty()) {
                    ++stats_.leaves;
                    return evaluate(game, me_, options_.weights);
                }
                ++stats_.nodes;
                const bool maximizing = game.decision().player == me_;
                double best = maximizing ? -kInfinity : kInfinity;
                for (std::size_t i = 0; i < list.size() && !aborted_; ++i) {
                    const double v = move_value(game, list[i], depth, alpha, beta);
                    if (maximizing) {
                        best = std::max(best, v);
                        alpha = std::max(alpha, v);
                    } else {
                        best = std::min(best, v);
                        beta = std::min(beta, v);
                    }
                    if (options_.prune && alpha >= beta) {
                        stats_.alpha_beta_cuts += static_cast<long long>(list.size() - i - 1);
                        break;
                    }
                }
                return best;
            }

        private:
            double chance(const GameState& game, const Action& action, int depth, double alpha, double beta) {
                Rng rng(mix(hash_state(game) ^ mix(action_key(action))));
                std::vector<Outcome> outcomes = chance_outcomes(game, action, rng, options_.chance, &stats_.chance);
                if (outcomes.size() == 1) {
                    return value(outcomes.front().state, depth, alpha, beta);
                }
                ++stats_.chance_nodes;
                std::stable_sort(outcomes.begin(), outcomes.end(), [](const Outcome& a, const Outcome& b) { return a.probability > b.probability; });
                double sum = 0.0;
                double left = 1.0;
                for (std::size_t i = 0; i < outcomes.size() && !aborted_; ++i) {
                    const double p = outcomes[i].probability;
                    left = std::max(0.0, left - p);
                    double low = -kInfinity;
                    double high = kInfinity;
                    if (options_.prune) {
                        low = (alpha - sum - kHigh * left) / p;
                        high = (beta - sum - kLow * left) / p;
                    }
                    sum += p * value(outcomes[i].state, depth, low, high);
                    if (options_.prune && i + 1 < outcomes.size()) {
                        if (sum + kHigh * left <= alpha) {
                            stats_.star1_cuts += static_cast<long long>(outcomes.size() - i - 1);
                            return sum + kHigh * left;
                        }
                        if (sum + kLow * left >= beta) {
                            stats_.star1_cuts += static_cast<long long>(outcomes.size() - i - 1);
                            return sum + kLow * left;
                        }
                    }
                }
                return sum;
            }

            int me_;
            const ExpectiminimaxOptions& options_;
            ExpectiminimaxStats& stats_;
            bool aborted_ = false;
        };

    } // namespace

    long long visited(const ExpectiminimaxStats& stats) {
        return stats.nodes + stats.chance_nodes + stats.leaves;
    }

    std::vector<Plan> root_moves(const GameState& game, const ExpectiminimaxOptions& options) {
        if (game.is_over()) {
            return {};
        }
        ExpectiminimaxStats stats;
        return Solver(game.decision().player, options, stats).moves(game, true);
    }

    SolveResult solve_worlds(const std::vector<const GameState*>& worlds, const std::vector<Plan>& plans, const ExpectiminimaxOptions& options) {
        const auto start = std::chrono::steady_clock::now();
        SolveResult result;
        if (worlds.empty() || worlds.front()->is_over()) {
            return result;
        }
        const int me = worlds.front()->decision().player;
        Solver solver(me, options, result.stats);
        const double p = 1.0 / static_cast<double>(worlds.size());
        double best = -kInfinity;
        for (const Plan& plan : plans) {
            const double alpha = options.prune ? best : -kInfinity;
            double sum = 0.0;
            double left = 1.0;
            for (std::size_t w = 0; w < worlds.size(); ++w) {
                left = std::max(0.0, left - p);
                const double low = options.prune ? (alpha - sum - kHigh * left) / p : -kInfinity;
                sum += p * solver.move_value(*worlds[w], plan, options.depth, low, kInfinity);
                if (solver.aborted()) {
                    break;
                }
                if (options.prune && w + 1 < worlds.size() && sum + kHigh * left <= alpha) {
                    result.stats.star1_cuts += static_cast<long long>(worlds.size() - w - 1);
                    sum += kHigh * left;
                    break;
                }
            }
            if (solver.aborted()) {
                result.complete = false;
                break;
            }
            result.plans.push_back(SolvedPlan{plan, sum, !options.prune || sum > best});
            if (sum > best) {
                best = sum;
                result.best = result.plans.size() - 1;
            }
        }
        result.value = result.plans.empty() ? evaluate(*worlds.front(), me, options.weights) : best;
        result.stats.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        return result;
    }

    SolveResult expectiminimax(const GameState& game, const ExpectiminimaxOptions& options) {
        const auto start = std::chrono::steady_clock::now();
        if (game.is_over()) {
            return SolveResult{};
        }
        const std::vector<Plan> plans = root_moves(game, options);
        SolveResult result = solve_worlds({&game}, plans, options);
        if (planning(game)) {
            ++result.stats.and_trees;
        }
        result.stats.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        return result;
    }

} // namespace pokebot
