#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "agents/agent.hpp"
#include "agents/evaluation.hpp"
#include "agents/greedy_agent.hpp"
#include "engine/shared/rng.hpp"

namespace pokebot {

    struct PlannerOptions {
        int or_budget = 200;
        int and_budget = 300;
        int samples = 3;
        Weights weights = default_weights();
    };

    struct PlannerStats  {
        long long plans = 0;
        long long wins = 0;
        long long followed = 0;
        long long greedy = 0;
    };

    class PlannerAgent final : public Agent {
        public:
            explicit PlannerAgent(std::uint64_t seed, const PlannerOptions& options = {});

            std::string name() const override { return "planner"; }
            Action choose(const Observation& observation, const std::vector<Action>& actions) override;
            const PlannerStats& stats() const { return stats_; }

        private:
            bool following(const Observation& observation, const std::vector<Action>& actions) const;
            bool plan_turn(const Observation& observation);

            PlannerOptions options_;
            Rng rng_;
            GreedyAgent greedy_;
            std::vector<Action> plan_;
            std::vector<Observation> expected_;
            std::size_t next_ = 0;
            PlannerStats stats_;
    };
    
} // namespace pokebot