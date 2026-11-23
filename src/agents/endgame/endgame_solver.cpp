#include "agents/endgame/endgame_solver.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <numeric>

#include "agents/world.hpp"

namespace pokebot {

    namespace {

        void add(ExpectiminimaxStats& total, const ExpectiminimaxStats& more) {
            total.nodes += more.nodes;
            total.chance_nodes += more.chance_nodes;
            total.leaves += more.leaves;
            total.and_trees += more.and_trees;
            total.alpha_beta_cuts += more.alpha_beta_cuts;
            total.star1_cuts += more.star1_cuts;
            total.chance.actions += more.chance.actions;
            total.chance.flipped += more.chance.flipped;
            total.chance.enumerated += more.chance.enumerated;
            total.chance.sampled += more.chance.sampled;
            total.chance.outcomes += more.chance.outcomes;
            total.seconds += more.seconds;
        }

        bool playable(const GameState& game, const Plan& plan) {
            GameState walk = game;
            return apply_plan_checked(walk, plan);
        }

    } // namespace

    EndgameSolver::EndgameSolver(const DeckModel* model, std::uint64_t seed, const EndgameOptions& options) : model_(model), options_(options), rng_(seed) {}

    EndgameResult EndgameSolver::solve(const Observation& observation) {
        const auto start = std::chrono::steady_clock::now();
        ++stats_.solves;
        EndgameResult result;
        std::vector<std::unique_ptr<World>> worlds;
        std::vector<const GameState*> games;
        for (int i = 0; i < std::max(1, options_.worlds); ++i) {
            Rng deal(rng_.next());
            worlds.push_back(std::make_unique<World>(observation, deal, model_));
            games.push_back(&worlds.back()->game());
        }
        std::vector<Plan> plans;
        for (const Plan& plan : root_moves(*games.front(), options_.search)) {
            if (std::all_of(games.begin() + 1, games.end(), [&](const GameState* game) { return playable(*game, plan); })) {
                plans.push_back(plan);
            } else {
                ++stats_.dropped;
            }
        }
        if (plans.size() == 1) {
            result.plans.push_back(SolvedPlan{plans.front(), 0.0, false});
        }
        for (int depth = 1; depth <= options_.max_depth && plans.size() > 1; ++depth) {
            ExpectiminimaxOptions search = options_.search;
            search.depth = depth;
            if (options_.max_nodes > 0) {
                if (result.visited >= options_.max_nodes) {
                    break;
                }
                search.max_nodes = options_.max_nodes - result.visited;
            }
            const SolveResult r = solve_worlds(games, plans, search);
            ++stats_.iterations;
            add(stats_.search, r.stats);
            result.visited += visited(r.stats);
            if (!r.complete) {
                break;
            }
            result.plans = r.plans;
            result.best = r.best;
            result.value = r.value;
            result.depth = depth;
            std::vector<std::size_t> order(r.plans.size());
            std::iota(order.begin(), order.end(), std::size_t{0});
            std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return r.plans[a].value > r.plans[b].value; });
            for (std::size_t i = 0; i < order.size(); ++i) {
                plans[i] = r.plans[order[i]].plan;
            }
        }
        ++stats_.depths[static_cast<std::size_t>(std::min<int>(result.depth, static_cast<int>(stats_.depths.size()) - 1))];
        stats_.seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        return result;
    }

} // namespace pokebot
