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

        constexpr double kMaxScale = 10.0;
        constexpr int kScaleSteps = 40;

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

        std::vector<std::vector<Position>> all_positions(const CardDB& db, const std::vector<Replay>& games, int threads) {
            std::vector<std::vector<Position>> out(games.size());
            std::atomic<std::size_t> next{0};
            const auto work = [&] {
                for (std::size_t g = next++; g < games.size(); g = next++) {
                    out[g] = positions(db, games[g], g);
                }
            };
            std::vector<std::future<void>> workers;
            for (int t = 0; t < std::max(threads, 1); ++t) {
                workers.push_back(std::async(std::launch::async, work));
            }
            for (std::future<void>& worker : workers) {
                worker.get();
            }
            return out;
        }

        std::vector<double> held_out_scores(const SampleSet& set, const Model& model) {
            std::vector<double> z(set.samples.size());
            for (std::uint64_t fold = 0; fold < kFolds; ++fold) {
                SampleRefs train;
                for (const Sample& sample : set.samples) {
                    if (sample.seed % kFolds != fold) {
                        train.push_back(&sample);
                    }
                }
                const Weights weights = model.fit(train);
                for (std::size_t i = 0; i < z.size(); ++i) {
                    if (set.samples[i].seed % kFolds == fold) {
                        z[i] = weighted_sum(weights, set.samples[i].position.features);
                    }
                }
            }
            return z;
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
            const std::vector<std::vector<Position>> found = all_positions(db, games, threads);
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

    double weighted_sum(const Weights& weights, const Features& features) {
        double sum = 0.0;
        for (std::size_t i = 0; i < kFeatureCount; ++i) {
            sum += weights[i] * features[i];
        }
        return sum;
    }

    double win_chance(double z) {
        return 1.0 / (1.0 + std::exp(-2.0 * z));
    }

    double log_loss(double z, double result) {
        const double t = 2.0 * z;
        return std::max(t, 0.0) + std::log1p(std::exp(-std::abs(t))) - result * t;
    }

    double best_scale(const SampleRefs& train, const Weights& weights) {
        std::vector<std::pair<double, double>> rows;
        rows.reserve(train.size());
        for (const Sample* sample : train) {
            rows.emplace_back(weighted_sum(weights, sample->position.features), sample->position.result);
        }
        const auto slope = [&](double scale) {
            double sum = 0.0;
            for (const auto& [z, result] : rows) {
                sum += (win_chance(scale * z) - result) * z;
            }
            return sum;
        };
        double low = 0.0;
        double high = kMaxScale;
        for (int step = 0; step < kScaleSteps; ++step) {
            const double middle = 0.5 * (low + high);
            if (slope(middle) < 0.0) {
                low = middle;
            } else {
                high = middle;
            }
        }
        return 0.5 * (low + high);
    }

    Model fixed_model(std::string name, const Weights& weights) {
        return Model{std::move(name), [weights](const SampleRefs&) { return weights; }};
    }

    Model scaled_model(std::string name, const Weights& weights) {
        return Model{std::move(name), [weights](const SampleRefs& train) {
                         const double scale = best_scale(train, weights);
                         Weights scaled = weights;
                         for (double& weight : scaled) {
                             weight *= scale;
                         }
                         return scaled;
                     }};
    }

    std::vector<ModelScore> score_models(const SampleSet& set, const std::vector<Model>& models, std::size_t reference) {
        if (reference >= models.size()) {
            throw std::invalid_argument("score_models: no such reference model");
        }
        std::vector<std::vector<double>> z;
        for (const Model& model : models) {
            z.push_back(held_out_scores(set, model));
        }
        SampleRefs everything;
        for (const Sample& sample : set.samples) {
            everything.push_back(&sample);
        }

        std::vector<ModelScore> out;
        for (std::size_t m = 0; m < models.size(); ++m) {
            GroupBuilder all;
            Clustered change;
            std::array<GroupBuilder, 2> by_kind;
            std::array<GroupBuilder, 7> by_prizes;
            std::array<GroupBuilder, 10> by_prediction;
            std::vector<GroupBuilder> by_deck(set.decks.size());
            for (std::size_t i = 0; i < set.samples.size(); ++i) {
                const Sample& sample = set.samples[i];
                const Position& position = sample.position;
                const double own = z[m][i];
                all.add(sample, own);
                change.add(sample.seed, log_loss(own, position.result) - log_loss(z[reference][i], position.result));
                by_kind[position.start ? 0 : 1].add(sample, own);
                by_prizes[static_cast<std::size_t>(std::min(position.prizes, position.opponent_prizes))].add(sample, own);
                by_prediction[bin_of(win_chance(own), by_prediction.size())].add(sample, own);
                by_deck[sample.deck].add(sample, own);
            }
            ModelScore result;
            result.name = models[m].name;
            result.weights = models[m].fit(everything);
            result.all = all.group();
            result.loss_error = all.loss.error();
            result.change = change.mean();
            result.change_error = change.error();
            const auto groups = [](const auto& from, auto& to) {
                for (std::size_t i = 0; i < from.size(); ++i) {
                    to[i] = from[i].group();
                }
            };
            groups(by_kind, result.by_kind);
            groups(by_prizes, result.by_prizes);
            groups(by_prediction, result.by_prediction);
            result.by_deck.resize(by_deck.size());
            groups(by_deck, result.by_deck);
            out.push_back(std::move(result));
        }
        return out;
    }

} // namespace pokebot
