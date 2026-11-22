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
  engine/effects/triggers.cpp
  engine/effects/custom.cpp
  engine/setup/setup.cpp
  engine/actions/turn.cpp
  engine/actions/main_actions.cpp
  engine/actions/combat.cpp
  engine/actions/conditions.cpp
  engine/actions/lasting.cpp
  engine/actions/bench_limit.cpp
  engine/knowledge/knowledge.cpp
  engine/observation/observation.cpp
  engine/invariants/invariants.cpp
  engine/describe/describe.cpp
  agents/random/random_agent.cpp
  agents/human_agent.cpp
  agents/world.cpp
  agents/evaluation/evaluation.cpp
  agents/greedy/greedy_agent.cpp
  agents/planner/action_class.cpp
  agents/planner/state_hash.cpp
  agents/planner/plan.cpp
  agents/planner/or_tree.cpp
  agents/planner/and_tree.cpp
  agents/planner/planner_agent.cpp
  agents/flat_mc/deck_model.cpp
  agents/flat_mc/playout.cpp
  agents/flat_mc/flat_mc_agent.cpp
  agents/is_mcts/info_set.cpp
  agents/is_mcts/tree.cpp
  agents/is_mcts/macro_actions.cpp
  agents/is_mcts/is_mcts.cpp
  agents/is_mcts/is_mcts_agent.cpp
  harness/match.cpp
  harness/replay.cpp
  harness/arena.cpp
  harness/planner_lab.cpp
  harness/budget_lab.cpp
)
target_include_directories(pokebot_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# card.hpp includes nlohmann/json, so anything using pokebot_lib needs it too.
# The match harness plays games on several threads.
find_package(Threads REQUIRED)
target_link_libraries(pokebot_lib
  PUBLIC nlohmann_json::nlohmann_json Threads::Threads
  PRIVATE pokebot_warnings)

# Command-line client (play against the bot, run matches).
add_executable(pokebot client/main.cpp)
target_link_libraries(pokebot PRIVATE pokebot_lib pokebot_warnings)
