#include "harness/evaluation_lab.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <future>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace pokebot {

    namespace {

        constexpr std::size_t kNoDeck = static_cast<std::size_t>(-1);

        class Clustered {
        public:
            void add(std::uint64_t seed, double value) {
                Cluster& cluster = clusters_[seed];
                cluster.sum += value;
                ++cluster.count;
                sum_ += value;
                ++count_;
            }

            double mean() const { return count_ > 0 ? sum_ / static_cast<double>(count_) : 0.0; }

            double error() const {
                if (clusters_.size() < 2) {
                    return 0.0;
                }
                double squares = 0.0;
                for (const auto& [seed, cluster] : clusters_) {
                    const double off = cluster.sum - mean() * static_cast<double>(cluster.count);
                    squares += off * off;
                }
                const double n = static_cast<double>(clusters_.size());
                return std::sqrt(squares * n / (n - 1.0)) / static_cast<double>(count_);
            }

            long long count() const { return count_; }

        private:
            struct Cluster {
                double sum = 0.0;
                long long count = 0;
            };

            std::map<std::uint64_t, Cluster> clusters_;
            double sum_ = 0.0;
            long long count_ = 0;
        };

        struct GroupBuilder {
            Clustered loss;
            Clustered gap;
            double predicted = 0.0;
            double result = 0.0;

            void add(const Sample& sample, double z) {
                const double chance = win_chance(z);
                loss.add(sample.seed, log_loss(z, sample.position.result));
                gap.add(sample.seed, sample.position.result - chance);
                predicted += chance;
                result += sample.position.result;
            }

            Group group() const {
                Group out;
                out.positions = loss.count();
                if (out.positions > 0) {
                    const double n = static_cast<double>(out.positions);
                    out.loss = loss.mean();
                    out.predicted = predicted / n;
                    out.result = result / n;
                    out.gap_error = gap.error();
                }
                return out;
            }
        };

        std::size_t bin_of(double chance, std::size_t bins) {
            return std::min(static_cast<std::size_t>(chance * static_cast<double>(bins)), bins - 1);
        }

        std::size_t deck_index(std::vector<std::string>& decks, const std::string& name) {
            const auto at = std::find(decks.begin(), decks.end(), name);
            if (at != decks.end()) {
                return static_cast<std::size_t>(at - decks.begin());
            }
            decks.push_back(name);
            return decks.size() - 1;
        }

        template <class Work>
        void in_parallel(std::size_t jobs, int threads, const Work& work) {
            std::atomic<std::size_t> next{0};
            const auto take = [&] {
                for (std::size_t job = next++; job < jobs; job = next++) {
                    work(job);
                }
            };
            std::vector<std::future<void>> workers;
            for (int t = 0; t < std::max(threads, 1); ++t) {
                workers.push_back(std::async(std::launch::async, take));
            }
            for (std::future<void>& worker : workers) {
                worker.get();
            }
        }

        bool has_deck(const Sample& sample, std::size_t deck) {
            return deck != kNoDeck && (sample.deck == deck || sample.opponent_deck == deck);
        }

        struct HeldOut {
            std::vector<double> raw;
            std::vector<double> adjusted;
            std::array<Weights, kFolds> weights{};
        };

        void score_fold(const SampleSet& set, const Model& model, std::uint64_t fold, std::size_t left_out, HeldOut& out) {
            SampleRefs train;
            SampleRefs without_deck;
            for (const Sample& sample : set.samples) {
                if (sample.seed % kFolds != fold) {
                    train.push_back(&sample);
                    if (!has_deck(sample, left_out)) {
                        without_deck.push_back(&sample);
                    }
                }
            }
            if (without_deck.empty()) {
                throw std::runtime_error("no samples to fit on");
            }
            const Weights weights = model.fit(without_deck);
            const GameTerms terms = fit_terms(train, set.decks.size(), weights).terms;
            out.weights[fold] = weights;
            for (std::size_t i = 0; i < set.samples.size(); ++i) {
                const Sample& sample = set.samples[i];
                if (sample.seed % kFolds == fold && (left_out == kNoDeck || has_deck(sample, left_out))) {
                    out.raw[i] = weighted_sum(weights, sample.position.features);
                    out.adjusted[i] = out.raw[i] + term_sum(terms, sample, set.decks.size());
                }
            }
        }

        Weights jackknife_errors(const std::array<Weights, kFolds>& fits) {
            const double n = static_cast<double>(kFolds);
            Weights out{};
            for (std::size_t i = 0; i < kFeatureCount; ++i) {
                double mean = 0.0;
                for (const Weights& fit : fits) {
                    mean += fit[i] / n;
                }
                for (const Weights& fit : fits) {
                    out[i] += (fit[i] - mean) * (fit[i] - mean);
                }
                out[i] = std::sqrt(out[i] * (n - 1.0) / n);
            }
            return out;
        }

        Change change_of(const SampleSet& set, const std::vector<double>& z, const std::vector<double>& reference, std::size_t deck) {
            Clustered loss;
            Clustered change;
            for (std::size_t i = 0; i < set.samples.size(); ++i) {
                const Sample& sample = set.samples[i];
                if (deck == kNoDeck || has_deck(sample, deck)) {
                    const double own = log_loss(z[i], sample.position.result);
                    loss.add(sample.seed, own);
                    change.add(sample.seed, own - log_loss(reference[i], sample.position.result));
                }
            }
            return Change{loss.mean(), loss.error(), change.mean(), change.error()};
        }

        Breakdown break_down(const SampleSet& set, const std::vector<double>& z, const std::vector<double>& reference) {
            GroupBuilder all;
            std::array<GroupBuilder, 2> by_kind;
            std::array<GroupBuilder, 7> by_prizes;
            std::array<GroupBuilder, 10> by_prediction;
            std::vector<GroupBuilder> by_deck(set.decks.size());
            for (std::size_t i = 0; i < set.samples.size(); ++i) {
                const Sample& sample = set.samples[i];
                const Position& position = sample.position;
                all.add(sample, z[i]);
                by_kind[position.start ? 0 : 1].add(sample, z[i]);
                by_prizes[static_cast<std::size_t>(std::min(position.prizes, position.opponent_prizes))].add(sample, z[i]);
                by_prediction[bin_of(win_chance(z[i]), by_prediction.size())].add(sample, z[i]);
                by_deck[sample.deck].add(sample, z[i]);
            }
            Breakdown out;
            out.total = change_of(set, z, reference, kNoDeck);
            out.all = all.group();
            const auto groups = [](const auto& from, auto& to) {
                for (std::size_t g = 0; g < from.size(); ++g) {
                    to[g] = from[g].group();
                }
            };
            groups(by_kind, out.by_kind);
            groups(by_prizes, out.by_prizes);
            groups(by_prediction, out.by_prediction);
            out.by_deck.resize(by_deck.size());
            groups(by_deck, out.by_deck);
            return out;
        }

    } // namespace

    SampleSet load_samples(const CardDB& db, const std::vector<std::filesystem::path>& files, int threads) {
        SampleSet set;
        std::set<std::uint64_t> earlier;
        for (const std::filesystem::path& file : files) {
            const std::vector<Replay> games = load_games(file);
            std::set<std::uint64_t> seeds;
            for (const Replay& game : games) {
                if (earlier.contains(game.seed)) {
                    throw std::runtime_error(file.string() + ": seed " + std::to_string(game.seed) + " is in an earlier file");
                }
                seeds.insert(game.seed);
            }
            earlier.merge(seeds);
            std::vector<std::vector<Position>> found(games.size());
            in_parallel(games.size(), threads, [&](std::size_t g) { found[g] = positions(db, games[g], g); });
            for (std::size_t g = 0; g < games.size(); ++g) {
                const std::array<std::size_t, 2> decks{deck_index(set.decks, games[g].players[0].deck_name), deck_index(set.decks, games[g].players[1].deck_name)};
                for (Position position : found[g]) {
                    position.game += static_cast<std::size_t>(set.games);
                    const std::size_t player = static_cast<std::size_t>(position.player);
                    set.samples.push_back(Sample{games[g].seed, decks[player], decks[1 - player], position});
                }
            }
            set.games += static_cast<long long>(games.size());
        }
        return set;
    }

    Model fixed_model(std::string name, const Weights& weights) {
        return Model{std::move(name), [weights](const SampleRefs&) { return weights; }};
    }

    Model scaled_model(std::string name, const Weights& weights, std::size_t decks, const FitOptions& options) {
        FitOptions scale = options;
        scale.non_negative = true;
        return Model{std::move(name), [weights, decks, scale](const SampleRefs& train) { return fit_scale(train, decks, weights, scale).weights; }};
    }

    Model fitted_model(std::string name, std::size_t decks, const FitOptions& options) {
        return Model{std::move(name), [decks, options](const SampleRefs& train) { return fit_weights(train, decks, options).weights; }};
    }

    std::vector<ModelScore> score_models(const SampleSet& set, const std::vector<Model>& models, std::size_t reference, int threads) {
        if (reference >= models.size()) {
            throw std::invalid_argument("score_models: no such reference model");
        }
        const std::size_t decks = set.decks.size();
        const std::size_t cuts = decks > 2 ? decks + 1 : 1;
        const auto cut_deck = [](std::size_t cut) {
            return cut == 0 ? kNoDeck : cut - 1;
        };
        std::vector<std::vector<HeldOut>> held(models.size(), std::vector<HeldOut>(cuts));
        for (std::vector<HeldOut>& model : held) {
            for (HeldOut& cut : model) {
                cut.raw.resize(set.samples.size());
                cut.adjusted.resize(set.samples.size());
            }
        }
        const std::size_t folds = static_cast<std::size_t>(kFolds);
        in_parallel(models.size() * cuts * folds, threads, [&](std::size_t job) {
            const std::size_t m = job / (cuts * folds);
            const std::size_t cut = job / folds % cuts;
            score_fold(set, models[m], job % folds, cut_deck(cut), held[m][cut]);
        });

        std::vector<ModelScore> out(models.size());
        in_parallel(models.size(), threads, [&](std::size_t m) {
            ModelScore& score = out[m];
            score.name = models[m].name;
            score.raw = break_down(set, held[m][0].raw, held[reference][0].raw);
            score.adjusted = break_down(set, held[m][0].adjusted, held[reference][0].adjusted);
            score.weight_errors = jackknife_errors(held[m][0].weights);
            for (std::size_t cut = 0; cut < cuts; ++cut) {
                SampleRefs train;
                for (const Sample& sample : set.samples) {
                    if (!has_deck(sample, cut_deck(cut))) {
                        train.push_back(&sample);
                    }
                }
                const Weights weights = models[m].fit(train);
                if (cut == 0) {
                    score.weights = weights;
                    score.terms = fit_terms(train, decks, weights).terms;
                } else {
                    score.left_out.push_back(change_of(set, held[m][cut].adjusted, held[reference][cut].adjusted, cut_deck(cut)));
                    score.left_out_weights.push_back(weights);
                }
            }
        });
        return out;
    }

} // namespace pokebot
