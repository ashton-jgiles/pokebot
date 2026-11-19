#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "agents/greedy_agent.hpp"
#include "agents/human_agent.hpp"
#include "agents/planner_agent.hpp"
#include "agents/random_agent.hpp"
#include "cards/db/card_db.hpp"
#include "core/decklist.hpp"
#include "engine/describe/describe.hpp"
#include "engine/observation/observation.hpp"
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

    class UsageError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    void print_usage() {
        std::cerr << "usage:\n"
                     "  pokebot play   [--deck D] [--against D] [--agent random|biased|greedy|planner] [--seat 0|1] [--seed S] [--save-replay FILE]\n"
                     "  pokebot watch  [--deck D] [--against D] [--agent random|biased|greedy|planner] [--seed S] [--as 0|1|all] [--step] [--save-replay FILE]\n"
                     "  pokebot replay FILE [--as 0|1|all] [--step]\n"
                     "\n"
                     "play     you against an agent, seeing only what your player can see\n"
                     "watch    two agents play each other\n"
                     "replay   play back a saved game\n"
                     "\n"
                     "  --deck D         your deck (or player 0's when watching): a file, or a name in data/decks\n"
                     "                   such as dragapult (default: the first deck there)\n"
                     "  --against D      the other deck (default: the second deck in data/decks)\n"
                     "  --agent A        the agent to play: random, biased, greedy or planner (default biased)\n"
                     "  --seat P         play as player 0 or 1 (default 0)\n"
                     "  --seed S         the game's seed (default: a random one, printed so the game can be replayed)\n"
                     "  --as P           show what player 0 or 1 saw, or all (default all)\n"
                     "  --step           wait for Enter before each turn\n"
                     "  --save-replay F  save the game as a replay\n"
                     "Run it from the repository root, after pokebot-import.\n";
    }

    // --name value pairs, and the flags given without a value.
    struct Args {
        std::vector<std::string> positional;
        std::map<std::string, std::string> values;
        std::vector<std::string> flags;

        std::optional<std::string> value(const std::string& name) const {
            const auto found = values.find(name);
            return found == values.end() ? std::nullopt : std::optional<std::string>(found->second);
        }
        bool flag(const std::string& name) const { return std::find(flags.begin(), flags.end(), name) != flags.end(); }
    };

    Args parse(int argc, char** argv, const std::vector<std::string>& with_value, const std::vector<std::string>& without) {
        Args args;
        for (int i = 2; i < argc; ++i) {
            const std::string arg = argv[i];
            if (std::find(with_value.begin(), with_value.end(), arg) != with_value.end()) {
                if (i + 1 >= argc) {
                    throw UsageError(arg + " needs a value");
                }
                args.values[arg] = argv[++i];
            } else if (std::find(without.begin(), without.end(), arg) != without.end()) {
                args.flags.push_back(arg);
            } else if (!arg.starts_with("--")) {
                args.positional.push_back(arg);
            } else {
                throw UsageError("unknown option " + arg);
            }
        }
        return args;
    }

    int player_arg(const Args& args, const std::string& name, int fallback, bool allow_all) {
        const auto text = args.value(name);
        if (!text) {
            return fallback;
        }
        if (*text == "0" || *text == "1") {
            return std::stoi(*text);
        }
        if (allow_all && *text == "all") {
            return kEveryone;
        }
        throw UsageError(name + " must be 0 or 1" + (allow_all ? std::string(" or all") : std::string()));
    }

    std::uint64_t seed_arg(const Args& args) {
        if (const auto text = args.value("--seed")) {
            try {
                return std::stoull(*text);
            } catch (const std::exception&) {
                throw UsageError("--seed must be a number");
            }
        }
        std::random_device device;
        return (static_cast<std::uint64_t>(device()) << 32) ^ device();
    }

    std::unique_ptr<Agent> make_agent(const std::string& name, std::uint64_t seed) {
        if (name == "random") {
            return std::make_unique<RandomAgent>(seed);
        }
        if (name == "biased") {
            return std::make_unique<BiasedRandomAgent>(seed);
        }
        if (name == "greedy") {
            return std::make_unique<GreedyAgent>(seed);
        }
        if (name == "planner") {
            return std::make_unique<PlannerAgent>(seed);
        }
        throw UsageError("unknown agent '" + name + "' (try random, biased, greedy or planner)");
    }

    std::vector<std::filesystem::path> deck_files() {
        std::vector<std::filesystem::path> files;
        if (std::filesystem::exists("data/decks")) {
            for (const auto& file : std::filesystem::directory_iterator("data/decks")) {
                if (file.path().extension() == ".txt") {
                    files.push_back(file.path());
                }
            }
        }
        std::sort(files.begin(), files.end());
        return files;
    }

    // A deck given as a file, or by name from data/decks; otherwise the one at `fallback` there.
    std::filesystem::path find_deck(const std::optional<std::string>& name, std::size_t fallback) {
        const auto files = deck_files();
        if (!name) {
            if (fallback >= files.size()) {
                throw std::runtime_error("need at least 2 decklists in data/decks");
            }
            return files[fallback];
        }
        const std::filesystem::path decks = "data/decks";
        for (const std::filesystem::path& path : {std::filesystem::path(*name), decks / *name, decks / (*name + ".txt")}) {
            if (std::filesystem::is_regular_file(path)) {
                return path;
            }
        }
        std::string known;
        for (const auto& file : files) {
            known += " " + file.stem().string();
        }
        throw UsageError("no deck '" + *name + "'; decks in data/decks:" + known);
    }

    CardDB load_cards() {
        return CardDB::load("data/cards/generated", "data/cards/effects");
    }

    // Waits for Enter before each turn; q stops. Stops waiting if the input ends.
    std::function<void()> stepper(bool step) {
        if (!step) {
            return {};
        }
        auto waiting = std::make_shared<bool>(true);
        return [waiting] {
            if (!*waiting) {
                return;
            }
            std::cout << "\n(Enter for the next turn, q to stop) " << std::flush;
            std::string line;
            if (!std::getline(std::cin, line)) {
                *waiting = false;
                return;
            }
            if (line == "q") {
                throw QuitGame();
            }
        };
    }

    void save_if_asked(const Args& args, const Replay& replay) {
        if (const auto path = args.value("--save-replay")) {
            save_replay(replay, *path);
            std::cout << "Saved the replay to " << *path << '\n';
        }
    }

    const char* outcome(GameResult result, int seat) {
        if (result == GameResult::Draw) {
            return "It's a draw.";
        }
        if (result == GameResult::Ongoing) {
            return "The game didn't finish.";
        }
        const int winner = result == GameResult::Player0Wins ? 0 : 1;
        return winner == seat ? "You win!" : "You lose.";
    }

    int play(int argc, char** argv) {
        const Args args = parse(argc, argv, {"--deck", "--against", "--agent", "--seat", "--seed", "--save-replay"}, {});
        if (!args.positional.empty()) {
            throw UsageError("play takes no file");
        }
        const int seat = player_arg(args, "--seat", 0, false);
        const std::uint64_t seed = seed_arg(args);
        const std::string agent_name = args.value("--agent").value_or("biased");
        const auto agent = make_agent(agent_name, seed * 2 + 1);
        const auto mine = find_deck(args.value("--deck"), 0);
        const auto theirs = find_deck(args.value("--against"), 1);

        const CardDB db = load_cards();
        Replay replay;
        replay.seed = seed;
        replay.players[static_cast<std::size_t>(seat)] = ReplayPlayer{mine.filename().string(), "you", load_decklist(mine)};
        replay.players[static_cast<std::size_t>(opponent_of(seat))] = ReplayPlayer{theirs.filename().string(), agent_name, load_decklist(theirs)};
        const CardTable table(db, replay.players[0].deck, replay.players[1].deck);

        std::cout << "Game " << seed << ": you are player " << seat << " with " << mine.filename().string() << ", against the "
                  << agent_name << " agent with " << theirs.filename().string() << ".\n"
                  << "Type the number of your choice, b to see the board again, or q to quit.\n";

        HumanAgent you(std::cin, std::cout);
        std::optional<GameState> last;
        MatchOptions options;
        options.on_action = [&](const GameState& before, const Action& action, const GameState& after) {
            replay.actions.push_back(action);
            const int player = before.decision().player;
            std::cout << "  > " << (player == seat ? "you" : "opponent") << ": " << describe(before, action, after, seat) << '\n';
            if (after.turn != before.turn && !after.is_over()) {
                std::cout << "\nTurn " << after.turn << ": " << (after.current_player == seat ? "your" : "the opponent's") << " turn\n";
            }
            last = after;
        };
        try {
            const GameRecord record = seat == 0 ? play_game(table, seed, you, *agent, options) : play_game(table, seed, *agent, you, options);
            replay.result = record.result;
            if (last) {
                std::cout << '\n'
                          << describe(observe(*last, seat)) << "Ended by: " << to_string(how_game_ended(*last)) << '\n';
            }
            std::cout << outcome(record.result, seat) << '\n';
        } catch (const QuitGame&) {
            std::cout << "\nYou quit after " << replay.actions.size() << " actions.\n";
        }
        if (!args.value("--save-replay")) {
            std::cout << "Add --save-replay FILE to keep a game you play.\n";
        }
        save_if_asked(args, replay);
        return 0;
    }

    int watch(int argc, char** argv) {
        const Args args = parse(argc, argv, {"--deck", "--against", "--agent", "--seed", "--as", "--save-replay"}, {"--step"});
        if (!args.positional.empty()) {
            throw UsageError("watch takes no file");
        }
        const int viewer = player_arg(args, "--as", kEveryone, true);
        const std::uint64_t seed = seed_arg(args);
        const std::string agent_name = args.value("--agent").value_or("biased");
        const auto agent0 = make_agent(agent_name, seed * 2);
        const auto agent1 = make_agent(agent_name, seed * 2 + 1);
        const auto first = find_deck(args.value("--deck"), 0);
        const auto second = find_deck(args.value("--against"), 1);

        const CardDB db = load_cards();
        Replay replay;
        replay.seed = seed;
        replay.players[0] = ReplayPlayer{first.filename().string(), agent_name, load_decklist(first)};
        replay.players[1] = ReplayPlayer{second.filename().string(), agent_name, load_decklist(second)};
        const GameRecord record = play_game(CardTable(db, replay.players[0].deck, replay.players[1].deck), seed, *agent0, *agent1);
        replay.actions = record.moves;
        replay.result = record.result;
        try {
            print_replay(std::cout, replay_table(db, replay), replay, viewer, stepper(args.flag("--step")));
        } catch (const QuitGame&) {
            std::cout << "\nStopped.\n";
        }
        save_if_asked(args, replay);
        return 0;
    }

    int replay(int argc, char** argv) {
        const Args args = parse(argc, argv, {"--as"}, {"--step"});
        if (args.positional.size() != 1) {
            throw UsageError("replay needs one file");
        }
        const int viewer = player_arg(args, "--as", kEveryone, true);
        const Replay saved = load_replay(args.positional.front());
        const CardDB db = load_cards();
        try {
            print_replay(std::cout, replay_table(db, saved), saved, viewer, stepper(args.flag("--step")));
        } catch (const QuitGame&) {
            std::cout << "\nStopped.\n";
        }
        return 0;
    }

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc < 2) {
        print_usage();
        return 2;
    }
    const std::string command = argv[1];
    try {
        if (command == "play") {
            return play(argc, argv);
        }
        if (command == "watch") {
            return watch(argc, argv);
        }
        if (command == "replay") {
            return replay(argc, argv);
        }
        if (command != "help" && command != "--help") {
            std::cerr << "error: unknown command '" << command << "'\n";
        }
        print_usage();
        return command == "help" || command == "--help" ? 0 : 2;
    } catch (const UsageError& e) {
        std::cerr << "error: " << e.what() << "\n\n";
        print_usage();
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
