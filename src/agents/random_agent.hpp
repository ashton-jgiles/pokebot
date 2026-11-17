#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "agents/agent.hpp"
#include "engine/rng.hpp"

namespace pokebot {

    // Picks uniformly at random: the baseline every other agent has to beat.
    class RandomAgent final : public Agent {
    public:
        explicit RandomAgent(std::uint64_t seed) : rng_(seed) {}

        std::string name() const override { return "random"; }
        Action choose(const GameState& game, const std::vector<Action>& actions) override;

    private:
        Rng rng_;
    };

    // Random, but strongly prefers attacking and powering up the Active Pokemon, so games end by
    // Knock Outs and Prize cards rather than deck-out. Used to stress-test the combat rules.
    class BiasedRandomAgent final : public Agent {
    public:
        explicit BiasedRandomAgent(std::uint64_t seed) : rng_(seed) {}

        std::string name() const override { return "biased"; }
        Action choose(const GameState& game, const std::vector<Action>& actions) override;

    private:
        Rng rng_;
    };

} // namespace pokebot
