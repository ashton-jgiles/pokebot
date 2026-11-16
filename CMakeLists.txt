cmake_minimum_required(VERSION 3.24)

project(pokebot
  VERSION 0.1.0
  DESCRIPTION "Search-based AI for the Pokemon TCG"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MINGW)
  add_link_options(-static)
endif()

option(POKEBOT_BUILD_TESTS "Build the test suite" ON)
option(POKEBOT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# Shared warning settings, applied to every pokebot target.
add_library(pokebot_warnings INTERFACE)
if(MSVC)
  target_compile_options(pokebot_warnings INTERFACE /W4 /permissive-)
  if(POKEBOT_WARNINGS_AS_ERRORS)
    target_compile_options(pokebot_warnings INTERFACE /WX)
  endif()
else()
  target_compile_options(pokebot_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
  if(POKEBOT_WARNINGS_AS_ERRORS)
    target_compile_options(pokebot_warnings INTERFACE -Werror)
  endif()
endif()

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_subdirectory(src)
add_subdirectory(tools/import)

if(POKEBOT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
