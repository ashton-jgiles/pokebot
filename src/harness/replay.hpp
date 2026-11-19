#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "cards/db/card_db.hpp"
#include "core/decklist.hpp"
#include "engine/actions/action.hpp"
#include "engine/game/game_state.hpp"

// A game saved so it can be played back exactly: the two decks, the seed and every action.
namespace pokebot {

    struct ReplayPlayer {
        std::string deck_name; // where the deck came from, such as "dragapult.txt"
        std::string agent;     // who played it, such as "biased"
        Decklist deck;

        bool operator==(const ReplayPlayer&) const = default;
    };

    struct Replay {
        std::uint64_t seed = 0;
        std::array<ReplayPlayer, kPlayers> players;
        std::vector<Action> actions;
        GameResult result = GameResult::Ongoing; // how the game ended; Ongoing if it was saved before the end
        std::string note;                        // why it was saved, such as the invariant that broke

        bool operator==(const Replay&) const = default;
    };

    // A replay that can't be read or doesn't play back.
    class ReplayError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    nlohmann::ordered_json to_json(const Replay& replay);
    Replay replay_from_json(const nlohmann::ordered_json& json);
    void save_replay(const Replay& replay, const std::filesystem::path& path);
    Replay load_replay(const std::filesystem::path& path);

    // The card table the game was played on, numbered as play_game numbers it.
    CardTable replay_table(const CardDB& db, const Replay& replay);

    // Plays the replay back on `table` (from replay_table) and returns the final state. `each` is
    // called with the state before and after every action. Throws ReplayError if an action isn't
    // legal or the game ends differently, which means the cards or the engine have changed.
    using ReplayStep = std::function<void(const GameState& before, const Action& action, const GameState& after)>;
    GameState play_back(const CardTable& table, const Replay& replay, const ReplayStep& each = {});

    // Shows everything, rather than one player's view.
    inline constexpr int kEveryone = -1;

    // Writes the game out: the board at the start of each turn and every action, as `viewer` saw them
    // (or everything, for kEveryone).
    void print_replay(std::ostream& out, const CardTable& table, const Replay& replay, int viewer = kEveryone);

} // namespace pokebot
