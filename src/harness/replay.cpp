#include "harness/replay.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "engine/describe/describe.hpp"
#include "engine/engine.hpp"
#include "engine/observation/observation.hpp"
#include "harness/match.hpp"

namespace pokebot {

    namespace {

        using json = nlohmann::ordered_json;

        constexpr int kVersion = 1;

        // Actions are saved by name, so reordering ActionKind doesn't break old replays.
        constexpr std::array<const char*, 24> kActionNames = {
            "ChooseTurnOrder", "SetupPlaceActive", "SetupPlaceBench", "SetupDone", "DrawMulliganCards",
            "PlayBasic", "Evolve", "AttachEnergy", "Retreat", "PlayTrainer", "AttachTool", "UseAbility",
            "UseStadium", "Attack", "EndTurn", "DiscardEnergy", "Promote", "ChooseCard", "ChooseDone",
            "ChoosePokemon", "AnswerYes", "AnswerNo", "ChooseAttack", "DiscardBenched"};
        static_assert(kActionNames.size() == static_cast<std::size_t>(ActionKind::DiscardBenched) + 1,
                      "every ActionKind needs a name");

        constexpr std::array<const char*, 4> kResultNames = {"ongoing", "player 0 wins", "player 1 wins", "draw"};

        // "AttachEnergy 37 0 0": the kind, the card ("-" for none), the target and the amount.
        std::string action_line(const Action& action) {
            const std::string card = action.card == kNoCard ? "-" : std::to_string(action.card);
            return std::string(kActionNames[static_cast<std::size_t>(action.kind)]) + ' ' + card + ' ' +
                   std::to_string(action.target) + ' ' + std::to_string(action.amount);
        }

        std::uint8_t small_number(const std::string& text, int limit, const std::string& line) {
            std::size_t used = 0;
            int value = -1;
            try {
                value = std::stoi(text, &used);
            } catch (const std::exception&) {
                used = 0;
            }
            if (used != text.size() || value < 0 || value > limit) {
                throw ReplayError("bad action '" + line + "'");
            }
            return static_cast<std::uint8_t>(value);
        }

        Action action_from(const std::string& line) {
            std::istringstream words(line);
            std::string kind;
            std::string card;
            std::string target;
            std::string amount;
            std::string extra;
            if (!(words >> kind >> card >> target >> amount) || (words >> extra)) {
                throw ReplayError("bad action '" + line + "'");
            }
            const auto found = std::find_if(kActionNames.begin(), kActionNames.end(), [&](const char* name) { return kind == name; });
            if (found == kActionNames.end()) {
                throw ReplayError("unknown action '" + kind + "'");
            }
            Action action;
            action.kind = static_cast<ActionKind>(found - kActionNames.begin());
            action.card = card == "-" ? kNoCard : small_number(card, static_cast<int>(kMaxCards) - 1, line);
            action.target = small_number(target, 255, line);
            action.amount = small_number(amount, 255, line);
            return action;
        }

        GameResult result_from(const std::string& name) {
            const auto found = std::find_if(kResultNames.begin(), kResultNames.end(), [&](const char* n) { return name == n; });
            if (found == kResultNames.end()) {
                throw ReplayError("unknown result '" + name + "'");
            }
            return static_cast<GameResult>(found - kResultNames.begin());
        }

        const char* result_name(GameResult result) {
            return kResultNames[static_cast<std::size_t>(result)];
        }

        Decklist deck_from(const json& lines) {
            std::string text;
            for (const auto& line : lines) {
                text += line.get<std::string>() + '\n';
            }
            std::istringstream in(text);
            try {
                return parse_decklist(in);
            } catch (const DecklistError& e) {
                throw ReplayError(std::string("bad deck: ") + e.what());
            }
        }

    } // namespace

    json to_json(const Replay& replay) {
        json players = json::array();
        for (const ReplayPlayer& player : replay.players) {
            json deck = json::array();
            for (const DeckEntry& entry : player.deck.entries) {
                deck.push_back(to_line(entry));
            }
            players.push_back({{"deck_name", player.deck_name}, {"agent", player.agent}, {"deck", deck}});
        }
        json actions = json::array();
        for (const Action& action : replay.actions) {
            actions.push_back(action_line(action));
        }
        return {{"format", "pokebot replay"}, {"version", kVersion}, {"seed", replay.seed}, {"players", players}, {"result", result_name(replay.result)}, {"note", replay.note}, {"actions", actions}};
    }

    Replay replay_from_json(const json& in) {
        try {
            if (!in.is_object() || in.value("format", "") != "pokebot replay") {
                throw ReplayError("not a pokebot replay");
            }
            if (in.at("version").get<int>() != kVersion) {
                throw ReplayError("replay version " + in.at("version").dump() + " isn't supported (this build reads version " +
                                  std::to_string(kVersion) + ")");
            }
            Replay replay;
            replay.seed = in.at("seed").get<std::uint64_t>();
            const json& players = in.at("players");
            if (!players.is_array() || players.size() != kPlayers) {
                throw ReplayError("a replay needs exactly 2 players");
            }
            for (std::size_t p = 0; p < kPlayers; ++p) {
                replay.players[p].deck_name = players[p].value("deck_name", "");
                replay.players[p].agent = players[p].value("agent", "");
                replay.players[p].deck = deck_from(players[p].at("deck"));
            }
            for (const auto& action : in.at("actions")) {
                replay.actions.push_back(action_from(action.get<std::string>()));
            }
            replay.result = result_from(in.at("result").get<std::string>());
            replay.note = in.value("note", "");
            return replay;
        } catch (const json::exception& e) {
            throw ReplayError(std::string("not a valid replay: ") + e.what());
        }
    }

    void save_replay(const Replay& replay, const std::filesystem::path& path) {
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
        std::ofstream out(path, std::ios::binary);
        if (!out) {
            throw ReplayError("cannot write " + path.string());
        }
        out << to_json(replay).dump(2) << '\n';
    }

    Replay load_replay(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            throw ReplayError("cannot open " + path.string());
        }
        try {
            return replay_from_json(json::parse(in));
        } catch (const json::exception& e) {
            throw ReplayError(path.string() + ": " + e.what());
        } catch (const ReplayError& e) {
            throw ReplayError(path.string() + ": " + e.what());
        }
    }

    CardTable replay_table(const CardDB& db, const Replay& replay) {
        try {
            return CardTable(db, replay.players[0].deck, replay.players[1].deck).shuffled(~replay.seed);
        } catch (const std::invalid_argument& e) {
            throw ReplayError(std::string("the replay's decks can't be played: ") + e.what());
        }
    }

    GameState play_back(const CardTable& table, const Replay& replay, const ReplayStep& each) {
        GameState game = new_game(table, replay.seed);
        for (std::size_t i = 0; i < replay.actions.size(); ++i) {
            const Action& action = replay.actions[i];
            const std::string where = "action " + std::to_string(i + 1) + " (" + action_line(action) + ")";
            if (game.is_over()) {
                throw ReplayError(where + " comes after the game ended: the cards or the engine have changed since the replay was saved");
            }
            const auto legal = legal_actions(game);
            if (std::find(legal.begin(), legal.end(), action) == legal.end()) {
                throw ReplayError(where + " isn't legal: the cards or the engine have changed since the replay was saved");
            }
            if (each) {
                const GameState before = game;
                apply_legal(game, action);
                each(before, action, game);
            } else {
                apply_legal(game, action);
            }
        }
        if (replay.result != GameResult::Ongoing && game.result != replay.result) {
            throw ReplayError(std::string("the replay ended with ") + result_name(replay.result) + ", but playing it back ends with " +
                              result_name(game.result));
        }
        return game;
    }

    void print_replay(std::ostream& out, const CardTable& table, const Replay& replay, int viewer, const std::function<void()>& at_turn) {
        const auto board = [&](const GameState& game) {
            return viewer == kEveryone ? describe(game) : describe(observe(game, viewer));
        };
        out << "Game " << replay.seed << ": " << replay.players[0].deck_name << " (player 0, " << replay.players[0].agent << ") vs "
            << replay.players[1].deck_name << " (player 1, " << replay.players[1].agent << ")\n";
        out << (viewer == kEveryone ? std::string("Showing everything") : "As player " + std::to_string(viewer) + " saw it") << '\n';
        if (!replay.note.empty()) {
            out << "Note: " << replay.note << '\n';
        }
        int last_turn = -1;
        const GameState end = play_back(table, replay, [&](const GameState& before, const Action& action, const GameState& after) {
            if (before.turn != last_turn) {
                last_turn = before.turn;
                if (at_turn) {
                    at_turn();
                }
                out << '\n'
                    << board(before);
            }
            const std::string text = viewer == kEveryone ? describe(before, action) : describe(before, action, after, viewer);
            out << "  > player " << static_cast<int>(before.decision().player) << ": " << text << '\n';
        });
        out << '\n'
            << board(end);
        if (end.is_over()) {
            out << "Ended by: " << to_string(how_game_ended(end)) << '\n';
        } else {
            out << "Stopped after " << replay.actions.size() << " actions\n";
        }
    }

} // namespace pokebot
