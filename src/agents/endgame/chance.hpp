#pragma once

#include <vector>

#include "engine/actions/action.hpp"
#include "engine/game/game_state.hpp"
#include "engine/shared/rng.hpp"

namespace pokebot {

    struct ChanceOptions {
        int max_outcomes = 32;
        int samples = 8;
        int max_flips = 8;
    };

    struct Outcome {
        GameState state;
        double probability = 0.0;
    };

    struct ChanceStats {
        long long actions = 0;
        long long flipped = 0;
        long long enumerated = 0;
        long long sampled = 0;
        long long outcomes = 0;
    };

    double count_placements(const std::vector<int>& copies, int first, int second = 0);
    std::vector<Outcome> chance_outcomes(const GameState& game, const Action& action, Rng& rng, const ChanceOptions& options = {},
                                         ChanceStats* stats = nullptr);

} // namespace pokebot
