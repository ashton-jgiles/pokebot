#pragma once

#include <filesystem>

#include "agents/evaluation/evaluation.hpp"

namespace pokebot {

    Weights load_weights(const std::filesystem::path& path);
    void save_weights(const std::filesystem::path& path, const Weights& weights);

} // namespace pokebot
