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

        void require_invariants(const GameState& game, std::uint64_t seed, int actions) {
            const auto problems = check_invariants(game);
            if (problems.empty()) {
                return;
            }
            std::string message = "invariant broken in game " + std::to_string(seed) + " after " + std::to_string(actions) + " actions:";
            for (const auto& problem : problems) {
                message += "\n  - " + problem;
            }
            message += "\n" + describe(game);
            throw std::logic_error(message);
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

    GameRecord play_game(const CardTable& cards, std::uint64_t seed, Agent& player0, Agent& player1,
                         const MatchOptions& options) {
        const CardTable table = cards.shuffled(~seed); // numbered in decklist order, cards would give the decks away
        GameState game = new_game(table, seed);
        const std::array<Agent*, kPlayers> agents{&player0, &player1};
        int actions = 0;
        if (options.check_invariants) {
            require_invariants(game, seed, actions);
        }

        while (!game.is_over() && actions < options.max_actions) {
            const auto legal = legal_actions(game);
            Agent& agent = *agents[game.decision().player];
            const Action action = agent.choose(game, legal);
            if (std::find(legal.begin(), legal.end(), action) == legal.end()) {
                throw std::invalid_argument("play_game: the " + agent.name() + " agent chose an action that isn't legal");
            }
            apply_legal(game, action); // the list was just built, so apply needn't build it again
            ++actions;
            if (options.check_invariants) {
                require_invariants(game, seed, actions);
            }
        }
        return GameRecord{game.result, how_game_ended(game), game.first_player, game.turn, actions};
    }

} // namespace pokebot
