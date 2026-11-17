#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "agents/random_agent.hpp"
#include "cards/db/card_db.hpp"
#include "core/decklist.hpp"
#include "engine/describe.hpp"
#include "engine/engine.hpp"
#include "harness/match.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

    using namespace pokebot;

    struct Options {
        long long games = 1000;
        std::string agent = "biased";
        std::uint64_t seed = 1;
        bool check = true;
        bool print = false; // narrate one game instead of running many
    };

    void print_usage() {
        std::cerr << "usage: pokebot-sim [--games N] [--agent random|biased] [--seed S] [--no-check] [--print]\n"
                     "  --games N    games to play (default 1000)\n"
                     "  --agent A    random or biased (default biased)\n"
                     "  --seed S     first game's seed; game i uses S + i (default 1)\n"
                     "  --no-check   skip the invariant check after every action (faster)\n"
                     "  --print      play one game with seed S and print every action\n";
    }

    bool parse(int argc, char** argv, Options& options) {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            const bool has_value = i + 1 < argc;
            if (arg == "--games" && has_value) {
                options.games = std::stoll(argv[++i]);
            } else if (arg == "--agent" && has_value) {
                options.agent = argv[++i];
            } else if (arg == "--seed" && has_value) {
                options.seed = std::stoull(argv[++i]);
            } else if (arg == "--no-check") {
                options.check = false;
            } else if (arg == "--print") {
                options.print = true;
            } else {
                return false;
            }
        }
        return options.games > 0 && (options.agent == "random" || options.agent == "biased");
    }

    std::unique_ptr<Agent> make_agent(const std::string& name, std::uint64_t seed) {
        if (name == "random") {
            return std::make_unique<RandomAgent>(seed);
        }
        return std::make_unique<BiasedRandomAgent>(seed);
    }

    // Every .txt in data/decks, sorted so deck numbering doesn't depend on the filesystem.
    std::vector<std::filesystem::path> deck_files() {
        std::vector<std::filesystem::path> files;
        for (const auto& file : std::filesystem::directory_iterator("data/decks")) {
            if (file.path().extension() == ".txt") {
                files.push_back(file.path());
            }
        }
        std::sort(files.begin(), files.end());
        return files;
    }

    // Plays one game, printing the board at the start of each turn and every action taken.
    void narrate(const CardTable& table, const Options& options) {
        GameState game = new_game(table, options.seed);
        const auto agent0 = make_agent(options.agent, options.seed * 2);
        const auto agent1 = make_agent(options.agent, options.seed * 2 + 1);
        int last_turn = -1;
        for (int actions = 0; !game.is_over() && actions < 20000; ++actions) {
            if (game.turn != last_turn) {
                last_turn = game.turn;
                std::cout << '\n'
                          << describe(game);
            }
            const auto legal = legal_actions(game);
            const int player = game.decision().player;
            Agent& agent = player == 0 ? *agent0 : *agent1;
            const Action action = agent.choose(game, legal);
            std::cout << "  > player " << player << ": " << describe(game, action) << '\n';
            apply(game, action);
        }
        std::cout << '\n'
                  << describe(game) << "Ended by: " << to_string(how_game_ended(game)) << '\n';
    }

    double percent(long long part, long long whole) {
        return whole == 0 ? 0.0 : 100.0 * static_cast<double>(part) / static_cast<double>(whole);
    }

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    Options options;
    if (!parse(argc, argv, options)) {
        print_usage();
        return 2;
    }

    try {
        const CardDB db = CardDB::load("data/cards/generated", "data/cards/effects");
        const auto files = deck_files();
        if (files.size() < 2) {
            std::cerr << "error: need at least 2 decklists in data/decks\n";
            return 1;
        }
        std::vector<Decklist> decks;
        for (const auto& file : files) {
            decks.push_back(load_decklist(file));
        }

        if (options.print) {
            const CardTable table(db, decks[0], decks[1 % decks.size()]);
            std::cout << "Game " << options.seed << ": " << files[0].filename().string() << " (player 0) vs "
                      << files[1 % files.size()].filename().string() << " (player 1)\n";
            narrate(table, options);
            return 0;
        }

        // Game i plays deck (i mod n) against deck ((i / n) mod n), so every pairing comes up.
        const std::size_t n = decks.size();
        std::vector<std::unique_ptr<CardTable>> tables(n * n);
        MatchOptions match_options;
        match_options.check_invariants = options.check;

        std::array<long long, 5> ends{};
        long long total_turns = 0;
        long long total_actions = 0;
        long long first_player_wins = 0;
        long long decided = 0;

        const auto start = std::chrono::steady_clock::now();
        for (long long i = 0; i < options.games; ++i) {
            const std::size_t game_index = static_cast<std::size_t>(i);
            const std::size_t pairing = (game_index % n) * n + (game_index / n) % n;
            if (!tables[pairing]) {
                tables[pairing] = std::make_unique<CardTable>(db, decks[pairing / n], decks[pairing % n]);
            }
            const std::uint64_t seed = options.seed + static_cast<std::uint64_t>(i);
            const auto agent0 = make_agent(options.agent, seed * 2);
            const auto agent1 = make_agent(options.agent, seed * 2 + 1);
            const GameRecord record = play_game(*tables[pairing], seed, *agent0, *agent1, match_options);

            ++ends[static_cast<std::size_t>(record.end)];
            total_turns += record.turns;
            total_actions += record.actions;
            if (record.result == GameResult::Player0Wins || record.result == GameResult::Player1Wins) {
                ++decided;
                const int winner = record.result == GameResult::Player0Wins ? 0 : 1;
                if (winner == record.first_player) {
                    ++first_player_wins;
                }
            }
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

        std::cout << std::fixed << std::setprecision(1);
        std::cout << "Played " << options.games << " games (" << options.agent << " vs " << options.agent << ") across "
                  << n << " decks in " << seconds << " s\n";
        std::cout << "  " << static_cast<double>(options.games) / seconds << " games/s, "
                  << static_cast<double>(total_actions) / seconds / 1e6 << " million actions/s\n";
        std::cout << "  average " << static_cast<double>(total_turns) / static_cast<double>(options.games) << " turns, "
                  << static_cast<double>(total_actions) / static_cast<double>(options.games) << " actions per game\n";
        std::cout << "Ended by:\n";
        for (std::size_t end = 0; end < ends.size(); ++end) {
            std::cout << "  " << std::setw(16) << std::left << to_string(static_cast<GameEnd>(end)) << std::right
                      << std::setw(8) << ends[end] << "  (" << percent(ends[end], options.games) << "%)\n";
        }
        std::cout << "Player who went first won " << percent(first_player_wins, decided) << "% of decided games\n";
        std::cout << (options.check ? "Invariants checked after every action: no problems found\n"
                                    : "Invariant checks were off (--no-check)\n");
        return ends[static_cast<std::size_t>(GameEnd::ActionLimit)] == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
