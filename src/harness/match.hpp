#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
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
        // Called after every action with the state before and after it, such as to show the game as it's played.
        std::function<void(const GameState& before, const Action& action, const GameState& after)> on_action;
        // If set, receives the state the game ended in, with `cards` cleared: the numbered table play_game made is gone.
        GameState* final_state = nullptr;
    };

    struct GameRecord {
        GameResult result = GameResult::Ongoing;
        GameEnd end = GameEnd::ActionLimit;
        int first_player = 0;
        int turns = 0;
        int actions = 0;
        std::vector<Action> moves; // every action taken, in order: what a replay needs
    };

    // An invariant broke during play_game. It carries the game's seed and moves, so the game can be
    // saved as a replay.
    class InvariantError : public std::logic_error {
    public:
        InvariantError(const std::string& message, std::uint64_t game_seed, std::vector<Action> taken)
            : std::logic_error(message), seed(game_seed), moves(std::move(taken)) {}

        std::uint64_t seed;
        std::vector<Action> moves; // up to and including the action after which it broke
    };

    GameRecord play_game(const CardTable& cards, std::uint64_t seed, Agent& player0, Agent& player1, const MatchOptions& options = {});

} // namespace pokebot
