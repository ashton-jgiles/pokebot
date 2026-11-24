#include "agents/set_weights.hpp"

namespace pokebot {

    void set_weights(PlannerOptions& options, const Weights& weights) {
        options.weights = weights;
    }

    void set_weights(FlatMcOptions& options, const Weights& weights) {
        options.weights = weights;
        options.playout.weights = weights;
    }

    void set_weights(IsMctsAgentOptions& options, const Weights& weights) {
        options.weights = weights;
        options.search.playout.weights = weights;
        options.search.macros.weights = weights;
    }

    void set_weights(EndgameAgentOptions& options, const Weights& weights) {
        set_weights(options.is_mcts, weights);
        options.solver.search.weights = weights;
    }

} // namespace pokebot
