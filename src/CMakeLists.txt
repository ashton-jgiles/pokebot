# Core library: everything except the executables links against this.
# Engine, cards, agents and harness sources get added here as milestones land.
add_library(pokebot_lib
  core/decklist.cpp
  cards/card/card.cpp
  cards/effects/program.cpp
  cards/db/card_db.cpp
  cards/validator/deck_validator.cpp
  engine/game/game_state.cpp
  engine/engine.cpp
  engine/rules/rules.cpp
  engine/events/events.cpp
  engine/effects/interpreter.cpp
  engine/setup/setup.cpp
  engine/actions/turn.cpp
  engine/actions/main_actions.cpp
  engine/actions/combat.cpp
  engine/invariants/invariants.cpp
  engine/describe/describe.cpp
  agents/random_agent.cpp
  harness/match.cpp
)
target_include_directories(pokebot_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# card.hpp includes nlohmann/json, so anything using pokebot_lib needs it too.
target_link_libraries(pokebot_lib
  PUBLIC nlohmann_json::nlohmann_json
  PRIVATE pokebot_warnings)

# Command-line client (play against the bot, run matches).
add_executable(pokebot client/main.cpp)
target_link_libraries(pokebot PRIVATE pokebot_lib pokebot_warnings)
