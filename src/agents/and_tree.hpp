#pragma once

#include <vector>

#include "agents/evaluation.hpp"
#include "agents/plan.hpp"
#include "engine/game/game_state.hpp"

namespace pokebot {

    struct AndTreeOptions {
        int budget = 2000;
        int keep = 1;
        bool closed_set = true;
        bool bound = true;
    };

    struct RankedPlan {
        Plan plan;
        double value = 0.0;
    };

    struct AndTreeResult {
        std::vector<RankedPlan> plans;
        bool out_of_budget = false;
        int expanded = 0;
        int generated = 0;
        int closed = 0;
        int pruned = 0;
        int scored = 0;
    };

    AndTreeResult and_tree_search(const GameState& game, int player, PlanScorer& scorer, const AndTreeOptions& options = {});

    double upper_bound(const GameState& game, int player, const Weights& weights);

} // namespace pokebot
