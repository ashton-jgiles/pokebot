#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "agents/agent.hpp"
#include "agents/evaluation/evaluation.hpp"
#include "agents/flat_mc/deck_model.hpp"
#include "agents/greedy/greedy_agent.hpp"
#include "agents/is_mcts/is_mcts.hpp"
#include "agents/planner/plan.hpp"
#include "engine/game/game_state.hpp"
#include "engine/shared/rng.hpp"

namespace pokebot {

    struct IsMctsAgentOptions {
        int or_budget = 200;
        IsMctsOptions search{.iterations = 42, .playout{.depth = 2}, .matched_worlds = true};
        Weights weights = default_weights();
    };

    struct IsMctsAgentStats {
        long long plans = 0;
        long long wins = 0;
        long long followed = 0;
        long long greedy = 0;
        long long or_nodes = 0;
        double or_seconds = 0.0;
    };

    class IsMctsAgent final : public Agent {
    public:
        IsMctsAgent(std::uint64_t seed, const DeckModel& model, const IsMctsAgentOptions& options = {});

        std::string name() const override { return "ismcts"; }
        Action choose(const Observation& observation, const std::vector<Action>& actions) override;
        const IsMctsAgentStats& stats() const { return stats_; }
        const IsMctsStats& search_stats() const { return search_.stats(); }
        const std::vector<RootChoice>& analysis() const { return analysis_; }
        std::vector<std::pair<std::string, double>> counters() const override;
        bool following(const Observation& observation, const std::vector<Action>& actions) const;

    private:
        Action follow(const GameState& world, int me, const Plan& plan);

        const DeckModel& model_;
        IsMctsAgentOptions options_;
        Rng rng_;
        IsMcts search_;
        GreedyAgent greedy_;
        std::vector<Action> plan_;
        std::vector<Observation> expected_;
        std::size_t next_ = 0;
        std::vector<RootChoice> analysis_;
        IsMctsAgentStats stats_;
    };

} // namespace pokebot
