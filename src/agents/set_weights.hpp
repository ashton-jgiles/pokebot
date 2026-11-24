#pragma once

#include "agents/endgame/endgame_agent.hpp"
#include "agents/evaluation/evaluation.hpp"
#include "agents/flat_mc/flat_mc_agent.hpp"
#include "agents/is_mcts/is_mcts_agent.hpp"
#include "agents/planner/planner_agent.hpp"

namespace pokebot {

    void set_weights(PlannerOptions& options, const Weights& weights);
    void set_weights(FlatMcOptions& options, const Weights& weights);
    void set_weights(IsMctsAgentOptions& options, const Weights& weights);
    void set_weights(EndgameAgentOptions& options, const Weights& weights);

} // namespace pokebot
