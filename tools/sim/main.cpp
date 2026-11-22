#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "agents/flat_mc/deck_model.hpp"
#include "agents/flat_mc/flat_mc_agent.hpp"
#include "agents/flat_mc/playout.hpp"
#include "agents/greedy/greedy_agent.hpp"
#include "agents/planner/planner_agent.hpp"
#include "agents/random/random_agent.hpp"
#include "cards/db/card_db.hpp"
#include "core/decklist.hpp"
#include "harness/arena.hpp"
#include "harness/match.hpp"
#include "harness/planner_lab.hpp"
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
        bool print = false;              // narrate one game instead of running many
        std::string save_replay;         // with --print: where to save that game
        std::string replay;              // print this saved game instead of playing
        int as = kEveryone;              // whose view --print and --replay show
        bool check_replays = false;      // save, load and play back every game
        std::vector<std::string> agents; // --agents A,B: A against B on every pairing
        int threads = 0;                 // with --agents: games at once; 0 = one per hardware thread
        int replay_every = 0;            // with --agents: save every Nth game
        PlannerOptions planner;
        FlatMcOptions flat_mc;
        const DeckModel* model = nullptr;
        int lab_games = 0;
        int every = 5;
    };

    void print_usage() {
        std::cerr << "usage: pokebot-sim [--games N] [--agent random|biased|greedy|planner|flatmc] [--seed S] [--no-check]\n"
                     "                   [--print [--save-replay FILE]] [--replay FILE] [--as 0|1|all]\n"
                     "                   [--agents A,B [--threads N] [--save-replays K]]\n"
                     "                   [--planner-samples M] [--planner-or-budget N] [--planner-and-budget N]\n"
                     "                   [--mc-plans K] [--mc-playouts N] [--mc-depth R] [--mc-policy P] [--mc-greedy-worlds W]\n"
                     "                   [--planner-lab N [--every K]]\n"
                     "  --games N            games to play (default 1000)\n"
                     "  --agent A            random, biased, greedy, planner or flatmc (default biased)\n"
                     "  --seed S             first game's seed; game i uses S + i (default 1)\n"
                     "  --no-check           skip the invariant check after every action (faster)\n"
                     "  --print              play one game with seed S and print every action\n"
                     "  --save-replay FILE   with --print: also save that game as a replay\n"
                     "  --replay FILE        print a saved replay instead of playing\n"
                     "  --as 0|1|all         with --print or --replay: show what that player saw (default all)\n"
                     "  --check-replays      save every game as a replay, load it and play it back, checking it\n"
                     "                       ends in exactly the same state (slower)\n"
                     "  --agents A,B         play agent A against agent B on every deck pairing, each seed once with\n"
                     "                       A in each seat; --games is then the number of games per pairing\n"
                     "  --threads N          with --agents: games played at once (default: one per hardware thread)\n"
                     "  --save-replays K     with --agents: save every Kth game to replays/arena/\n"
                     "  --planner-samples M      the planner's sampled worlds for a plan that ends in chance (default 1)\n"
                     "  --planner-or-budget N    the planner's nodes for its win search (default 200)\n"
                     "  --planner-and-budget N   the planner's nodes for its best-plan search (default 300)\n"
                     "                           (the three --planner settings apply to flatmc's searches too)\n"
                     "  --mc-plans K             flat MC's plans played out each decision (default 4)\n"
                     "  --mc-playouts N          flat MC's playouts each decision, split over its plans (default 32)\n"
                     "  --mc-depth R             turns of the searching player a playout runs to (default 1)\n"
                     "  --mc-policy P            greedy or biased: who plays the playouts (default greedy)\n"
                     "  --mc-greedy-worlds W     worlds for a greedy playout's decisions (default 1)\n"
                     "  --planner-lab N          play N greedy games and measure the planner's searches at every Kth\n"
                     "                           main-phase decision (--every K, default 5)\n"
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
            } else if (arg == "--agents" && has_value) {
                std::stringstream names(argv[++i]);
                for (std::string name; std::getline(names, name, ',');) {
                    options.agents.push_back(name);
                }
            } else if (arg == "--threads" && has_value) {
                options.threads = std::stoi(argv[++i]);
            } else if (arg == "--save-replays" && has_value) {
                options.replay_every = std::stoi(argv[++i]);
            } else if (arg == "--planner-samples" && has_value) {
                options.planner.samples = std::stoi(argv[++i]);
            } else if (arg == "--planner-or-budget" && has_value) {
                options.planner.or_budget = std::stoi(argv[++i]);
            } else if (arg == "--planner-and-budget" && has_value) {
                options.planner.and_budget = std::stoi(argv[++i]);
            } else if (arg == "--mc-plans" && has_value) {
                options.flat_mc.plans = std::stoi(argv[++i]);
            } else if (arg == "--mc-playouts" && has_value) {
                options.flat_mc.playouts = std::stoi(argv[++i]);
            } else if (arg == "--mc-depth" && has_value) {
                options.flat_mc.playout.depth = std::stoi(argv[++i]);
            } else if (arg == "--mc-policy" && has_value) {
                try {
                    options.flat_mc.playout.policy = playout_policy(argv[++i]);
                } catch (const std::invalid_argument&) {
                    return false;
                }
            } else if (arg == "--mc-greedy-worlds" && has_value) {
                options.flat_mc.playout.greedy_worlds = std::stoi(argv[++i]);
            } else if (arg == "--planner-lab" && has_value) {
                options.lab_games = std::stoi(argv[++i]);
            } else if (arg == "--every" && has_value) {
                options.every = std::stoi(argv[++i]);
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
        const auto known = [](const std::string& name) {
            return name == "random" || name == "biased" || name == "greedy" || name == "planner" || name == "flatmc";
        };
        if (options.planner.samples < 1 || options.planner.or_budget < 1 || options.planner.and_budget < 1 || options.every < 1) {
            return false;
        }
        if (options.flat_mc.plans < 1 || options.flat_mc.playouts < 1 || options.flat_mc.playout.depth < 0 || options.flat_mc.playout.greedy_worlds < 1) {
            return false;
        }
        options.flat_mc.or_budget = options.planner.or_budget;
        options.flat_mc.and_budget = options.planner.and_budget;
        options.flat_mc.samples = options.planner.samples;
        if (options.lab_games < 0 || (options.lab_games > 0 && (!options.agents.empty() || options.print || options.check_replays))) {
            return false;
        }
        if (options.agents.empty()) {
            if (options.threads != 0 || options.replay_every != 0) {
                return false;
            }
        } else if (options.agents.size() != 2 || !known(options.agents[0]) || !known(options.agents[1]) || options.print ||
                   options.check_replays || options.threads < 0 || options.replay_every < 0) {
            return false;
        }
        return options.games > 0 && known(options.agent);
    }

    std::unique_ptr<Agent> make_agent(const std::string& name, std::uint64_t seed, const Options& options) {
        if (name == "random") {
            return std::make_unique<RandomAgent>(seed);
        }
        if (name == "greedy") {
            return std::make_unique<GreedyAgent>(seed);
        }
        if (name == "planner") {
            return std::make_unique<PlannerAgent>(seed, options.planner);
        }
        if (name == "flatmc") {
            return std::make_unique<FlatMcAgent>(seed, *options.model, options.flat_mc);
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
        const auto agent0 = make_agent(options.agent, seed * 2, options);
        const auto agent1 = make_agent(options.agent, seed * 2 + 1, options);
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

    std::string percent_text(double rate) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(1) << 100.0 * rate << '%';
        return out.str();
    }

    // "96.9% (95% CI 96.6% to 97.2%)"
    std::string score_text(const Tally& tally) {
        const Interval ci = score_interval(tally);
        return percent_text(tally.score()) + " (95% CI " + percent_text(ci.low) + " to " + percent_text(ci.high) + ")";
    }

    int run_lab(const CardDB& db, const std::vector<std::filesystem::path>& files, const std::vector<Decklist>& decks, const Options& options) {
        std::vector<ArenaDeck> arena_decks;
        for (std::size_t i = 0; i < files.size(); ++i) {
            arena_decks.push_back(ArenaDeck{files[i].filename().string(), decks[i]});
        }
        PlannerLabOptions lab;
        lab.games = options.lab_games;
        lab.seed = options.seed;
        lab.every = options.every;
        lab.or_budget = options.planner.or_budget;
        lab.samples = options.planner.samples;
        const auto started = std::chrono::steady_clock::now();
        lab.on_progress = [started](long long done, long long total) {
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            const double left = seconds * static_cast<double>(total - done) / static_cast<double>(done);
            std::cerr << "  " << done << " of " << total << " games, " << std::fixed << std::setprecision(0) << seconds << " s, about "
                      << left << " s left\n";
        };
        const PlannerLabResult r = run_planner_lab(db, arena_decks, lab);

        const auto per = [&](long long total) {
            return r.finished == 0 ? 0.0 : static_cast<double>(total) / static_cast<double>(r.finished);
        };
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "Planner lab: " << r.games << " greedy games, one in every " << lab.every << " main-phase decisions: " << r.positions
                  << " positions, " << r.finished << " searched to the end (" << lab.limit << " nodes at most)\n";
        std::cout << "Win this turn (or-tree): " << r.wins << " positions have one; the planner's " << lab.or_budget << "-node search found "
                  << r.wins_found << " (" << percent(r.wins_found, r.wins) << "%); " << r.false_wins << " false wins\n";
        if (r.wins > 0) {
            std::cout << "  nodes to reach a win: " << static_cast<double>(r.win_nodes) / static_cast<double>(r.wins) << " on average, "
                      << r.win_nodes_max << " at most\n";
        }
        std::cout << "  nodes per position: " << per(r.or_closed) << " with the closed set, " << per(r.or_open) << " without\n";
        std::cout << "Best plan (and-tree), nodes per position:\n";
        std::cout << "  closed set and f_bound  " << std::setw(10) << per(r.and_both) << "  (" << per(r.pruned) << " children pruned)\n";
        std::cout << "  closed set only         " << std::setw(10) << per(r.and_closed) << '\n';
        std::cout << "  f_bound only            " << std::setw(10) << per(r.and_bound) << '\n';
        std::cout << "  neither                 " << std::setw(10) << per(r.and_neither) << '\n';
        std::cout << "  f_bound changed the best plan's value in " << r.bound_changed << " of " << r.finished << " positions\n";
        std::cout << "Took " << r.seconds << " s\n";
        return r.false_wins == 0 ? 0 : 1;
    }

    // --agents A,B: A against B on every deck pairing, with win rates and their confidence intervals.
    int run_match(const CardDB& db, const std::vector<std::filesystem::path>& files, const std::vector<Decklist>& decks,
                  const Options& options) {
        std::vector<ArenaDeck> arena_decks;
        for (std::size_t i = 0; i < files.size(); ++i) {
            arena_decks.push_back(ArenaDeck{files[i].filename().string(), decks[i]});
        }
        // The same agent on both sides is labelled with its side, such as random-A and random-B.
        const bool same = options.agents[0] == options.agents[1];
        const auto contestant = [&](const std::string& name, const char* side) {
            return Contestant{same ? name + "-" + side : name, [name, &options](std::uint64_t seed) { return make_agent(name, seed, options); }};
        };
        const Contestant a = contestant(options.agents[0], "A");
        const Contestant b = contestant(options.agents[1], "B");
        ArenaOptions arena;
        arena.games = static_cast<int>(options.games);
        arena.seed = options.seed;
        arena.threads = options.threads;
        arena.check_invariants = options.check;
        arena.replay_every = options.replay_every;
        const auto started = std::chrono::steady_clock::now();
        arena.on_progress = [started](long long done, long long total) {
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            const double left = seconds * static_cast<double>(total - done) / static_cast<double>(done);
            std::cerr << "  " << done << " of " << total << " games, " << std::fixed << std::setprecision(0) << seconds << " s, about "
                      << left << " s left\n";
        };

        ArenaResult result;
        try {
            result = run_arena(db, arena_decks, a, b, arena);
        } catch (const ArenaError& e) {
            const std::string path = "replays/invariant-seed-" + std::to_string(e.replay.seed) + ".json";
            save_replay(e.replay, path);
            std::cerr << "saved the game as a replay: " << path << '\n';
            throw;
        }

        const std::size_t n = decks.size();
        const long long per_pairing = result.games / static_cast<long long>(n * n);
        std::cout << std::fixed << std::setprecision(1);
        std::cout << a.name << " vs " << b.name << ": " << n << " decks, " << n * n << " pairings, " << per_pairing
                  << " games each (every seed once with " << a.name << " in each seat), " << result.threads << " threads\n";
        if (options.agents[0] == "flatmc" || options.agents[1] == "flatmc") {
            std::cout << "  flatmc: " << options.flat_mc.plans << " plans, " << options.flat_mc.playouts << " playouts, depth "
                      << options.flat_mc.playout.depth << ", " << to_string(options.flat_mc.playout.policy) << " playouts with "
                      << options.flat_mc.playout.greedy_worlds << " world(s)\n";
        }
        if (options.agents[0] == "planner" || options.agents[1] == "planner") {
            std::cout << "  planner: " << options.planner.samples << " sampled worlds, or-tree budget " << options.planner.or_budget
                      << ", and-tree budget " << options.planner.and_budget << '\n';
        }
        std::cout << "Played " << result.games << " games in " << result.seconds << " s: "
                  << static_cast<double>(result.games) / result.seconds << " games/s, "
                  << static_cast<double>(result.actions) / result.seconds / 1e6 << " million actions/s, "
                  << static_cast<double>(result.turns) / static_cast<double>(result.games) << " turns and "
                  << static_cast<double>(result.actions) / static_cast<double>(result.games) << " actions per game\n";
        std::cout << a.name << "'s score: " << score_text(result.overall) << ": " << result.overall.wins << " won, "
                  << result.overall.losses << " lost, " << result.overall.draws << " drawn\n";
        std::cout << "  " << a.name << " went first:  " << score_text(result.a_first) << " over " << result.a_first.games() << " games\n";
        std::cout << "  " << b.name << " went first:  " << score_text(result.b_first) << " over " << result.b_first.games() << " games\n";
        std::cout << "By deck pairing (" << a.name << "'s deck against " << b.name << "'s):\n";
        for (const PairingResult& pairing : result.pairings) {
            std::cout << "  " << std::setw(16) << std::left << files[pairing.deck_a].stem().string() << std::setw(16)
                      << files[pairing.deck_b].stem().string() << std::right << std::setw(7) << pairing.tally.games() << "  "
                      << score_text(pairing.tally) << '\n';
        }
        std::cout << "Ended by:\n";
        for (std::size_t end = 0; end < result.ends.size(); ++end) {
            std::cout << "  " << std::setw(16) << std::left << to_string(static_cast<GameEnd>(end)) << std::right
                      << std::setw(8) << result.ends[end] << "  (" << percent(result.ends[end], result.games) << "%)\n";
        }
        // Prize cards and no Pokemon left are the winner's doing, deck-out the loser's.
        std::cout << "How " << a.name << " won and lost:\n";
        std::cout << "  " << std::setw(16) << std::left << "" << std::right << std::setw(8) << "won" << std::setw(8) << "lost" << '\n';
        for (const GameEnd end : {GameEnd::PrizeCards, GameEnd::NoPokemon, GameEnd::DeckOut}) {
            const auto i = static_cast<std::size_t>(end);
            std::cout << "  " << std::setw(16) << std::left << to_string(end) << std::right << std::setw(8) << result.a_wins_by[i]
                      << std::setw(8) << result.a_losses_by[i] << '\n';
        }
        std::cout << "  (won by deck-out: " << b.name << "'s deck ran out; lost by deck-out: " << a.name << "'s did)\n";
        const auto print_counters = [&](const std::string& name, const std::vector<std::pair<std::string, double>>& counters) {
            if (counters.empty()) {
                return;
            }
            std::cout << name << " per game:" << std::setprecision(2);
            const char* separator = " ";
            for (const auto& [label, total] : counters) {
                std::cout << separator << label << ' ' << total / static_cast<double>(result.games);
                separator = ", ";
            }
            std::cout << std::setprecision(1) << '\n';
        };
        print_counters(a.name, result.a_counters);
        print_counters(b.name, result.b_counters);
        std::cout << (options.check ? "Invariants checked after every action: no problems found\n"
                                    : "Invariant checks were off (--no-check)\n");
        for (std::size_t i = 0; i < result.replays.size(); ++i) {
            const Replay& replay = result.replays[i];
            std::ostringstream path;
            path << "replays/arena/" << std::setw(5) << std::setfill('0') << i << "-seed-" << replay.seed << '-'
                 << replay.players[0].agent << '-' << std::filesystem::path(replay.players[0].deck_name).stem().string() << "-vs-"
                 << replay.players[1].agent << '-' << std::filesystem::path(replay.players[1].deck_name).stem().string() << ".json";
            save_replay(replay, path.str());
        }
        if (!result.replays.empty()) {
            std::cout << "Saved " << result.replays.size() << " replays to replays/arena/\n";
        }
        return result.ends[static_cast<std::size_t>(GameEnd::ActionLimit)] == 0 ? 0 : 1;
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
        std::unique_ptr<ArchetypeLibrary> library;
        std::unique_ptr<DeckModel> model;
        const bool flat_mc = options.agent == "flatmc" || std::find(options.agents.begin(), options.agents.end(), "flatmc") != options.agents.end();
        if (flat_mc) {
            library = std::make_unique<ArchetypeLibrary>(ArchetypeLibrary::load("data/archetypes", db));
            model = std::make_unique<DeckModel>(*library);
            options.model = model.get();
        }
        if (!options.agents.empty()) {
            return run_match(db, files, decks, options);
        }
        if (options.lab_games > 0) {
            return run_lab(db, files, decks, options);
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
