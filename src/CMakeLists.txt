# Core library: everything except the executables links against this.
# Engine, cards, agents and harness sources get added here as milestones land.
add_library(pokebot_lib
  core/version.cpp
)
target_include_directories(pokebot_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pokebot_lib PRIVATE pokebot_warnings)
target_compile_definitions(pokebot_lib PRIVATE
  POKEBOT_VERSION_STRING="${PROJECT_VERSION}")

# Command-line client (play against the bot, run matches).
add_executable(pokebot client/main.cpp)
target_link_libraries(pokebot PRIVATE pokebot_lib pokebot_warnings)
