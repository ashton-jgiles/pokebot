# Command-line tool: pokebot-sim [--games N] [--agent random|biased] [--seed S] [--no-check] [--print]
add_executable(pokebot-sim main.cpp)
target_link_libraries(pokebot-sim PRIVATE pokebot_lib pokebot_warnings)
