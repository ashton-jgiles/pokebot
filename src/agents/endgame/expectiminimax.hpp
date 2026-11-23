#pragma once

#include <vector>

#include "agents/endgame/chance.hpp"
#include "agents/evaluation/evaluation.hpp"
#include "agents/planner/plan.hpp"
#include "engine/game/game_state.hpp"

namespace pokebot {

    struct ExpectiminimaxOptions {
        int depth = 2;
        int root_budget = 300;
        int root_plans = 16;
        int budget = 25;
        int plans = 4;
        int samples = 1;
        bool prune = true;
        ChanceOptions chance{};
        Weights weights = default_weights();
    };

    struct ExpectiminimaxStats {
        long long nodes = 0;
        long long chance_nodes = 0;
        long long leaves = 0;
        long long and_trees = 0;
        long long alpha_beta_cuts = 0;
        long long star1_cuts = 0;
        ChanceStats chance;
        double seconds = 0.0;
    };

    struct SolvedPlan {
        Plan plan;
        double value = 0.0;
        bool exact = false;
    };

    struct SolveResult {
        std::vector<SolvedPlan> plans;
        std::size_t best = 0;
        double value = -1.0;
        ExpectiminimaxStats stats;
    };

    SolveResult expectiminimax(const GameState& game, const ExpectiminimaxOptions& options = {});

} // namespace pokebot
