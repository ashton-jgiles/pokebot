#include "harness/match.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

#include "engine/describe/describe.hpp"
#include "engine/engine.hpp"
#include "engine/invariants/invariants.hpp"

namespace pokebot {

    namespace {

        void require_invariants(const GameState& game, std::uint64_t seed, const std::vector<Action>& moves) {
            const auto problems = check_invariants(game);
            if (problems.empty()) {
                return;
            }
            std::string message = "invariant broken in game " + std::to_string(seed) + " after " + std::to_string(moves.size()) + " actions:";
            for (const auto& problem : problems) {
                message += "\n  - " + problem;
            }
            message += "\n" + describe(game);
            throw InvariantError(message, seed, moves);
        }

    } // namespace

    const char* to_string(GameEnd end) {
        switch (end) {
            case GameEnd::PrizeCards:
                return "Prize cards";
            case GameEnd::NoPokemon:
                return "no Pokemon left";
            case GameEnd::DeckOut:
                return "deck-out";
            case GameEnd::Draw:
                return "draw";
            case GameEnd::ActionLimit:
                return "action limit";
        }
        return "unknown";
    }

    GameEnd how_game_ended(const GameState& game) {
        switch (game.result) {
            case GameResult::Ongoing:
                return GameEnd::ActionLimit;
            case GameResult::Draw:
                return GameEnd::Draw;
            default:
                break;
        }
        const int winner = game.result == GameResult::Player0Wins ? 0 : 1;
        const PlayerState& loser = game.player(opponent_of(winner));
        if (game.player(winner).prizes.empty()) {
            return GameEnd::PrizeCards;
        }
        if (!loser.active && loser.bench.empty()) {
            return GameEnd::NoPokemon;
        }
        return GameEnd::DeckOut;
    }

    Action ask(Agent& agent, const GameState& game, const std::vector<Action>& actions) {
        return agent.choose(observe(game, game.decision().player), actions);
    }

    GameRecord play_game(const CardTable& cards, std::uint64_t seed, Agent& player0, Agent& player1,
                         const MatchOptions& options) {
        const CardTable table = cards.shuffled(~seed); // numbered in decklist order, cards would give the decks away
        GameState game = new_game(table, seed);
        const std::array<Agent*, kPlayers> agents{&player0, &player1};
        GameRecord record;
        record.moves.reserve(256);
        if (options.check_invariants) {
            require_invariants(game, seed, record.moves);
        }

        while (!game.is_over() && record.moves.size() < static_cast<std::size_t>(options.max_actions)) {
            const auto legal = legal_actions(game);
            Agent& agent = *agents[game.decision().player];
            const Action action = ask(agent, game, legal);
            if (std::find(legal.begin(), legal.end(), action) == legal.end()) {
                throw std::invalid_argument("play_game: the " + agent.name() + " agent chose an action that isn't legal");
            }
            if (options.on_action) {
                const GameState before = game;
                apply_legal(game, action);
                options.on_action(before, action, game);
            } else {
                apply_legal(game, action); // the list was just built, so apply needn't build it again
            }
            record.moves.push_back(action);
            if (options.check_invariants) {
                require_invariants(game, seed, record.moves);
            }
        }
        record.result = game.result;
        record.end = how_game_ended(game);
        record.first_player = game.first_player;
        record.turns = game.turn;
        record.actions = static_cast<int>(record.moves.size());
        return record;
    }

} // namespace pokebot
