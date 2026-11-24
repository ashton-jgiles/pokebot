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
#include "harness/self_play.hpp"

namespace pokebot {

    struct Sample {
        std::uint64_t seed = 0;
        std::size_t deck = 0;
        std::size_t opponent_deck = 0;
        Position position;
    };

    struct SampleSet {
        std::vector<std::string> decks;
        std::vector<Sample> samples;
        long long games = 0;
    };

    using SampleRefs = std::vector<const Sample*>;

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

    struct ModelScore {
        std::string name;
        Weights weights{};
        Group all;
        double loss_error = 0.0;
        double change = 0.0;
        double change_error = 0.0;
        std::array<Group, 2> by_kind{};
        std::array<Group, 7> by_prizes{};
        std::array<Group, 10> by_prediction{};
        std::vector<Group> by_deck;
    };

    inline constexpr std::uint64_t kFolds = 10;

    SampleSet load_samples(const CardDB& db, const std::vector<std::filesystem::path>& files, int threads = 1);

    double weighted_sum(const Weights& weights, const Features& features);
    double win_chance(double z);
    double log_loss(double z, double result);
    double best_scale(const SampleRefs& train, const Weights& weights);

    Model fixed_model(std::string name, const Weights& weights);
    Model scaled_model(std::string name, const Weights& weights);

    std::vector<ModelScore> score_models(const SampleSet& set, const std::vector<Model>& models, std::size_t reference);

} // namespace pokebot
