#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "agents/evaluation/evaluation.hpp"
#include "cards/db/card_db.hpp"
#include "harness/weight_fit.hpp"

namespace pokebot {

    struct SampleSet {
        std::vector<std::string> decks;
        std::vector<Sample> samples;
        long long games = 0;
    };

    struct Model {
        std::string name;
        std::function<Weights(const SampleRefs& train)> fit;
    };

    struct Group {
        long long positions = 0;
        double loss = 0.0;
        double predicted = 0.0;
        double result = 0.0;
        double gap_error = 0.0;
    };

    struct Change {
        double loss = 0.0;
        double loss_error = 0.0;
        double change = 0.0;
        double change_error = 0.0;
    };

    struct Breakdown {
        Change total;
        Group all;
        std::array<Group, 2> by_kind{};
        std::array<Group, 7> by_prizes{};
        std::array<Group, 10> by_prediction{};
        std::vector<Group> by_deck;
    };

    struct ModelScore {
        std::string name;
        Weights weights{};
        Weights weight_errors{};
        GameTerms terms;
        Breakdown raw;
        Breakdown adjusted;
        std::vector<Change> left_out;
        std::vector<Weights> left_out_weights;
    };

    inline constexpr std::uint64_t kFolds = 10;

    SampleSet load_samples(const CardDB& db, const std::vector<std::filesystem::path>& files, int threads = 1);

    Model fixed_model(std::string name, const Weights& weights);
    Model scaled_model(std::string name, const Weights& weights, std::size_t decks, const FitOptions& options = {});
    Model fitted_model(std::string name, std::size_t decks, const FitOptions& options = {});

    std::vector<ModelScore> score_models(const SampleSet& set, const std::vector<Model>& models, std::size_t reference, int threads = 1);

} // namespace pokebot
