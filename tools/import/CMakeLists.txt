add_library(pokebot_import_lib STATIC
  http_client.cpp
  tcgdex.cpp
  convert.cpp
  standard.cpp
  translate.cpp
)
target_include_directories(pokebot_import_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pokebot_import_lib
  PUBLIC pokebot_lib nlohmann_json::nlohmann_json
  PRIVATE pokebot_warnings)

add_executable(pokebot-import main.cpp)
target_link_libraries(pokebot-import PRIVATE pokebot_import_lib pokebot_warnings)
