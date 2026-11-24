#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "agents/endgame/expectiminimax.hpp"
#include "agents/flat_mc/deck_model.hpp"
#include "engine/observation/observation.hpp"
#include "engine/shared/rng.hpp"

namespace pokebot {

    struct EndgameOptions {
        int worlds = 4;
        int max_depth = 2;
        long long max_nodes = 10000;
        ExpectiminimaxOptions search{};
    };

    struct EndgameResult {
        std::vector<SolvedPlan> plans;
        std::size_t best = 0;
        double value = 0.0;
        int depth = 0;
        long long visited = 0;
    };

    struct EndgameStats {
        long long solves = 0;
        long long iterations = 0;
        std::array<long long, 8> depths{};
        long long dropped = 0;
        ExpectiminimaxStats search;
        double seconds = 0.0;
    };

    class EndgameSolver {
    public:
        EndgameSolver(const DeckModel* model, std::uint64_t seed, const EndgameOptions& options = {});

        EndgameResult solve(const Observation& observation);
        const EndgameStats& stats() const { return stats_; }

    private:
        const DeckModel* model_;
        EndgameOptions options_;
        Rng rng_;
        EndgameStats stats_;
    };

} // namespace pokebot
