include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.8.1
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(pokebot_tests
  test_decklist.cpp
  test_tcgdex.cpp
  test_convert.cpp
  test_card_db.cpp
  test_deck_validator.cpp
  test_engine_state.cpp
  test_engine_setup.cpp
  test_engine_turns.cpp
  test_engine_actions.cpp
  test_engine_attacks.cpp
  test_rules.cpp
  test_engine_events.cpp
  test_effect_program.cpp
  test_engine_effects.cpp
  test_engine_trainers.cpp
  test_engine_conditions.cpp
  test_engine_damage.cpp
  test_engine_targets.cpp
  test_engine_lasting.cpp
  test_engine_passive.cpp
  test_engine_abilities.cpp
  test_engine_triggers.cpp
  test_engine_energy.cpp
  test_engine_attach.cpp
  test_engine_custom.cpp
  test_engine_knowledge.cpp
  test_observation.cpp
  test_invariants.cpp
  test_agents.cpp
  test_world.cpp
  test_evaluation.cpp
  test_greedy.cpp
  test_replay.cpp
  test_client.cpp
  test_stress.cpp
  test_coverage.cpp
)
target_link_libraries(pokebot_tests PRIVATE
  pokebot_lib pokebot_import_lib pokebot_warnings Catch2::Catch2WithMain)
# Lets tests find files in the repo (e.g. data/decks) no matter where they run from.
target_compile_definitions(pokebot_tests PRIVATE
  POKEBOT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(pokebot_tests)
