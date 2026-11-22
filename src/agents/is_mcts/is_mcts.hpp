#pragma once

#include <cstdint>
#include <vector>

#include "agents/flat_mc/deck_model.hpp"
#include "agents/flat_mc/playout.hpp"
#include "agents/is_mcts/macro_actions.hpp"
#include "agents/planner/plan.hpp"
#include "engine/observation/observation.hpp"
#include "engine/shared/rng.hpp"

namespace pokebot {

    struct IsMctsOptions {
        int iterations = 200;
        double exploration = 0.7;
        int max_depth = 32;
        PlayoutOptions playout{.depth = 1};
        MacroOptions macros{};
    };

    struct IsMctsStats {
        long long searches = 0;
        long long iterations = 0;
        long long nodes = 0;
        long long playout_actions = 0;
        long long unplayable = 0;
        double seconds = 0.0;
        double playout_seconds = 0.0;
        MacroStats macros;
    };

    struct RootChoice {
        Plan plan;
        long long visits = 0;
        double mean = 0.0;
    };

    class IsMcts {
    public:
        IsMcts(const DeckModel& model, std::uint64_t seed, const IsMctsOptions& options = {});

        std::vector<RootChoice> search(const Observation& observation);
        const IsMctsStats& stats() const { return stats_; }

    private:
        const DeckModel& model_;
        IsMctsOptions options_;
        Rng rng_;
        IsMctsStats stats_;
    };

} // namespace pokebot