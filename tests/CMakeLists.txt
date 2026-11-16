include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.8.1
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(pokebot_tests
  test_version.cpp
  test_decklist.cpp
  test_tcgdex.cpp
)
target_link_libraries(pokebot_tests PRIVATE
  pokebot_lib pokebot_import_lib pokebot_warnings Catch2::Catch2WithMain)
# Lets tests find files in the repo (e.g. data/decks) no matter where they run from.
target_compile_definitions(pokebot_tests PRIVATE
  POKEBOT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(pokebot_tests)
