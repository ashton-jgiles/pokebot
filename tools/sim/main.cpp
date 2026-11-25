#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "agents/endgame/endgame_agent.hpp"
#include "agents/evaluation/weights.hpp"
#include "agents/flat_mc/deck_model.hpp"
#include "agents/flat_mc/flat_mc_agent.hpp"
#include "agents/flat_mc/playout.hpp"
#include "agents/greedy/greedy_agent.hpp"
#include "agents/is_mcts/is_mcts_agent.hpp"
#include "agents/planner/planner_agent.hpp"
#include "agents/random/random_agent.hpp"
#include "agents/set_weights.hpp"
#include "cards/db/card_db.hpp"
#include "core/decklist.hpp"
#include "harness/arena.hpp"
#include "harness/budget_lab.hpp"
#include "harness/endgame_lab.hpp"
#include "harness/evaluation_lab.hpp"
#include "harness/match.hpp"
#include "harness/planner_lab.hpp"
#include "harness/replay.hpp"
#include "harness/self_play.hpp"

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
        IsMctsAgentOptions is_mcts;
        EndgameAgentOptions endgame;
        Weights weights = default_weights();
        std::string save_games;
        std::string check_games;
        std::vector<std::filesystem::path> evaluate;
        std::string save_weights_to;
        std::string reference;
        std::vector<std::filesystem::path> score_weights;
        std::string weights_file;
        std::string weights_a_file;
        std::string weights_b_file;
        std::string playout_weights_a_file;
        std::string playout_weights_b_file;
        std::optional<double> mcts_c_a;
        std::optional<double> mcts_c_b;
        const DeckModel* model = nullptr;
        int lab_games = 0;
        int budget_lab_games = 0;
        int endgame_lab_games = 0;
        int endgame_prizes = 2;
        int endgame_worlds = 4;
        int endgame_depth = 2;
        int every = 5;
    };

    void print_usage() {
        std::cerr << "usage: pokebot-sim [--games N] [--agent random|biased|greedy|planner|flatmc|ismcts|endgame] [--seed S] [--no-check]\n"
                     "                   [--print [--save-replay FILE]] [--replay FILE] [--as 0|1|all]\n"
                     "                   [--agents A,B [--threads N] [--save-replays K] [--save-games FILE]]\n"
                     "                   [--check-games FILE] [--evaluate FILE,FILE,... [--threads N] [--save-weights DIR] [--reference NAME]\n"
                     "                    [--score-weights FILE,FILE,...]]\n"
                     "                   [--planner-samples M] [--planner-or-budget N] [--planner-and-budget N]\n"
                     "                   [--mc-plans K] [--mc-playouts N] [--mc-depth R] [--mc-policy P] [--mc-greedy-worlds W] [--mc-greedy-choices]\n"
                     "                   [--mcts-iterations N] [--mcts-c C] [--mcts-c-a C] [--mcts-c-b C] [--mcts-depth R] [--mcts-plans K]\n"
                     "                   [--mcts-opponent-budget B] [--mcts-opponent-plans K] [--mcts-fresh-worlds]\n"
                     "                   [--mcts-final-mean [--mcts-min-visits N]]\n"
                     "                   [--weights FILE] [--weights-a FILE] [--weights-b FILE]\n"
                     "                   [--playout-weights-a FILE] [--playout-weights-b FILE]\n"
                     "                   [--planner-lab N [--every K]] [--budget-lab N [--every K]]\n"
                     "                   [--endgame-prizes P] [--endgame-worlds M] [--endgame-depth D] [--endgame-nodes N]\n"
                     "                   [--endgame-min-depth D]\n"
                     "                   [--endgame-lab N [--every K]]\n"
                     "  --games N            games to play (default 1000)\n"
                     "  --agent A            random, biased, greedy, planner, flatmc, ismcts or endgame (default biased)\n"
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
                     "  --threads N          with --agents or --evaluate: games at once (default: one per hardware thread)\n"
                     "  --save-weights DIR   with --evaluate: save every fitted model's weights to DIR; no file may exist\n"
                     "  --reference NAME     with --evaluate: the model the others are compared with (default hand-scaled-terms)\n"
                     "  --score-weights FILE,...  with --evaluate: score the weights in these files as they are, instead of the usual\n"
                     "                       models, against the first\n"
                     "  --save-replays K     with --agents: save every Kth game to replays/arena/\n"
                     "  --save-games FILE    with --agents: save every game to FILE, one a line; FILE must not exist\n"
                     "  --check-games FILE   play back every game in FILE and count its positions\n"
                     "  --evaluate FILE,...  measure how well the evaluation predicts the results of the saved games in the files\n"
                     "  --planner-samples M      the planner's sampled worlds for a plan that ends in chance (default 1)\n"
                     "  --planner-or-budget N    the planner's nodes for its win search (default 200)\n"
                     "  --planner-and-budget N   the planner's nodes for its best-plan search (default 300)\n"
                     "                           (the three --planner settings apply to flatmc's searches too)\n"
                     "  --mc-plans K             flat MC's plans played out each decision (default 16)\n"
                     "  --mc-playouts N          flat MC's playouts each decision, split over its plans (default 64)\n"
                     "  --mc-depth R             turns of the searching player a playout runs to (default 2)\n"
                     "  --mc-policy P            greedy or biased: who plays the playouts (default greedy)\n"
                     "  --mc-greedy-worlds W     worlds for a greedy playout's decisions (default 1)\n"
                     "  --mc-greedy-choices      flat MC leaves its decisions outside the main phase to Greedy (default: plays them out)\n"
                     "  --mcts-iterations N      IS-MCTS's iterations each decision (default 42)\n"
                     "  --mcts-c C               IS-MCTS's exploration constant (default 0.7)\n"
                     "  --mcts-c-a C             with --agents A,B: A's exploration constant (default: --mcts-c's)\n"
                     "  --mcts-c-b C             with --agents A,B: B's exploration constant (default: --mcts-c's)\n"
                     "  --mcts-depth R           turns of the searching player an IS-MCTS playout runs to (default 2)\n"
                     "  --mcts-plans K           IS-MCTS's plans at the searcher's nodes (default 16)\n"
                     "  --mcts-opponent-budget B nodes for the opponent's and-tree at an IS-MCTS node (default 25)\n"
                     "  --mcts-opponent-plans K  plans at the opponent's IS-MCTS nodes (default 4)\n"
                     "  --mcts-fresh-worlds      deal every iteration a fresh world, not the k-th world shared by every root move\n"
                     "  --mcts-final-mean        play the root move with the best mean among those visited often enough\n"
                     "  --mcts-min-visits N      with --mcts-final-mean: visits a root move needs (default 3)\n"
                     "  --weights FILE           the evaluation's weights for every agent (default: the hand-set ones)\n"
                     "  --weights-a FILE         with --agents A,B: A's weights\n"
                     "  --weights-b FILE         with --agents A,B: B's weights\n"
                     "  --playout-weights-a FILE with --agents A,B: the weights A's playouts are played with (default: A's weights)\n"
                     "  --playout-weights-b FILE with --agents A,B: the weights B's playouts are played with (default: B's weights)\n"
                     "  --planner-lab N          play N greedy games and measure the planner's searches at every Kth\n"
                     "                           main-phase decision (--every K, default 5)\n"
                     "  --budget-lab N           play N greedy games and time the and-tree at cheap budgets against\n"
                     "                           --planner-and-budget, at every Kth main-phase decision\n"
                     "  --endgame-prizes P       the endgame agent solves when either player has at most P Prize cards\n"
                     "                           left (default 2)\n"
                     "  --endgame-worlds M       the endgame solver's worlds (default 4)\n"
                     "  --endgame-depth D        the endgame solver's deepest depth (default 2)\n"
                     "  --endgame-nodes N        the endgame solver's node budget, 0 for none (default 10000)\n"
                     "  --endgame-min-depth D    the endgame agent plays the solver's plan only if the solve reached\n"
                     "                           depth D, else IS-MCTS decides (default 2)\n"
                     "  --endgame-lab N          play N greedy games, count the hidden-card worlds at every main-phase\n"
                     "                           decision and, at every Kth one with at most P prizes left\n"
                     "                           (--endgame-prizes P, default 2), time a two-segment search and the\n"
                     "                           endgame solver over M worlds to depth D (--endgame-worlds M, default 4;\n"
                     "                           --endgame-depth D, default 2)\n"
                     "If an invariant breaks, the game is saved to replays/invariant-seed-S.json; with --agents, a game\n"
                     "that fails for any reason (a broken invariant or an agent's error) is saved to replays/failed-seed-S.json.\n";
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
            } else if (arg == "--save-games" && has_value) {
                options.save_games = argv[++i];
            } else if (arg == "--check-games" && has_value) {
                options.check_games = argv[++i];
            } else if (arg == "--evaluate" && has_value) {
                std::stringstream names(argv[++i]);
                for (std::string name; std::getline(names, name, ',');) {
                    options.evaluate.emplace_back(name);
                }
            } else if (arg == "--save-weights" && has_value) {
                options.save_weights_to = argv[++i];
            } else if (arg == "--reference" && has_value) {
                options.reference = argv[++i];
            } else if (arg == "--score-weights" && has_value) {
                std::stringstream names(argv[++i]);
                for (std::string name; std::getline(names, name, ',');) {
                    options.score_weights.emplace_back(name);
                }
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
            } else if (arg == "--mc-greedy-choices") {
                options.flat_mc.search_choices = false;
            } else if (arg == "--mcts-iterations" && has_value) {
                options.is_mcts.search.iterations = std::stoi(argv[++i]);
            } else if (arg == "--mcts-c" && has_value) {
                options.is_mcts.search.exploration = std::stod(argv[++i]);
            } else if (arg == "--mcts-c-a" && has_value) {
                options.mcts_c_a = std::stod(argv[++i]);
            } else if (arg == "--mcts-c-b" && has_value) {
                options.mcts_c_b = std::stod(argv[++i]);
            } else if (arg == "--mcts-depth" && has_value) {
                options.is_mcts.search.playout.depth = std::stoi(argv[++i]);
            } else if (arg == "--mcts-plans" && has_value) {
                options.is_mcts.search.macros.searcher_plans = std::stoi(argv[++i]);
            } else if (arg == "--mcts-opponent-budget" && has_value) {
                options.is_mcts.search.macros.opponent_budget = std::stoi(argv[++i]);
            } else if (arg == "--mcts-opponent-plans" && has_value) {
                options.is_mcts.search.macros.opponent_plans = std::stoi(argv[++i]);
            } else if (arg == "--mcts-fresh-worlds") {
                options.is_mcts.search.matched_worlds = false;
            } else if (arg == "--mcts-final-mean") {
                options.is_mcts.search.final_by_mean = true;
            } else if (arg == "--mcts-min-visits" && has_value) {
                options.is_mcts.search.final_min_visits = std::stoi(argv[++i]);
            } else if (arg == "--budget-lab" && has_value) {
                options.budget_lab_games = std::stoi(argv[++i]);
            } else if (arg == "--endgame-lab" && has_value) {
                options.endgame_lab_games = std::stoi(argv[++i]);
            } else if (arg == "--endgame-prizes" && has_value) {
                options.endgame_prizes = std::stoi(argv[++i]);
            } else if (arg == "--endgame-worlds" && has_value) {
                options.endgame_worlds = std::stoi(argv[++i]);
            } else if (arg == "--endgame-depth" && has_value) {
                options.endgame_depth = std::stoi(argv[++i]);
            } else if (arg == "--endgame-min-depth" && has_value) {
                options.endgame.min_depth = std::stoi(argv[++i]);
            } else if (arg == "--endgame-nodes" && has_value) {
                options.endgame.solver.max_nodes = std::stoll(argv[++i]);
            } else if (arg == "--weights" && has_value) {
                options.weights_file = argv[++i];
            } else if (arg == "--playout-weights-a" && has_value) {
                options.playout_weights_a_file = argv[++i];
            } else if (arg == "--playout-weights-b" && has_value) {
                options.playout_weights_b_file = argv[++i];
            } else if (arg == "--weights-a" && has_value) {
                options.weights_a_file = argv[++i];
            } else if (arg == "--weights-b" && has_value) {
                options.weights_b_file = argv[++i];
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
            return name == "random" || name == "biased" || name == "greedy" || name == "planner" || name == "flatmc" || name == "ismcts" || name == "endgame";
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
        const IsMctsOptions& mcts = options.is_mcts.search;
        if (mcts.iterations < 1 || mcts.exploration < 0.0 || mcts.playout.depth < 0 || mcts.macros.searcher_plans < 1 || mcts.macros.opponent_budget < 1 ||
            mcts.macros.opponent_plans < 1 || mcts.final_min_visits < 1 || options.mcts_c_a.value_or(0.0) < 0.0 || options.mcts_c_b.value_or(0.0) < 0.0) {
            return false;
        }
        options.is_mcts.or_budget = options.planner.or_budget;
        options.is_mcts.search.macros.searcher_budget = options.planner.and_budget;
        options.is_mcts.search.macros.samples = options.planner.samples;
        if (options.endgame.solver.max_nodes < 0 || options.endgame.min_depth < 0) {
            return false;
        }
        options.endgame.prizes = options.endgame_prizes;
        options.endgame.is_mcts = options.is_mcts;
        options.endgame.solver.worlds = options.endgame_worlds;
        options.endgame.solver.max_depth = options.endgame_depth;
        options.endgame.solver.search.root_budget = options.planner.and_budget;
        options.endgame.solver.search.root_plans = options.is_mcts.search.macros.searcher_plans;
        options.endgame.solver.search.budget = options.is_mcts.search.macros.opponent_budget;
        options.endgame.solver.search.plans = options.is_mcts.search.macros.opponent_plans;
        options.endgame.solver.search.samples = options.planner.samples;
        if (options.lab_games < 0 || (options.lab_games > 0 && (!options.agents.empty() || options.print || options.check_replays))) {
            return false;
        }
        if (options.budget_lab_games < 0 ||
            (options.budget_lab_games > 0 && (!options.agents.empty() || options.print || options.check_replays || options.lab_games > 0))) {
            return false;
        }
        if (options.endgame_lab_games < 0 || options.endgame_prizes < 1 || options.endgame_prizes > 6 || options.endgame_worlds < 1 ||
            options.endgame_depth < 1 ||
            (options.endgame_lab_games > 0 &&
             (!options.agents.empty() || options.print || options.check_replays || options.lab_games > 0 || options.budget_lab_games > 0))) {
            return false;
        }
        if (!options.evaluate.empty() && (!options.agents.empty() || options.print || options.check_replays || !options.check_games.empty() ||
                                          options.lab_games > 0 || options.budget_lab_games > 0 || options.endgame_lab_games > 0 || options.threads < 0)) {
            return false;
        }
        if (options.evaluate.empty() && (!options.save_weights_to.empty() || !options.reference.empty() || !options.score_weights.empty())) {
            return false;
        }
        if (!options.score_weights.empty() && (!options.reference.empty() || !options.save_weights_to.empty())) {
            return false;
        }
        const bool lab = options.lab_games > 0 || options.budget_lab_games > 0 || options.endgame_lab_games > 0 || !options.evaluate.empty();
        if (lab && !options.weights_file.empty()) {
            return false;
        }
        if (options.agents.empty()) {
            if ((options.threads != 0 && options.evaluate.empty()) || options.replay_every != 0 || !options.weights_a_file.empty() || !options.weights_b_file.empty() ||
                !options.save_games.empty() || options.mcts_c_a || options.mcts_c_b || !options.playout_weights_a_file.empty() ||
                !options.playout_weights_b_file.empty()) {
                return false;
            }
        } else if (options.agents.size() != 2 || !known(options.agents[0]) || !known(options.agents[1]) || options.print ||
                   options.check_replays || options.threads < 0 || options.replay_every < 0 || (!options.save_games.empty() && options.replay_every > 0) ||
                   !options.check_games.empty()) {
            return false;
        }
        return options.games > 0 && known(options.agent);
    }

    void use_weights(Options& options, const Weights& weights) {
        options.weights = weights;
        set_weights(options.planner, weights);
        set_weights(options.flat_mc, weights);
        set_weights(options.is_mcts, weights);
        set_weights(options.endgame, weights);
    }

    std::unique_ptr<Agent> make_agent(const std::string& name, std::uint64_t seed, const Options& options) {
        if (name == "random") {
            return std::make_unique<RandomAgent>(seed);
        }
        if (name == "greedy") {
            return std::make_unique<GreedyAgent>(seed, 3, options.weights);
        }
        if (name == "planner") {
            return std::make_unique<PlannerAgent>(seed, options.planner);
        }
        if (name == "flatmc") {
            return std::make_unique<FlatMcAgent>(seed, *options.model, options.flat_mc);
        }
        if (name == "ismcts") {
            return std::make_unique<IsMctsAgent>(seed, *options.model, options.is_mcts);
        }
        if (name == "endgame") {
            return std::make_unique<EndgameAgent>(seed, *options.model, options.endgame);
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

    int run_budget_lab_command(const CardDB& db, const std::vector<std::filesystem::path>& files, const std::vector<Decklist>& decks, const Options& options) {
        std::vector<ArenaDeck> arena_decks;
        for (std::size_t i = 0; i < files.size(); ++i) {
            arena_decks.push_back(ArenaDeck{files[i].filename().string(), decks[i]});
        }
        BudgetLabOptions lab;
        lab.games = options.budget_lab_games;
        lab.seed = options.seed;
        lab.every = options.every;
        lab.reference = options.planner.and_budget;
        lab.samples = options.planner.samples;
        const auto started = std::chrono::steady_clock::now();
        lab.on_progress = [started](long long done, long long total) {
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            std::cerr << "  " << done << " of " << total << " games, " << std::fixed << std::setprecision(0) << seconds << " s\n";
        };
        const BudgetLabResult r = run_budget_lab(db, arena_decks, lab);
        const auto line = [&](const BudgetRow& row) {
            const double searches = static_cast<double>(std::max(row.searches, 1LL));
            const double compared = static_cast<double>(std::max(r.reference.same_best, 1LL));
            std::cout << "  " << std::setw(6) << row.budget << std::setw(10) << std::setprecision(2) << 1000.0 * row.seconds / searches << " ms"
                      << std::setw(9) << std::setprecision(1) << 100.0 * static_cast<double>(row.same_best) / compared << "%" << std::setw(12)
                      << std::setprecision(3) << row.gap / compared << std::setw(9) << std::setprecision(2) << static_cast<double>(row.plans) / searches
                      << '\n';
        };
        std::cout << std::fixed << "Budget lab: " << r.games << " greedy games, one in every " << lab.every << " main-phase decisions: " << r.positions
                  << " positions, top " << lab.plans << " plans each\n";
        std::cout << "  budget  time/search  same best  value lost    plans\n";
        line(r.reference);
        for (const BudgetRow& row : r.rows) {
            line(row);
        }
        std::cout << std::setprecision(1) << "Took " << r.seconds << " s\n";
        return 0;
    }

    int run_endgame_lab_command(const CardDB& db, const std::vector<std::filesystem::path>& files, const std::vector<Decklist>& decks, const Options& options) {
        std::vector<ArenaDeck> arena_decks;
        for (std::size_t i = 0; i < files.size(); ++i) {
            arena_decks.push_back(ArenaDeck{files[i].filename().string(), decks[i]});
        }
        EndgameLabOptions lab;
        lab.games = options.endgame_lab_games;
        lab.seed = options.seed;
        lab.every = options.every;
        lab.tree_prizes = options.endgame_prizes;
        lab.solver_worlds = options.endgame_worlds;
        lab.solver_depth = options.endgame_depth;
        lab.and_budget = options.planner.and_budget;
        lab.samples = options.planner.samples;
        lab.plans = options.is_mcts.search.macros.searcher_plans;
        lab.opponent_budget = options.is_mcts.search.macros.opponent_budget;
        lab.opponent_plans = options.is_mcts.search.macros.opponent_plans;
        lab.model = options.model;
        const auto started = std::chrono::steady_clock::now();
        lab.on_progress = [started](long long done, long long total) {
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            std::cerr << "  " << done << " of " << total << " games, " << std::fixed << std::setprecision(0) << seconds << " s\n";
        };
        const EndgameLabResult r = run_endgame_lab(db, arena_decks, lab);
        const double games = static_cast<double>(std::max(r.games, 1LL));
        std::cout << std::fixed << std::setprecision(1) << "Endgame lab: " << r.games << " greedy games, " << r.positions.size() << " main-phase decisions\n";
        std::cout << "  games whose smaller prize count got down to 1, 2, 3, 4, 5, 6:";
        for (std::size_t k = 1; k < r.lowest_prizes.size(); ++k) {
            std::cout << ' ' << r.lowest_prizes[k];
        }
        std::cout << "\nWorlds at each decision by the smaller prize count (median log10, counted by card, with the opponent's true list):\n";
        std::cout << "  prizes  decisions/game   own  opponent  total   <=1e2   <=1e4   <=1e6   <=1e8  hidden hand    <=2  deck cards  lists\n";
        for (int p = 1; p <= 6; ++p) {
            const EndgameSummary s = summarize(r, p);
            if (s.positions == 0) {
                continue;
            }
            std::cout << std::setw(8) << p << std::setw(16) << static_cast<double>(s.positions) / games << std::setw(6) << s.median_own << std::setw(10)
                      << s.median_opponent << std::setw(7) << s.median_total;
            for (const double share : s.total_at_most) {
                std::cout << std::setw(7) << share << '%';
            }
            std::cout << std::setw(13) << s.median_hidden_hand << std::setw(6) << s.hidden_hand_at_most_2 << '%' << std::setw(7) << s.median_own_deck_cards
                      << " / " << s.median_opponent_deck_cards << std::setw(7) << s.median_lists << '\n';
        }
        std::cout << "Two-segment search in the true world (the searcher's " << lab.plans << " plans, a branch for each card the opponent could draw, its "
                  << lab.opponent_plans << " replies), no pruning, one in every " << lab.every << " decisions, averages a tree:\n";
        std::cout << "  prizes  trees  plans  mid-turn  (1 / 2-3 / 4+ drawn)  random  draws  replies  leaves  mean s  median s  p90 s\n";
        for (int p = 1; p <= lab.tree_prizes; ++p) {
            const EndgameSummary s = summarize(r, p);
            if (s.trees == 0) {
                continue;
            }
            const EndgameTree& t = s.total;
            const auto per = [&](long long value) {
                return static_cast<double>(value) / static_cast<double>(s.trees);
            };
            std::cout << std::setprecision(1) << std::setw(8) << p << std::setw(7) << s.trees << std::setw(7) << per(t.plans) << std::setw(10) << per(t.mid_turn)
                      << "   (" << per(t.mid_turn_draws[0]) << " / " << per(t.mid_turn_draws[1]) << " / " << per(t.mid_turn_draws[2]) << ")" << std::setw(11)
                      << per(t.random) << std::setw(7) << per(t.chance) << std::setw(9) << per(t.opponent_plans) << std::setw(8) << per(t.leaves)
                      << std::setprecision(3) << std::setw(8) << t.seconds / static_cast<double>(s.trees) << std::setw(10) << s.median_seconds << std::setw(7)
                      << s.p90_seconds << '\n';
        }
        std::cout << "Endgame solver at the same decisions (" << lab.solver_worlds << " worlds from the deck model, alpha-beta and Star1, iterative deepening to depth "
                  << lab.solver_depth << ", no node budget), averages a solve:\n";
        std::cout << "  prizes  solves  depth 0/1/2+  nodes  chance nodes  alpha-beta cuts  Star1 cuts  and-trees  plans dropped  mean s  median s  p90 s\n";
        for (int p = 1; p <= lab.tree_prizes; ++p) {
            const EndgameSummary s = summarize(r, p);
            if (s.trees == 0) {
                continue;
            }
            const EndgameSolve& v = s.solved;
            const auto per = [&](long long value) {
                return static_cast<double>(value) / static_cast<double>(s.trees);
            };
            std::cout << std::setprecision(1) << std::setw(8) << p << std::setw(8) << s.trees << "   " << s.solve_depths[0] << " / " << s.solve_depths[1] << " / "
                      << s.solve_depths[2] + s.solve_depths[3] << std::setw(9) << per(v.visited) << std::setw(14) << per(v.chance_nodes) << std::setw(17)
                      << per(v.alpha_beta_cuts) << std::setw(12) << per(v.star1_cuts) << std::setw(11) << per(v.and_trees) << std::setw(15) << per(v.dropped)
                      << std::setprecision(3) << std::setw(8) << v.seconds / static_cast<double>(s.trees) << std::setw(10) << s.solve_median_seconds << std::setw(7)
                      << s.solve_p90_seconds << '\n';
        }
        std::cout << std::setprecision(1) << "Took " << r.seconds << " s\n";
        return 0;
    }

    // --agents A,B: A against B on every deck pairing, with win rates and their confidence intervals.
    int run_match(const CardDB& db, const std::vector<std::filesystem::path>& files, const std::vector<Decklist>& decks,
                  const Options& options) {
        if (!options.save_games.empty() && std::filesystem::exists(options.save_games)) {
            throw std::runtime_error(options.save_games + " already exists");
        }
        std::vector<ArenaDeck> arena_decks;
        for (std::size_t i = 0; i < files.size(); ++i) {
            arena_decks.push_back(ArenaDeck{files[i].filename().string(), decks[i]});
        }
        // The same agent on both sides is labelled with its side, such as random-A and random-B.
        const bool same = options.agents[0] == options.agents[1];
        const auto contestant = [&](const std::string& name, const char* side, const std::string& weights_file, const std::string& playout_file,
                                    const std::optional<double>& c) {
            Options mine = options;
            if (!weights_file.empty()) {
                use_weights(mine, load_weights(weights_file));
            }
            if (!playout_file.empty()) {
                const Weights playout = load_weights(playout_file);
                set_playout_weights(mine.flat_mc, playout);
                set_playout_weights(mine.is_mcts, playout);
                set_playout_weights(mine.endgame, playout);
            }
            if (c) {
                mine.is_mcts.search.exploration = *c;
                mine.endgame.is_mcts.search.exploration = *c;
            }
            return Contestant{same ? name + "-" + side : name, [name, mine](std::uint64_t seed) { return make_agent(name, seed, mine); }};
        };
        const Contestant a = contestant(options.agents[0], "A", options.weights_a_file, options.playout_weights_a_file, options.mcts_c_a);
        const Contestant b = contestant(options.agents[1], "B", options.weights_b_file, options.playout_weights_b_file, options.mcts_c_b);
        ArenaOptions arena;
        arena.games = static_cast<int>(options.games);
        arena.seed = options.seed;
        arena.threads = options.threads;
        arena.check_invariants = options.check;
        arena.replay_every = options.save_games.empty() ? options.replay_every : 1;
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
            const std::string path = "replays/failed-seed-" + std::to_string(e.replay.seed) + ".json";
            save_replay(e.replay, path);
            std::cerr << "game with seed " << e.replay.seed << " (" << e.replay.players[0].agent << " with " << e.replay.players[0].deck_name << " against "
                      << e.replay.players[1].agent << " with " << e.replay.players[1].deck_name << ") failed after " << e.replay.actions.size()
                      << " actions; saved it as a replay: " << path << '\n';
            throw;
        }

        const std::size_t n = decks.size();
        const long long per_pairing = result.games / static_cast<long long>(n * n);
        std::cout << std::fixed << std::setprecision(1);
        std::cout << a.name << " vs " << b.name << ": " << n << " decks, " << n * n << " pairings, " << per_pairing
                  << " games each (every seed once with " << a.name << " in each seat), " << result.threads << " threads\n";
        if (options.agents[0] == "ismcts" || options.agents[1] == "ismcts" || options.agents[0] == "endgame" || options.agents[1] == "endgame") {
            const IsMctsOptions& m = options.is_mcts.search;
            std::cout << "  ismcts: " << m.iterations << " iterations, C " << std::setprecision(2) << options.mcts_c_a.value_or(m.exploration) << " for " << a.name
                      << " and " << options.mcts_c_b.value_or(m.exploration) << " for " << b.name << std::setprecision(1) << ", depth " << m.playout.depth << ", " << m.macros.searcher_plans
                      << " plans, opponent and-tree " << m.macros.opponent_budget << " nodes keeping " << m.macros.opponent_plans
                      << (m.matched_worlds ? ", matched worlds" : "") << (m.final_by_mean ? ", final by mean" : "") << '\n';
        }
        if (options.agents[0] == "endgame" || options.agents[1] == "endgame") {
            const EndgameOptions& e = options.endgame.solver;
            std::cout << "  endgame: the solver at " << options.endgame.prizes << " or fewer Prize cards left, " << e.worlds << " worlds, depth up to "
                      << e.max_depth << ", " << e.max_nodes << " nodes, plans " << e.search.root_plans << " at the root and " << e.search.plans
                      << " below, played if it reached depth " << options.endgame.min_depth << "; otherwise ismcts as above\n";
        }
        if (options.agents[0] == "flatmc" || options.agents[1] == "flatmc") {
            std::cout << "  flatmc: " << options.flat_mc.plans << " plans, " << options.flat_mc.playouts << " playouts, depth "
                      << options.flat_mc.playout.depth << ", " << to_string(options.flat_mc.playout.policy) << " playouts with "
                      << options.flat_mc.playout.greedy_worlds << " world(s)" << (options.flat_mc.search_choices ? ", choices played out" : "") << '\n';
        }
        if (options.agents[0] == "planner" || options.agents[1] == "planner") {
            std::cout << "  planner: " << options.planner.samples << " sampled worlds, or-tree budget " << options.planner.or_budget
                      << ", and-tree budget " << options.planner.and_budget << '\n';
        }
        const auto weights_of = [&](const std::string& side_file, const std::string& playout_file) {
            const std::string& file = side_file.empty() ? options.weights_file : side_file;
            return (file.empty() ? std::string("the hand-set ones") : file) + (playout_file.empty() ? "" : " (playouts played with " + playout_file + ")");
        };
        std::cout << "  weights: " << a.name << " " << weights_of(options.weights_a_file, options.playout_weights_a_file) << ", " << b.name << " " << weights_of(options.weights_b_file, options.playout_weights_b_file) << '\n';
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
        if (!options.save_games.empty()) {
            for (Replay& game : result.replays) {
                game.note = a.name + " with " + weights_of(options.weights_a_file, options.playout_weights_a_file) + ", " + b.name + " with " + weights_of(options.weights_b_file, options.playout_weights_b_file);
            }
            save_games(options.save_games, result.replays);
            std::cout << "Saved " << result.replays.size() << " games to " << options.save_games << '\n';
            result.replays.clear();
        }
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

    int run_check_games(const CardDB& db, const std::string& file) {
        const std::vector<Replay> games = load_games(file);
        std::set<std::uint64_t> seeds;
        std::set<std::tuple<std::uint64_t, std::string, std::string>> deals;
        std::set<std::string> notes;
        std::array<long long, 2> rows{};
        std::array<std::array<long long, 7>, 2> by_prizes{};
        long long unfinished = 0;
        double first_player = 0.0;
        for (std::size_t g = 0; g < games.size(); ++g) {
            const Replay& game = games[g];
            seeds.insert(game.seed);
            deals.emplace(game.seed, game.players[0].deck_name, game.players[1].deck_name);
            notes.insert(game.note);
            const std::vector<Position> found = positions(db, game, g);
            if (found.empty()) {
                ++unfinished;
                continue;
            }
            first_player += found.front().went_first ? found.front().result : 1.0 - found.front().result;
            for (const Position& position : found) {
                const std::size_t kind = position.start ? 0 : 1;
                ++rows[kind];
                ++by_prizes[kind][static_cast<std::size_t>(std::min(position.prizes, position.opponent_prizes))];
            }
        }
        if (games.empty()) {
            std::cout << "No games in " << file << '\n';
            return 1;
        }
        const double played = static_cast<double>(std::max<std::size_t>(games.size() - static_cast<std::size_t>(unfinished), 1));
        std::cout << std::fixed << std::setprecision(1) << games.size() << " games, all played back to their saved result; seeds " << *seeds.begin() << " to "
                  << *seeds.rbegin() << " (" << seeds.size() << "), " << deals.size() << " different deals, " << unfinished << " without positions\n";
        for (const std::string& note : notes) {
            std::cout << "  " << note << '\n';
        }
        std::cout << "  the player who went first scored " << 100.0 * first_player / played << "%\n";
        std::cout << "  positions: " << rows[0] << " at a turn's start, " << rows[1] << " later in a turn (" << static_cast<double>(rows[0] + rows[1]) / played
                  << " a game)\n  by the smaller prize count, 0 to 6:";
        for (std::size_t p = 0; p < by_prizes[0].size(); ++p) {
            std::cout << ' ' << by_prizes[0][p] + by_prizes[1][p];
        }
        std::cout << '\n';
        return 0;
    }

    void print_group(const std::string& name, const Group& group) {
        if (group.positions == 0) {
            return;
        }
        std::cout << "    " << std::left << std::setw(22) << name << std::right << std::setw(8) << group.positions << std::setprecision(4) << std::setw(9)
                  << group.loss << std::setprecision(1) << std::setw(9) << 100.0 * group.predicted << "%" << std::setw(8) << 100.0 * group.result << "%"
                  << std::showpos << std::setw(9) << 100.0 * (group.result - group.predicted) << std::noshowpos << " +/- " << 196.0 * group.gap_error << '\n';
    }

    void print_change(double change, double error) {
        std::cout << std::setprecision(4) << std::showpos << std::setw(10) << change << std::noshowpos << " +/- " << 1.96 * error;
    }

    int run_score_weights(const CardDB& db, const Options& options) {
        const int threads = options.threads > 0 ? options.threads : static_cast<int>(std::max(std::thread::hardware_concurrency(), 1U));
        std::vector<Model> models;
        for (const std::filesystem::path& file : options.score_weights) {
            models.push_back(fixed_model(file.stem().string(), load_weights(file)));
        }
        const SampleSet set = load_samples(db, options.evaluate, threads);
        if (set.samples.empty()) {
            std::cout << "No positions in the files\n";
            return 1;
        }
        const std::vector<ModelScore> scores = score_models(set, models, 0, threads);

        std::cout << std::fixed << set.games << " games, " << set.samples.size() << " positions; the weights as they are, the terms fitted in " << kFolds
                  << " folds by seed\n";
        std::cout << "Log loss, and its change from " << scores[0].name << " (lower is better; 95% by seed):\n"
                  << "  " << std::left << std::setw(26) << "" << std::setw(32) << "raw" << "adjusted for the decks, first player and player to move\n"
                  << std::right;
        for (const ModelScore& score : scores) {
            std::cout << "  " << std::left << std::setw(24) << score.name << std::right;
            for (const Breakdown* breakdown : {&score.raw, &score.adjusted}) {
                std::cout << std::setprecision(4) << std::setw(8) << breakdown->total.loss;
                print_change(breakdown->total.change, breakdown->total.change_error);
            }
            std::cout << '\n';
        }
        std::cout << "\nAdjusted change on each deck's games:\n";
        for (const ModelScore& score : scores) {
            std::cout << "  " << std::left << std::setw(24) << score.name << std::right;
            for (std::size_t d = 0; d < score.left_out.size(); ++d) {
                std::cout << "  " << set.decks[d];
                print_change(score.left_out[d].change, score.left_out[d].change_error);
            }
            std::cout << '\n';
        }
        std::cout << "\nAdjusted loss by the smaller prize count, 1 to 6:\n";
        for (const ModelScore& score : scores) {
            std::cout << "  " << std::left << std::setw(24) << score.name << std::right << std::setprecision(4);
            for (std::size_t p = 1; p < score.adjusted.by_prizes.size(); ++p) {
                std::cout << std::setw(9) << score.adjusted.by_prizes[p].loss;
            }
            std::cout << '\n';
        }
        for (const ModelScore& score : scores) {
            std::cout << "\n" << score.name << ", raw (as played):\n    " << std::left << std::setw(22) << "" << std::right << std::setw(8) << "n" << std::setw(9)
                      << "loss" << std::setw(10) << "predicted" << std::setw(9) << "result" << std::setw(9) << "gap" << '\n';
            print_group("all", score.raw.all);
            for (std::size_t b = 0; b < score.raw.by_prediction.size(); ++b) {
                print_group("predicted " + std::to_string(10 * b) + " to " + std::to_string(10 * b + 10) + "%", score.raw.by_prediction[b]);
            }
        }
        return 0;
    }

    int run_evaluate(const CardDB& db, const Options& options) {
        const int threads = options.threads > 0 ? options.threads : static_cast<int>(std::max(std::thread::hardware_concurrency(), 1U));
        const auto start = std::chrono::steady_clock::now();
        const SampleSet set = load_samples(db, options.evaluate, threads);
        if (set.samples.empty()) {
            std::cout << "No positions in the files\n";
            return 1;
        }
        const double loaded = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::set<std::uint64_t> seeds;
        for (const Sample& sample : set.samples) {
            seeds.insert(sample.seed);
        }

        const std::size_t decks = set.decks.size();
        Weights prizes{};
        prizes[static_cast<std::size_t>(Feature::Prizes)] = default_weights()[static_cast<std::size_t>(Feature::Prizes)];
        const auto fit_options = [](double l2, bool non_negative, bool game_terms) {
            FitOptions fit;
            fit.l2 = l2;
            fit.non_negative = non_negative;
            fit.game_terms = game_terms;
            return fit;
        };
        const std::vector<Model> models{fixed_model("coin", Weights{}),
                                        scaled_model("prizes-scaled", prizes, decks, fit_options(0.0, true, false)),
                                        fixed_model("hand", default_weights()),
                                        scaled_model("hand-scaled", default_weights(), decks, fit_options(0.0, true, false)),
                                        scaled_model("hand-scaled-terms", default_weights(), decks, fit_options(0.0, true, true)),
                                        fitted_model("fitted", decks, fit_options(0.0, false, true)),
                                        fitted_model("fitted-nonneg", decks, fit_options(0.0, true, true)),
                                        fitted_model("fitted-l2-1000", decks, fit_options(1000.0, false, true)),
                                        fitted_model("fitted-l2-10000", decks, fit_options(10000.0, false, true)),
                                        fitted_model("fitted-no-terms", decks, fit_options(0.0, false, false))};
        const std::string reference_name = options.reference.empty() ? "hand-scaled-terms" : options.reference;
        const auto chosen = std::find_if(models.begin(), models.end(), [&](const Model& model) { return model.name == reference_name; });
        if (chosen == models.end()) {
            throw std::runtime_error("no model called " + reference_name);
        }
        const std::size_t reference = static_cast<std::size_t>(chosen - models.begin());
        const std::vector<std::size_t> shown{4, 5, 6, 8, 9};
        const auto weights_file = [&](const std::string& name) {
            return std::filesystem::path(options.save_weights_to) / (name + ".txt");
        };
        const auto saved = [&](const std::string& name) {
            return !options.save_weights_to.empty() && (name.starts_with("fitted") || name.starts_with("hand-scaled"));
        };
        for (const Model& model : models) {
            if (saved(model.name) && std::filesystem::exists(weights_file(model.name))) {
                throw std::runtime_error(weights_file(model.name).string() + " already exists");
            }
        }
        const std::vector<ModelScore> scores = score_models(set, models, reference, threads);
        const double total = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

        std::cout << std::fixed << std::setprecision(1) << set.games << " games, " << seeds.size() << " seeds, " << set.samples.size() << " positions; "
                  << kFolds << " folds by seed; " << loaded << " s to play back, " << total << " s in all\n";
        std::cout << "Held-out log loss, and its change from " << models[reference].name << " (lower is better; 95% by seed):\n"
                  << "  " << std::left << std::setw(20) << "" << std::setw(32) << "raw" << "adjusted for the decks, first player and player to move\n"
                  << std::right;
        for (const ModelScore& score : scores) {
            std::cout << "  " << std::left << std::setw(18) << score.name << std::right;
            for (const Breakdown* breakdown : {&score.raw, &score.adjusted}) {
                std::cout << std::setprecision(4) << std::setw(8) << breakdown->total.loss;
                print_change(breakdown->total.change, breakdown->total.change_error);
            }
            std::cout << '\n';
        }

        std::cout << "\nWeights fitted on every seed (95%, from the folds):\n  " << std::setw(20) << "";
        for (const std::size_t m : shown) {
            std::cout << std::setw(20) << scores[m].name;
        }
        std::cout << '\n';
        for (std::size_t i = 0; i < kFeatureCount; ++i) {
            std::cout << "  " << std::left << std::setw(20) << to_string(static_cast<Feature>(i)) << std::right << std::setprecision(3);
            for (const std::size_t m : shown) {
                std::cout << std::setw(10) << scores[m].weights[i] << " +/- " << scores[m].weight_errors[i] * 1.96;
            }
            std::cout << '\n';
        }

        std::cout << "\nTerms (dropped at play time):";
        for (const std::size_t m : {std::size_t{4}, std::size_t{5}}) {
            std::cout << "\n  " << scores[m].name << ": first " << scores[m].terms[kFirstTerm] << ", turn start " << scores[m].terms[kStartTerm] << ", later "
                      << scores[m].terms[kLaterTerm];
            for (std::size_t a = 0; a < decks; ++a) {
                for (std::size_t b = a + 1; b < decks; ++b) {
                    std::cout << "; " << set.decks[a] << " over " << set.decks[b] << ' ' << scores[m].terms[pairing_term(a, b, decks)];
                }
            }
        }

        std::cout << "\n\nFitted without a deck, scored on its games (adjusted, change from " << models[reference].name << "):\n";
        for (const std::size_t m : {std::size_t{5}, std::size_t{6}, std::size_t{9}}) {
            std::cout << "  " << scores[m].name << '\n';
            for (std::size_t d = 0; d < scores[m].left_out.size(); ++d) {
                std::cout << "    " << std::left << std::setw(20) << set.decks[d] << std::right << std::setprecision(4) << std::setw(8)
                          << scores[m].left_out[d].loss;
                print_change(scores[m].left_out[d].change, scores[m].left_out[d].change_error);
                std::cout << "   weights" << std::setprecision(2);
                for (const double weight : scores[m].left_out_weights[d]) {
                    std::cout << ' ' << weight;
                }
                std::cout << '\n';
            }
        }

        std::cout << "\nAdjusted loss by the smaller prize count:\n  " << std::setw(18) << "";
        for (std::size_t p = 1; p < 7; ++p) {
            std::cout << std::setw(9) << p;
        }
        std::cout << '\n' << std::setprecision(4);
        for (const ModelScore& score : scores) {
            std::cout << "  " << std::left << std::setw(18) << score.name << std::right;
            for (std::size_t p = 1; p < score.adjusted.by_prizes.size(); ++p) {
                std::cout << std::setw(9) << score.adjusted.by_prizes[p].loss;
            }
            std::cout << '\n';
        }

        for (const std::size_t m : {std::size_t{4}, std::size_t{5}, std::size_t{6}}) {
            const Breakdown& breakdown = scores[m].adjusted;
            std::cout << "\n" << scores[m].name << ", adjusted:\n    " << std::left << std::setw(22) << "" << std::right << std::setw(8) << "n" << std::setw(9)
                      << "loss" << std::setw(10) << "predicted" << std::setw(9) << "result" << std::setw(9) << "gap" << '\n';
            print_group("all", breakdown.all);
            print_group("turn start", breakdown.by_kind[0]);
            print_group("later in the turn", breakdown.by_kind[1]);
            for (std::size_t p = 0; p < breakdown.by_prizes.size(); ++p) {
                print_group("smaller prizes " + std::to_string(p), breakdown.by_prizes[p]);
            }
            for (std::size_t d = 0; d < breakdown.by_deck.size(); ++d) {
                print_group(set.decks[d], breakdown.by_deck[d]);
            }
            for (std::size_t b = 0; b < breakdown.by_prediction.size(); ++b) {
                print_group("predicted " + std::to_string(10 * b) + " to " + std::to_string(10 * b + 10) + "%", breakdown.by_prediction[b]);
            }
        }

        for (const ModelScore& score : scores) {
            if (saved(score.name)) {
                std::filesystem::create_directories(options.save_weights_to);
                save_weights(weights_file(score.name), score.weights);
                std::cout << "\nSaved " << weights_file(score.name).string();
            }
        }
        std::cout << '\n';
        return 0;
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
        if (!options.weights_file.empty()) {
            use_weights(options, load_weights(options.weights_file));
        }
        const CardDB db = CardDB::load("data/cards/generated", "data/cards/effects");
        if (!options.replay.empty()) {
            const Replay replay = load_replay(options.replay);
            print_replay(std::cout, replay_table(db, replay), replay, options.as);
            return 0;
        }
        if (!options.check_games.empty()) {
            return run_check_games(db, options.check_games);
        }
        if (!options.evaluate.empty()) {
            return options.score_weights.empty() ? run_evaluate(db, options) : run_score_weights(db, options);
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
        const auto playing = [&](const char* name) {
            return options.agent == name || std::find(options.agents.begin(), options.agents.end(), name) != options.agents.end();
        };
        if (playing("flatmc") || playing("ismcts") || playing("endgame") || options.endgame_lab_games > 0) {
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
        if (options.budget_lab_games > 0) {
            return run_budget_lab_command(db, files, decks, options);
        }
        if (options.endgame_lab_games > 0) {
            return run_endgame_lab_command(db, files, decks, options);
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
