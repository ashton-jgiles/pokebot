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
#include "agents/greedy_agent.hpp"
#include "cards/db/card_db.hpp"
#include "core/decklist.hpp"
#include "harness/match.hpp"
#include "harness/replay.hpp"

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
        bool print = false;         // narrate one game instead of running many
        std::string save_replay;    // with --print: where to save that game
        std::string replay;         // print this saved game instead of playing
        int as = kEveryone;         // whose view --print and --replay show
        bool check_replays = false; // save, load and play back every game
    };

    void print_usage() {
        std::cerr << "usage: pokebot-sim [--games N] [--agent random|biased|greedy] [--seed S] [--no-check]\n"
                     "                   [--print [--save-replay FILE]] [--replay FILE] [--as 0|1|all]\n"
                     "  --games N            games to play (default 1000)\n"
                     "  --agent A            random, biased or greedy (default biased)\n"
                     "  --seed S             first game's seed; game i uses S + i (default 1)\n"
                     "  --no-check           skip the invariant check after every action (faster)\n"
                     "  --print              play one game with seed S and print every action\n"
                     "  --save-replay FILE   with --print: also save that game as a replay\n"
                     "  --replay FILE        print a saved replay instead of playing\n"
                     "  --as 0|1|all         with --print or --replay: show what that player saw (default all)\n"
                     "  --check-replays      save every game as a replay, load it and play it back, checking it\n"
                     "                       ends in exactly the same state (slower)\n"
                     "If an invariant breaks, the game is saved to replays/invariant-seed-S.json.\n";
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
            } else if (arg == "--check-replays") {
                options.check_replays = true;
            } else if (arg == "--save-replay" && has_value) {
                options.save_replay = argv[++i];
            } else if (arg == "--replay" && has_value) {
                options.replay = argv[++i];
            } else if (arg == "--as" && has_value) {
                const std::string who = argv[++i];
                if (who != "0" && who != "1" && who != "all") {
                    return false;
                }
                options.as = who == "all" ? kEveryone : std::stoi(who);
            } else {
                return false;
            }
        }
        if (!options.save_replay.empty() && !options.print) {
            return false;
        }
        return options.games > 0 && (options.agent == "random" || options.agent == "biased" || options.agent == "greedy");
    }

    std::unique_ptr<Agent> make_agent(const std::string& name, std::uint64_t seed) {
        if (name == "random") {
            return std::make_unique<RandomAgent>(seed);
        }
        if (name == "greedy") {
            return std::make_unique<GreedyAgent>(seed);
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

    // A replay of the game with `seed` between two decks, with no actions yet.
    Replay start_replay(std::uint64_t seed, const std::string& agent, const std::filesystem::path& file0, const Decklist& deck0,
                        const std::filesystem::path& file1, const Decklist& deck1) {
        Replay replay;
        replay.seed = seed;
        replay.players[0] = ReplayPlayer{file0.filename().string(), agent, deck0};
        replay.players[1] = ReplayPlayer{file1.filename().string(), agent, deck1};
        return replay;
    }

    // Plays one game with play_game. If an invariant breaks, saves the game as a replay, made by
    // make_replay() only then, so the decks aren't copied for every game.
    template <class MakeReplay>
    GameRecord play_saving_failures(const CardTable& table, std::uint64_t seed, const Options& options, MakeReplay make_replay,
                                    GameState* end = nullptr) {
        const auto agent0 = make_agent(options.agent, seed * 2);
        const auto agent1 = make_agent(options.agent, seed * 2 + 1);
        MatchOptions match_options;
        match_options.check_invariants = options.check;
        match_options.final_state = end;
        try {
            return play_game(table, seed, *agent0, *agent1, match_options);
        } catch (const InvariantError& e) {
            Replay replay = make_replay();
            replay.actions = e.moves;
            replay.note = e.what();
            const std::string path = "replays/invariant-seed-" + std::to_string(e.seed) + ".json";
            save_replay(replay, path);
            std::cerr << "saved the game as a replay: " << path << '\n';
            throw;
        }
    }

    // Saves the game as replay text, loads it back and plays it, checking it ends in exactly `end`.
    // A game that doesn't is saved to replays/mismatch-seed-S.json.
    bool round_trips(const CardDB& db, Replay replay, const GameRecord& record, const GameState& end) {
        replay.actions = record.moves;
        replay.result = record.result;
        bool same = false;
        try {
            const Replay loaded = replay_from_json(nlohmann::ordered_json::parse(to_json(replay).dump()));
            GameState replayed = play_back(replay_table(db, loaded), loaded);
            replayed.cards = nullptr;
            same = loaded == replay && replayed == end;
        } catch (const ReplayError&) {
            same = false;
        }
        if (!same) {
            const std::string path = "replays/mismatch-seed-" + std::to_string(replay.seed) + ".json";
            save_replay(replay, path);
            std::cerr << "game " << replay.seed << " didn't play back the same; saved it to " << path << '\n';
        }
        return same;
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
        if (!options.replay.empty()) {
            const Replay replay = load_replay(options.replay);
            print_replay(std::cout, replay_table(db, replay), replay, options.as);
            return 0;
        }
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
            const CardTable table(db, decks[0], decks[1]);
            Replay replay = start_replay(options.seed, options.agent, files[0], decks[0], files[1], decks[1]);
            const GameRecord record = play_saving_failures(table, options.seed, options, [&] { return replay; });
            replay.actions = record.moves;
            replay.result = record.result;
            print_replay(std::cout, replay_table(db, replay), replay, options.as);
            if (!options.save_replay.empty()) {
                save_replay(replay, options.save_replay);
                std::cout << "Saved the replay to " << options.save_replay << '\n';
            }
            return 0;
        }

        // Game i plays deck (i mod n) against deck ((i / n) mod n), so every pairing comes up.
        const std::size_t n = decks.size();
        std::vector<std::unique_ptr<CardTable>> tables(n * n);

        std::array<long long, 5> ends{};
        long long total_turns = 0;
        long long total_actions = 0;
        long long first_player_wins = 0;
        long long decided = 0;
        long long replay_mismatches = 0;

        const auto start = std::chrono::steady_clock::now();
        for (long long i = 0; i < options.games; ++i) {
            const std::size_t game_index = static_cast<std::size_t>(i);
            const std::size_t pairing = (game_index % n) * n + (game_index / n) % n;
            if (!tables[pairing]) {
                tables[pairing] = std::make_unique<CardTable>(db, decks[pairing / n], decks[pairing % n]);
            }
            const std::uint64_t seed = options.seed + static_cast<std::uint64_t>(i);
            const std::size_t a = pairing / n;
            const std::size_t b = pairing % n;
            const auto replay_of = [&] {
                return start_replay(seed, options.agent, files[a], decks[a], files[b], decks[b]);
            };
            GameState end;
            const GameRecord record = play_saving_failures(*tables[pairing], seed, options, replay_of, options.check_replays ? &end : nullptr);
            if (options.check_replays && !round_trips(db, replay_of(), record, end)) {
                ++replay_mismatches;
            }

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
        if (options.check_replays) {
            if (replay_mismatches == 0) {
                std::cout << "Replays: every game was saved, loaded and played back to exactly the same final state\n";
            } else {
                std::cout << "Replays: " << replay_mismatches << " of " << options.games << " games didn't play back the same\n";
            }
        }
        return ends[static_cast<std::size_t>(GameEnd::ActionLimit)] == 0 && replay_mismatches == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
