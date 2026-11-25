#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "agents/evaluation/evaluation.hpp"
#include "harness/self_play.hpp"

namespace pokebot {

    struct Sample {
        std::uint64_t seed = 0;
        std::size_t deck = 0;
        std::size_t opponent_deck = 0;
        Position position;
    };

    using SampleRefs = std::vector<const Sample*>;
    using GameTerms = std::vector<double>;

    using FeatureMask = std::array<bool, kFeatureCount>;

    FeatureMask first_features(std::size_t count);

    struct FitOptions {
        double l2 = 0.0;
        bool non_negative = false;
        bool game_terms = true;
        FeatureMask use = first_features(kFeatureCount);
    };

    struct Fit {
        Weights weights{};
        GameTerms terms;
        int steps = 0;
        bool converged = false;
    };

    inline constexpr std::size_t kFirstTerm = 0;
    inline constexpr std::size_t kStartTerm = 1;
    inline constexpr std::size_t kLaterTerm = 2;
    inline constexpr std::size_t kPairingTerms = 3;

    std::size_t term_count(std::size_t decks);
    std::size_t pairing_term(std::size_t a, std::size_t b, std::size_t decks);

    double weighted_sum(const Weights& weights, const Features& features);
    double term_sum(const GameTerms& terms, const Sample& sample, std::size_t decks);
    double win_chance(double z);
    double log_loss(double z, double result);

    Fit fit_weights(const SampleRefs& train, std::size_t decks, const FitOptions& options = {});
    Fit fit_scale(const SampleRefs& train, std::size_t decks, const Weights& direction, const FitOptions& options = {});
    Fit fit_terms(const SampleRefs& train, std::size_t decks, const Weights& weights);

} // namespace pokebot
