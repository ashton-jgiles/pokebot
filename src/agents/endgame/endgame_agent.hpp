#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "agents/agent.hpp"
#include "agents/endgame/endgame_solver.hpp"
#include "agents/flat_mc/deck_model.hpp"
#include "agents/is_mcts/is_mcts_agent.hpp"
#include "agents/planner/plan.hpp"
#include "engine/shared/rng.hpp"

namespace pokebot {

    struct EndgameAgentOptions {
        int prizes = 2;
        IsMctsAgentOptions is_mcts{};
        EndgameOptions solver{};
    };

    struct EndgameAgentStats {
        long long endgame = 0;
        long long played = 0;
        long long fallbacks = 0;
        long long followed = 0;
    };

    class EndgameAgent final : public Agent {
    public:
        EndgameAgent(std::uint64_t seed, const DeckModel& model, const EndgameAgentOptions& options = {});

        std::string name() const override { return "endgame"; }
        Action choose(const Observation& observation, const std::vector<Action>& actions) override;
        std::vector<std::pair<std::string, double>> counters() const override;

        const EndgameAgentStats& stats() const { return stats_; }
        const EndgameSolver& solver() const { return solver_; }
        const IsMctsAgent& is_mcts() const { return is_mcts_; }
        const EndgameResult& analysis() const { return analysis_; }

    private:
        bool endgame(const Observation& observation, const std::vector<Action>& actions) const;
        bool following(const Observation& observation, const std::vector<Action>& actions) const;
        bool follow(const Observation& observation, const std::vector<Action>& actions, const Plan& plan);

        const DeckModel& model_;
        EndgameAgentOptions options_;
        Rng rng_;
        IsMctsAgent is_mcts_;
        EndgameSolver solver_;
        std::vector<Action> plan_;
        std::vector<Observation> expected_;
        std::size_t next_ = 0;
        EndgameResult analysis_;
        EndgameAgentStats stats_;
    };

} // namespace pokebot
