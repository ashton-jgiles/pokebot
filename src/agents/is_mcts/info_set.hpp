#pragma once

#include <cstdint>

#include "engine/observation/observation.hpp"

namespace pokebot {

    std::uint64_t hash_observation(const Observation& observation, int numbered = -1);

} // namespace pokebot