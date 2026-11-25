#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "engine/game/game_state.hpp"

namespace pokebot {

    enum class Feature : std::uint8_t {
        Prizes,
        ActiveHp,
        TurnsToKnockout,
        ReadyAttackers,
        Energy,
        Bench,
        Hand,
        Deck,
        Conditions,
        PrizesAtRisk,
        Evolved,
        DeckOut,
    };

    constexpr std::size_t kFeatureCount = 12;

    using Features = std::array<double, kFeatureCount>;
    using Weights = std::array<double, kFeatureCount>;

    const char* to_string(Feature feature);

    Weights default_weights();
    Weights learned_weights();
    Features features(const GameState& game, int player);
    Features features(const GameState& game, int player, const Weights& wanted);
    double utility(const GameState& game, int player);
    double evaluate(const GameState& game, int player, const Weights& weights = default_weights());
    
} // namespace pokebot