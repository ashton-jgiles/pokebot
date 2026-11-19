#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "agents/agent.hpp"
#include "agents/evaluation.hpp"
#include "engine/shared/rng.hpp"

namespace pokebot {

    class GreedyAgent final : public Agent {
        public:
            explicit GreedyAgent(std::uint64_t seed, int worlds = 3, const Weights& weights = default_weights());
            std::string name() const override { return "greedy"; }
            Action choose(const Observation& observation, const std::vector<Action>& actions) override;
            std::vector<double> scores(const Observation& observation, const std::vector<Action>& actions);

        private:
            Rng rng_;
            int worlds_;
            Weights weights_;
    };
    
} // namespace pokebot