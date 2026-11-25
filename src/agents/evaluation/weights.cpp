#include "agents/evaluation/weights.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace pokebot {

    namespace {

        std::size_t feature_named(const std::string& name) {
            for (std::size_t i = 0; i < kFeatureCount; ++i) {
                if (name == to_string(static_cast<Feature>(i))) {
                    return i;
                }
            }
            return kFeatureCount;
        }

    } // namespace

    Weights load_weights(const std::filesystem::path& path) {
        std::ifstream in(path);
        if (!in) {
            throw std::runtime_error("cannot open weights " + path.string());
        }
        Weights weights{};
        std::array<bool, kFeatureCount> given{};
        std::string line;
        for (int number = 1; std::getline(in, line); ++number) {
            const auto fail = [&](const std::string& why) {
                throw std::runtime_error(path.string() + " line " + std::to_string(number) + ": " + why);
            };
            std::istringstream words(line);
            std::string name;
            if (!(words >> name)) {
                continue;
            }
            const std::size_t feature = feature_named(name);
            if (feature == kFeatureCount) {
                fail("no feature is called " + name);
            }
            if (given[feature]) {
                fail(name + " is given twice");
            }
            std::string extra;
            if (!(words >> weights[feature]) || !std::isfinite(weights[feature]) || (words >> extra)) {
                fail("expected a feature and one number");
            }
            given[feature] = true;
        }
        for (std::size_t i = 0; i < kFirstCandidate; ++i) {
            if (!given[i]) {
                throw std::runtime_error(path.string() + ": no weight for " + to_string(static_cast<Feature>(i)));
            }
        }
        return weights;
    }

    void save_weights(const std::filesystem::path& path, const Weights& weights) {
        std::ofstream out(path);
        out << std::setprecision(std::numeric_limits<double>::max_digits10);
        for (std::size_t i = 0; i < kFeatureCount; ++i) {
            if (i < kFirstCandidate || weights[i] != 0.0) {
                out << to_string(static_cast<Feature>(i)) << ' ' << weights[i] << '\n';
            }
        }
        out.flush();
        if (!out) {
            throw std::runtime_error("cannot write weights " + path.string());
        }
    }

} // namespace pokebot