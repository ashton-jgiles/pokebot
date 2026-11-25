#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine/shared/rng.hpp"
#include "harness/weight_fit.hpp"

using pokebot::Fit;
using pokebot::FitOptions;
using pokebot::GameTerms;
using pokebot::kFeatureCount;
using pokebot::Sample;
using pokebot::SampleRefs;
using pokebot::Weights;

namespace {

    constexpr std::size_t kDecks = 3;

    const Weights kTruth{1.5, 0.4, 0.6, 0.3, 0.2, 0.1, 0.3, 0.2, 0.1, 0.5};
    const GameTerms kTerms{0.1, 0.2, -0.1, 0.5, -0.3, 0.4};

    std::vector<Sample> made_up(int count, std::size_t decks, const Weights& weights, const GameTerms& terms) {
        pokebot::Rng rng(29);
        std::vector<Sample> out;
        for (int i = 0; i < count; ++i) {
            Sample sample;
            sample.seed = static_cast<std::uint64_t>(i);
            sample.deck = rng.below(static_cast<std::uint32_t>(decks));
            sample.opponent_deck = rng.below(static_cast<std::uint32_t>(decks));
            sample.position.start = rng.below(2) == 0;
            sample.position.went_first = rng.below(2) == 0;
            for (double& value : sample.position.features) {
                value = static_cast<double>(rng.below(2001)) / 1000.0 - 1.0;
            }
            sample.position.result = pokebot::win_chance(pokebot::weighted_sum(weights, sample.position.features) + pokebot::term_sum(terms, sample, decks));
            out.push_back(sample);
        }
        return out;
    }

    SampleRefs refs(const std::vector<Sample>& samples) {
        SampleRefs out;
        for (const Sample& sample : samples) {
            out.push_back(&sample);
        }
        return out;
    }

    void check_close(const Fit& fit, const Weights& weights, const GameTerms& terms, double tolerance) {
        CHECK(fit.converged);
        for (std::size_t i = 0; i < kFeatureCount; ++i) {
            CAPTURE(i, fit.weights[i], weights[i]);
            CHECK(std::abs(fit.weights[i] - weights[i]) < tolerance);
        }
        REQUIRE(fit.terms.size() == terms.size());
        for (std::size_t j = 0; j < terms.size(); ++j) {
            CAPTURE(j, fit.terms[j], terms[j]);
            CHECK(std::abs(fit.terms[j] - terms[j]) < tolerance);
        }
    }

    double size_of(const Weights& weights) {
        double sum = 0.0;
        for (const double weight : weights) {
            sum += weight * weight;
        }
        return sum;
    }

} // namespace

TEST_CASE("known weights and terms are recovered from exact chances", "[weight_fit]") {
    CHECK(pokebot::term_count(kDecks) == kTerms.size());
    CHECK(pokebot::pairing_term(0, 1, kDecks) == 3);
    CHECK(pokebot::pairing_term(0, 2, kDecks) == 4);
    CHECK(pokebot::pairing_term(1, 2, kDecks) == 5);
    CHECK_THROWS_AS(pokebot::pairing_term(2, 1, kDecks), std::invalid_argument);
    const auto samples = made_up(2000, kDecks, kTruth, kTerms);
    check_close(pokebot::fit_weights(refs(samples), kDecks), kTruth, kTerms, 1e-4);
    CHECK_THROWS_AS(pokebot::fit_weights({}, kDecks), std::invalid_argument);
}

TEST_CASE("known weights and terms are recovered from random wins and losses", "[weight_fit]") {
    auto samples = made_up(20000, kDecks, kTruth, kTerms);
    pokebot::Rng rng(31);
    for (Sample& sample : samples) {
        sample.position.result = static_cast<double>(rng.below(1000000)) / 1e6 < sample.position.result ? 1.0 : 0.0;
    }
    check_close(pokebot::fit_weights(refs(samples), kDecks), kTruth, kTerms, 0.1);
}

TEST_CASE("a non-negative fit has no negative weight, and no small change improves it", "[weight_fit]") {
    Weights truth = kTruth;
    truth[3] = -0.5;
    truth[6] = -0.2;
    const auto samples = made_up(2000, kDecks, truth, kTerms);
    FitOptions options;
    options.non_negative = true;
    const Fit fit = pokebot::fit_weights(refs(samples), kDecks, options);
    CHECK(fit.converged);
    CHECK(fit.weights[3] == 0.0);
    CHECK(fit.weights[6] == 0.0);
    const auto loss = [&](const Weights& weights) {
        double sum = 0.0;
        for (const Sample& sample : samples) {
            const double z = pokebot::weighted_sum(weights, sample.position.features) + pokebot::term_sum(fit.terms, sample, kDecks);
            sum += pokebot::log_loss(z, sample.position.result);
        }
        return sum;
    };
    const double best = loss(fit.weights);
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        CHECK(fit.weights[i] >= 0.0);
        for (const double nudge : {-0.01, 0.01}) {
            Weights other = fit.weights;
            other[i] += nudge;
            if (other[i] >= 0.0) {
                CAPTURE(i, nudge);
                CHECK(loss(other) > best);
            }
        }
    }
}

TEST_CASE("more L2 gives smaller weights", "[weight_fit]") {
    const auto samples = made_up(2000, kDecks, kTruth, kTerms);
    double last = 2.0 * size_of(kTruth);
    for (const double l2 : {0.0, 100.0, 1000.0, 1e9}) {
        FitOptions options;
        options.l2 = l2;
        const Fit fit = pokebot::fit_weights(refs(samples), kDecks, options);
        CHECK(fit.converged);
        CHECK(size_of(fit.weights) < last);
        last = size_of(fit.weights);
    }
    CHECK(last < 1e-6);
}

TEST_CASE("the pairing terms stop a feature that only marks the deck from getting a weight", "[weight_fit]") {
    const GameTerms terms{0.0, 0.0, 0.0, 0.6};
    auto samples = made_up(4000, 2, Weights{}, terms);
    for (Sample& sample : samples) {
        const double deck = static_cast<double>(sample.opponent_deck) - static_cast<double>(sample.deck);
        sample.position.features[7] = 0.5 * deck + 0.5 * sample.position.features[7];
    }
    FitOptions options;
    CHECK(std::abs(pokebot::fit_weights(refs(samples), 2, options).weights[7]) < 1e-3);
    options.game_terms = false;
    CHECK(pokebot::fit_weights(refs(samples), 2, options).weights[7] > 0.3);
}

TEST_CASE("fitting the terms leaves the weights alone", "[weight_fit]") {
    const auto samples = made_up(2000, kDecks, kTruth, kTerms);
    const Fit fit = pokebot::fit_terms(refs(samples), kDecks, kTruth);
    CHECK(fit.weights == kTruth);
    check_close(fit, kTruth, kTerms, 1e-4);
    Weights doubled = kTruth;
    for (double& weight : doubled) {
        weight *= 2.0;
    }
    CHECK(pokebot::fit_terms(refs(samples), kDecks, doubled).weights == doubled);
}
