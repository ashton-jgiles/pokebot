#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/random/random_agent.hpp"
#include "engine/shared/rng.hpp"
#include "harness/evaluation_lab.hpp"
#include "harness/match.hpp"
#include "support/test_cards.hpp"

using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::Group;
using pokebot::Model;
using pokebot::ModelScore;
using pokebot::Replay;
using pokebot::Sample;
using pokebot::SampleRefs;
using pokebot::SampleSet;
using pokebot::Weights;

namespace {

    const Weights kTruth{1.5, 0.4, 0.6, 0.3, 0.2, 0.1, 0.3, 0.2, 0.1, 0.5};

    SampleSet made_up(std::uint64_t seeds, int per_seed, double scale) {
        pokebot::Rng rng(17);
        SampleSet set;
        set.decks = {"one", "two"};
        for (std::uint64_t seed = 1; seed <= seeds; ++seed) {
            for (int i = 0; i < per_seed; ++i) {
                Sample sample;
                sample.seed = seed;
                sample.deck = rng.below(2);
                sample.opponent_deck = 1 - sample.deck;
                sample.position.start = rng.below(2) == 0;
                sample.position.prizes = static_cast<int>(1 + rng.below(6));
                sample.position.opponent_prizes = static_cast<int>(1 + rng.below(6));
                for (double& value : sample.position.features) {
                    value = static_cast<double>(rng.below(2001)) / 1000.0 - 1.0;
                }
                sample.position.result = pokebot::win_chance(scale * pokebot::weighted_sum(kTruth, sample.position.features));
                set.samples.push_back(sample);
            }
        }
        set.games = static_cast<long long>(seeds);
        return set;
    }

    SampleRefs refs(const SampleSet& set) {
        SampleRefs out;
        for (const Sample& sample : set.samples) {
            out.push_back(&sample);
        }
        return out;
    }

    template <class Groups>
    long long total(const Groups& groups) {
        long long sum = 0;
        for (const Group& group : groups) {
            sum += group.positions;
        }
        return sum;
    }

    CardDef with_attack(CardDef c, int damage) {
        c.attacks.push_back(test::attack("Tackle", {pokebot::EnergyType::Colorless}, damage));
        return c;
    }

    CardDB sample_db() {
        CardDB db;
        db.add(with_attack(test::basic_pokemon("TST-1", "Tackler"), 30));
        db.add(test::basic_energy("TST-2", "Darkness Energy", pokebot::EnergyType::Darkness));
        db.add(with_attack(test::basic_pokemon("TST-3", "Tackler B"), 30));
        db.add(with_attack(test::basic_pokemon("TST-4", "Tackler C"), 30));
        return db;
    }

    std::vector<Replay> play(const CardDB& db, std::uint64_t first_seed, std::uint64_t games) {
        const pokebot::Decklist deck = test::deck({{4, 1}, {4, 3}, {4, 4}, {48, 2}});
        std::vector<Replay> out;
        for (std::uint64_t seed = first_seed; seed < first_seed + games; ++seed) {
            const CardTable table(db, deck, deck);
            pokebot::BiasedRandomAgent first(seed * 2);
            pokebot::BiasedRandomAgent second(seed * 2 + 1);
            const auto record = pokebot::play_game(table, seed, first, second);
            Replay replay;
            replay.seed = seed;
            replay.players[0] = {"one.txt", "biased", deck};
            replay.players[1] = {"two.txt", "biased", deck};
            replay.actions = record.moves;
            replay.result = record.result;
            out.push_back(replay);
        }
        return out;
    }

    struct TempFile {
        std::filesystem::path path;

        explicit TempFile(const std::string& name) : path(std::filesystem::temp_directory_path() / ("pokebot-" + name + ".jsonl")) { remove(); }
        ~TempFile() { remove(); }
        TempFile(const TempFile&) = delete;
        TempFile& operator=(const TempFile&) = delete;

        void remove() const {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    };

} // namespace

TEST_CASE("log loss and win chance, at known and extreme values", "[evaluation_lab]") {
    CHECK(std::abs(pokebot::win_chance(0.0) - 0.5) < 1e-12);
    CHECK(std::abs(pokebot::log_loss(0.0, 1.0) - std::log(2.0)) < 1e-12);
    CHECK(std::abs(pokebot::log_loss(0.0, 0.5) - std::log(2.0)) < 1e-12);
    for (const double z : {-1.5, -0.2, 0.7, 2.0}) {
        const double chance = pokebot::win_chance(z);
        CHECK(std::abs(2.0 * chance - 1.0 - std::tanh(z)) < 1e-12);
        CHECK(std::abs(pokebot::log_loss(z, 1.0) + std::log(chance)) < 1e-12);
        CHECK(std::abs(pokebot::log_loss(z, 0.0) + std::log(1.0 - chance)) < 1e-12);
    }
    CHECK(std::abs(pokebot::log_loss(400.0, 0.0) - 800.0) < 1e-9);
    CHECK(std::abs(pokebot::log_loss(400.0, 1.0)) < 1e-9);
    CHECK(std::abs(pokebot::log_loss(-400.0, 0.0)) < 1e-9);
}

TEST_CASE("the best scale of weights that are only too small or too large is found", "[evaluation_lab]") {
    for (const double scale : {0.25, 1.0, 3.0}) {
        INFO("scale " << scale);
        const SampleSet set = made_up(20, 30, scale);
        CHECK(std::abs(pokebot::best_scale(refs(set), kTruth) - scale) < 1e-6);
        const Weights fitted = pokebot::scaled_model("scaled", kTruth).fit(refs(set));
        for (std::size_t i = 0; i < fitted.size(); ++i) {
            CHECK(std::abs(fitted[i] - scale * kTruth[i]) < 1e-5);
        }
    }
}

TEST_CASE("weights that predict backwards get a scale of 0", "[evaluation_lab]") {
    const SampleSet set = made_up(20, 30, -1.0);
    CHECK(pokebot::best_scale(refs(set), kTruth) < 1e-6);
}

TEST_CASE("doubling every sample within its seed changes neither the loss nor its error", "[evaluation_lab]") {
    const SampleSet once = made_up(40, 10, 1.0);
    SampleSet twice = once;
    twice.samples.insert(twice.samples.end(), once.samples.begin(), once.samples.end());
    const std::vector<Model> models{pokebot::fixed_model("coin", Weights{}), pokebot::fixed_model("truth", kTruth)};
    const auto a = pokebot::score_models(once, models, 0);
    const auto b = pokebot::score_models(twice, models, 0);
    REQUIRE(a.size() == 2);
    REQUIRE(b.size() == 2);
    CHECK(a[1].loss_error > 0.0);
    CHECK(a[1].change < 0.0);
    CHECK(a[1].change_error > 0.0);
    for (std::size_t m = 0; m < 2; ++m) {
        CHECK(b[m].all.positions == 2 * a[m].all.positions);
        CHECK(std::abs(a[m].all.loss - b[m].all.loss) < 1e-9);
        CHECK(std::abs(a[m].all.gap_error - b[m].all.gap_error) < 1e-9);
        CHECK(std::abs(a[m].loss_error - b[m].loss_error) < 1e-9);
        CHECK(std::abs(a[m].change - b[m].change) < 1e-9);
        CHECK(std::abs(a[m].change_error - b[m].change_error) < 1e-9);
    }
}

TEST_CASE("no seed is scored by a model fitted on it", "[evaluation_lab]") {
    SampleSet set = made_up(40, 5, 1.0);
    for (Sample& sample : set.samples) {
        sample.position.features = {};
        sample.position.features[sample.seed % pokebot::kFolds] = 1.0;
        sample.position.result = 1.0;
    }
    const auto fits = std::make_shared<std::vector<std::set<std::uint64_t>>>();
    const Model model{"seen", [fits](const SampleRefs& train) {
                          std::set<std::uint64_t> seeds;
                          Weights weights{};
                          for (const Sample* sample : train) {
                              seeds.insert(sample->seed);
                              weights[sample->seed % pokebot::kFolds] = 5.0;
                          }
                          fits->push_back(seeds);
                          return weights;
                      }};
    const auto scores = pokebot::score_models(set, {model}, 0);
    REQUIRE(fits->size() == pokebot::kFolds + 1);
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
        const auto with = std::count_if(fits->begin(), fits->end() - 1, [&](const auto& seeds) { return seeds.contains(seed); });
        CHECK(with == static_cast<std::ptrdiff_t>(pokebot::kFolds) - 1);
        CHECK(fits->back().contains(seed));
    }
    REQUIRE(scores.size() == 1);
    CHECK(std::abs(scores[0].all.loss - std::log(2.0)) < 1e-9);
    for (const double weight : scores[0].weights) {
        CHECK(weight == 5.0);
    }
}

TEST_CASE("every breakdown covers all positions, and the reference doesn't change from itself", "[evaluation_lab]") {
    const SampleSet set = made_up(40, 10, 1.0);
    const std::vector<Model> models{pokebot::fixed_model("coin", Weights{}), pokebot::fixed_model("truth", kTruth)};
    const auto scores = pokebot::score_models(set, models, 1);
    for (const ModelScore& score : scores) {
        INFO(score.name);
        CHECK(score.all.positions == 400);
        CHECK(total(score.by_kind) == 400);
        CHECK(total(score.by_prizes) == 400);
        CHECK(total(score.by_prediction) == 400);
        CHECK(total(score.by_deck) == 400);
        CHECK(score.by_prizes[0].positions == 0);
    }
    CHECK(std::abs(scores[0].all.loss - std::log(2.0)) < 1e-9);
    CHECK(scores[0].by_prediction[5].positions == 400);
    CHECK(scores[1].change == 0.0);
    CHECK(scores[1].change_error == 0.0);
    CHECK(scores[0].change > 0.0);
    for (const Group& group : scores[1].by_prediction) {
        CHECK(std::abs(group.predicted - group.result) < 1e-9);
    }
    CHECK_THROWS_AS(pokebot::score_models(set, models, 2), std::invalid_argument);
}

TEST_CASE("samples load the same on any number of threads, and a repeated seed is rejected", "[evaluation_lab]") {
    const CardDB db = sample_db();
    const std::vector<Replay> first = play(db, 1, 6);
    const std::vector<Replay> second = play(db, 7, 6);
    const TempFile a("lab-a");
    const TempFile b("lab-b");
    pokebot::save_games(a.path, first);
    pokebot::save_games(b.path, second);

    const SampleSet one = pokebot::load_samples(db, {a.path, b.path}, 1);
    const SampleSet four = pokebot::load_samples(db, {a.path, b.path}, 4);
    CHECK(one.games == 12);
    CHECK(one.decks == std::vector<std::string>{"one.txt", "two.txt"});
    CHECK(four.decks == one.decks);
    std::size_t expected = 0;
    for (const auto& games : {first, second}) {
        for (const Replay& game : games) {
            expected += pokebot::positions(db, game).size();
        }
    }
    REQUIRE(expected > 0);
    REQUIRE(one.samples.size() == expected);
    REQUIRE(four.samples.size() == expected);
    for (std::size_t i = 0; i < expected; ++i) {
        const Sample& x = one.samples[i];
        const Sample& y = four.samples[i];
        CHECK(x.seed == y.seed);
        CHECK(x.deck == y.deck);
        CHECK(x.position.game == y.position.game);
        CHECK(x.position.action == y.position.action);
        CHECK(x.position.features == y.position.features);
        CHECK(x.position.result == y.position.result);
        CHECK(x.seed == x.position.game + 1);
        CHECK(one.decks[x.deck] == (x.position.player == 0 ? "one.txt" : "two.txt"));
        CHECK(x.opponent_deck == 1 - x.deck);
    }

    CHECK_THROWS_AS(pokebot::load_samples(db, {a.path, a.path}, 2), std::runtime_error);
    CHECK_THROWS_AS(pokebot::load_samples(db, {a.path.parent_path() / "pokebot-no-such-lab.jsonl"}, 2), pokebot::ReplayError);
}
