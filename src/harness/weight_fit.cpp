#include "harness/weight_fit.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace pokebot {

    namespace {

        constexpr double kTermL2 = 1e-6;
        constexpr int kMaxSteps = 50;
        constexpr int kMaxSweeps = 100000;
        constexpr double kStepGain = 1e-12;
        constexpr double kSweepGain = 1e-16;

        struct Problem {
            const SampleRefs* train = nullptr;
            std::size_t decks = 0;
            const Weights* direction = nullptr;
            std::size_t slopes = 0;
            std::vector<double> beta;
            std::vector<std::size_t> free;
            std::vector<double> l2;
            std::vector<char> non_negative;
        };

        void add_terms(const Sample& sample, std::size_t decks, double* out) {
            out[kFirstTerm] = sample.position.went_first ? 1.0 : -1.0;
            out[sample.position.start ? kStartTerm : kLaterTerm] = 1.0;
            if (sample.deck != sample.opponent_deck) {
                const std::size_t pairing = pairing_term(std::min(sample.deck, sample.opponent_deck), std::max(sample.deck, sample.opponent_deck), decks);
                out[pairing] = sample.deck < sample.opponent_deck ? 1.0 : -1.0;
            }
        }

        void fill(const Problem& problem, const Sample& sample, std::vector<double>& row) {
            std::fill(row.begin(), row.end(), 0.0);
            if (problem.direction != nullptr) {
                row[0] = weighted_sum(*problem.direction, sample.position.features);
            } else {
                std::copy(sample.position.features.begin(), sample.position.features.end(), row.begin());
            }
            add_terms(sample, problem.decks, row.data() + problem.slopes);
        }

        double dot(const std::vector<double>& a, const std::vector<double>& b) {
            double sum = 0.0;
            for (std::size_t i = 0; i < a.size(); ++i) {
                sum += a[i] * b[i];
            }
            return sum;
        }

        double penalty(const Problem& problem, const std::vector<double>& beta) {
            double sum = 0.0;
            for (const std::size_t j : problem.free) {
                sum += 0.5 * problem.l2[j] * beta[j] * beta[j];
            }
            return sum;
        }

        double objective(const Problem& problem, const std::vector<double>& beta) {
            std::vector<double> row(beta.size());
            double sum = 0.0;
            for (const Sample* sample : *problem.train) {
                fill(problem, *sample, row);
                sum += log_loss(dot(row, beta), sample->position.result);
            }
            return sum + penalty(problem, beta);
        }

        struct Curve {
            std::vector<double> gradient;
            std::vector<double> hessian;
            double value = 0.0;
        };

        Curve curve_at(const Problem& problem, const std::vector<double>& beta) {
            const std::size_t m = problem.free.size();
            Curve curve{std::vector<double>(m), std::vector<double>(m * m), penalty(problem, beta)};
            std::vector<double> row(beta.size());
            std::vector<double> x(m);
            for (const Sample* sample : *problem.train) {
                fill(problem, *sample, row);
                const double z = dot(row, beta);
                const double chance = win_chance(z);
                const double slope = 2.0 * (chance - sample->position.result);
                const double bend = 4.0 * chance * (1.0 - chance);
                curve.value += log_loss(z, sample->position.result);
                for (std::size_t a = 0; a < m; ++a) {
                    x[a] = row[problem.free[a]];
                }
                for (std::size_t a = 0; a < m; ++a) {
                    if (x[a] == 0.0) {
                        continue;
                    }
                    curve.gradient[a] += slope * x[a];
                    const double scaled = bend * x[a];
                    for (std::size_t b = a; b < m; ++b) {
                        curve.hessian[a * m + b] += scaled * x[b];
                    }
                }
            }
            for (std::size_t a = 0; a < m; ++a) {
                const std::size_t j = problem.free[a];
                curve.gradient[a] += problem.l2[j] * beta[j];
                curve.hessian[a * m + a] += problem.l2[j];
                for (std::size_t b = a + 1; b < m; ++b) {
                    curve.hessian[b * m + a] = curve.hessian[a * m + b];
                }
            }
            return curve;
        }

        std::vector<double> newton_step(const Problem& problem, const std::vector<double>& beta, const Curve& curve) {
            const std::size_t m = problem.free.size();
            std::vector<double> step(m);
            std::vector<double> slope = curve.gradient;
            for (int sweep = 0; sweep < kMaxSweeps; ++sweep) {
                double gain = 0.0;
                for (std::size_t a = 0; a < m; ++a) {
                    const double bend = curve.hessian[a * m + a];
                    if (bend <= 0.0) {
                        continue;
                    }
                    const std::size_t j = problem.free[a];
                    double next = step[a] - slope[a] / bend;
                    if (problem.non_negative[j]) {
                        next = std::max(next, -beta[j]);
                    }
                    const double change = next - step[a];
                    if (change == 0.0) {
                        continue;
                    }
                    step[a] = next;
                    for (std::size_t b = 0; b < m; ++b) {
                        slope[b] += curve.hessian[b * m + a] * change;
                    }
                    gain += 0.5 * bend * change * change;
                }
                if (gain <= kSweepGain * static_cast<double>(problem.train->size())) {
                    break;
                }
            }
            return step;
        }

        std::vector<double> moved(const Problem& problem, const std::vector<double>& beta, const std::vector<double>& step, double size) {
            std::vector<double> out = beta;
            for (std::size_t a = 0; a < step.size(); ++a) {
                const std::size_t j = problem.free[a];
                out[j] += size * step[a];
                if (problem.non_negative[j]) {
                    out[j] = std::max(out[j], 0.0);
                }
            }
            return out;
        }

        Fit minimise(Problem problem) {
            Fit fit;
            const std::size_t m = problem.free.size();
            const double enough = kStepGain * static_cast<double>(problem.train->size());
            while (fit.steps < kMaxSteps && !fit.converged) {
                const Curve curve = curve_at(problem, problem.beta);
                const std::vector<double> step = newton_step(problem, problem.beta, curve);
                double gain = 0.0;
                for (std::size_t a = 0; a < m; ++a) {
                    gain -= curve.gradient[a] * step[a];
                    for (std::size_t b = 0; b < m; ++b) {
                        gain -= 0.5 * step[a] * curve.hessian[a * m + b] * step[b];
                    }
                }
                ++fit.steps;
                if (gain <= enough) {
                    problem.beta = moved(problem, problem.beta, step, 1.0);
                    fit.converged = true;
                    break;
                }
                double size = 1.0;
                std::vector<double> next = moved(problem, problem.beta, step, size);
                while (objective(problem, next) > curve.value && size > 1e-6) {
                    size *= 0.5;
                    next = moved(problem, problem.beta, step, size);
                }
                problem.beta = next;
            }
            if (problem.direction != nullptr) {
                for (std::size_t i = 0; i < kFeatureCount; ++i) {
                    fit.weights[i] = problem.beta[0] * (*problem.direction)[i];
                }
            } else {
                std::copy_n(problem.beta.begin(), kFeatureCount, fit.weights.begin());
            }
            fit.terms.assign(problem.beta.begin() + static_cast<std::ptrdiff_t>(problem.slopes), problem.beta.end());
            return fit;
        }

        Problem start(const SampleRefs& train, std::size_t decks, std::size_t slopes) {
            if (train.empty()) {
                throw std::invalid_argument("no samples to fit");
            }
            Problem problem;
            problem.train = &train;
            problem.decks = decks;
            problem.slopes = slopes;
            problem.beta.resize(slopes + term_count(decks));
            problem.l2.assign(problem.beta.size(), kTermL2);
            problem.non_negative.assign(problem.beta.size(), 0);
            return problem;
        }

        Fit fit_slopes(Problem problem, const FitOptions& options) {
            for (std::size_t j = 0; j < problem.slopes; ++j) {
                problem.l2[j] = options.l2;
                problem.non_negative[j] = options.non_negative ? 1 : 0;
            }
            const std::size_t count = options.game_terms ? problem.beta.size() : problem.slopes;
            for (std::size_t j = 0; j < count; ++j) {
                problem.free.push_back(j);
            }
            return minimise(std::move(problem));
        }

    } // namespace

    std::size_t term_count(std::size_t decks) {
        return kPairingTerms + (decks > 1 ? decks * (decks - 1) / 2 : 0);
    }

    std::size_t pairing_term(std::size_t a, std::size_t b, std::size_t decks) {
        if (a >= b || b >= decks) {
            throw std::invalid_argument("pairing_term: needs a < b < decks");
        }
        return kPairingTerms + a * decks - a * (a + 1) / 2 + (b - a - 1);
    }

    double weighted_sum(const Weights& weights, const Features& features) {
        double sum = 0.0;
        for (std::size_t i = 0; i < kFeatureCount; ++i) {
            sum += weights[i] * features[i];
        }
        return sum;
    }

    double term_sum(const GameTerms& terms, const Sample& sample, std::size_t decks) {
        std::vector<double> row(term_count(decks));
        add_terms(sample, decks, row.data());
        return dot(row, terms);
    }

    double win_chance(double z) {
        return 1.0 / (1.0 + std::exp(-2.0 * z));
    }

    double log_loss(double z, double result) {
        const double t = 2.0 * z;
        return std::max(t, 0.0) + std::log1p(std::exp(-std::abs(t))) - result * t;
    }

    Fit fit_weights(const SampleRefs& train, std::size_t decks, const FitOptions& options) {
        return fit_slopes(start(train, decks, kFeatureCount), options);
    }

    Fit fit_scale(const SampleRefs& train, std::size_t decks, const Weights& direction, const FitOptions& options) {
        Problem problem = start(train, decks, 1);
        problem.direction = &direction;
        return fit_slopes(std::move(problem), options);
    }

    Fit fit_terms(const SampleRefs& train, std::size_t decks, const Weights& weights) {
        Problem problem = start(train, decks, kFeatureCount);
        std::copy(weights.begin(), weights.end(), problem.beta.begin());
        for (std::size_t j = kFeatureCount; j < problem.beta.size(); ++j) {
            problem.free.push_back(j);
        }
        return minimise(std::move(problem));
    }

} // namespace pokebot
