#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "agents/agent.hpp"
#include "agents/evaluation/evaluation.hpp"
#include "agents/flat_mc/deck_model.hpp"
#include "agents/flat_mc/playout.hpp"
#include "agents/greedy/greedy_agent.hpp"
#include "agents/planner/and_tree.hpp"
#include "agents/planner/plan.hpp"
#include "engine/shared/rng.hpp"

namespace pokebot {

    struct FlatMcOptions {
        int or_budget = 200;
        int and_budget = 300;
        int samples = 1;
        int plans = 4;
        int playouts = 32;
        PlayoutOptions playout;
        Weights weights = default_weights();
    };

    struct FlatMcStats {
        long long plans = 0;
        long long wins = 0;
        long long searched = 0;
        long long playouts = 0;
        long long playout_actions = 0;
        long long followed = 0;
        long long greedy = 0;
        long long or_nodes = 0;
        long long and_nodes = 0;
        double or_seconds = 0.0;
        double and_seconds = 0.0;
        double playout_seconds = 0.0;
        double greedy_seconds = 0.0;
    };

    struct ScoredPlan {
        Plan plan;
        double plan_value = 0.0;
        double mean = 0.0;
        int playouts = 0;
    };

    class FlatMcAgent final : public Agent {
    public:
        FlatMcAgent(std::uint64_t seed, const DeckModel& model, const FlatMcOptions& options = {});

        std::string name() const override { return "flatmc"; }
        Action choose(const Observation& observation, const std::vector<Action>& actions) override;
        const FlatMcStats& stats() const { return stats_; }
        const std::vector<ScoredPlan>& analysis() const { return analysis_; }
        std::vector<std::pair<std::string, double>> counters() const override;

    private:
        bool following(const Observation& observation, const std::vector<Action>& actions) const;
        bool plan_turn(const Observation& observation);
        std::vector<ScoredPlan> score(const Observation& observation, int me, const std::vector<RankedPlan>& plans);

        const DeckModel& model_;
        FlatMcOptions options_;
        Rng rng_;
        GreedyAgent greedy_;
        std::vector<Action> plan_;
        std::vector<Observation> expected_;
        std::size_t next_ = 0;
        std::vector<ScoredPlan> analysis_;
        FlatMcStats stats_;
    };

} // namespace pokebot