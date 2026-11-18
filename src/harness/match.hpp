#pragma once

#include <cstdint>
#include <vector>

#include "agents/agent.hpp"
#include "engine/game/game_state.hpp"

namespace pokebot {

    enum class GameEnd : std::uint8_t {
        PrizeCards,
        NoPokemon,
        DeckOut,
        Draw,
        ActionLimit,
    };

    const char* to_string(GameEnd end);

    // How a game ended; ActionLimit if it hasn't.
    GameEnd how_game_ended(const GameState& game);

    // Asks `agent` to make the game's current decision, showing it only what that player can see.
    Action ask(Agent& agent, const GameState& game, const std::vector<Action>& actions);

    struct MatchOptions {
        int max_actions = 20000;
        bool check_invariants = true;
    };

    struct GameRecord {
        GameResult result = GameResult::Ongoing;
        GameEnd end = GameEnd::ActionLimit;
        int first_player = 0;
        int turns = 0;
        int actions = 0;
    };

    GameRecord play_game(const CardTable& cards, std::uint64_t seed, Agent& player0, Agent& player1, const MatchOptions& options = {});

} // namespace pokebot
