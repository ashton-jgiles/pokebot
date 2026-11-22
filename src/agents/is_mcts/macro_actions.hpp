#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "agents/evaluation/evaluation.hpp"
#include "agents/planner/plan.hpp"
#include "engine/game/game_state.hpp"

namespace pokebot {

    struct MacroOptions {
        int searcher_budget = 300;
        int searcher_plans = 16;
        int opponent_budget = 25;
        int opponent_plans = 4;
        int samples = 1;
        Weights weights = default_weights();
    };

    struct MacroStats {
        long long searcher_searches = 0;
        long long cache_hits = 0;
        long long opponent_searches = 0;
        long long single_actions = 0;
        double searcher_seconds = 0.0;
        double opponent_seconds = 0.0;
    };

    class MacroActions {
    public:
        MacroActions(int searcher, std::uint64_t seed, const MacroOptions& options = {});

        const std::vector<Plan>& moves(const GameState& world);
        const MacroStats& stats() const { return stats_; }

    private:
        std::vector<Plan> plans(const GameState& world, int actor, int budget, int keep);

        int searcher_;
        std::uint64_t seed_;
        MacroOptions options_;
        std::unordered_map<std::uint64_t, std::vector<Plan>> cache_;
        std::vector<Plan> scratch_;
        MacroStats stats_;
    };

} // namespace pokebot
