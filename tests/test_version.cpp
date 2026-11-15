#include <catch2/catch_test_macros.hpp>

#include "core/version.hpp"

TEST_CASE("version string is set from CMake", "[core]") {
  REQUIRE(pokebot::version() == "0.1.0");
}
