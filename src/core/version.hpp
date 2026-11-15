#pragma once

#include <string_view>

namespace pokebot {

// Project version, set from CMake's project(VERSION ...).
std::string_view version();

}  // namespace pokebot
