#include "harness/self_play.hpp"

#include <fstream>
#include <optional>
#include <string>

#include "engine/engine.hpp"
#include "engine/shared/rng.hpp"

namespace pokebot {

    namespace {

        double result_for(GameResult result, int player) {
            if (result == GameResult::Draw) {
                return 0.5;
            }
            return (result == GameResult::Player0Wins) == (player == 0) ? 1.0 : 0.0;
        }

        Position position_at(const GameState& state, std::size_t game, std::size_t action, bool start) {
            Position position;
            position.game = game;
            position.action = action;
            position.turn = state.turn;
            position.start = start;
            position.player = state.decision().player;
            position.went_first = state.first_player == position.player;
            position.prizes = static_cast<int>(state.player(position.player).prizes.size());
            position.opponent_prizes = static_cast<int>(state.player(opponent_of(position.player)).prizes.size());
            position.features = features(state, position.player);
            return position;
        }

    } // namespace

    void save_games(const std::filesystem::path& path, const std::vector<Replay>& games) {
        if (std::filesystem::exists(path)) {
            throw ReplayError(path.string() + " already exists");
        }
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
        std::ofstream out(path, std::ios::binary);
        for (const Replay& game : games) {
            out << to_json(game).dump() << '\n';
        }
        out.flush();
        if (!out) {
            throw ReplayError("cannot write " + path.string());
        }
    }

    std::vector<Replay> load_games(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            throw ReplayError("cannot open " + path.string());
        }
        std::vector<Replay> games;
        std::string line;
        for (int number = 1; std::getline(in, line); ++number) {
            try {
                games.push_back(replay_from_json(nlohmann::ordered_json::parse(line)));
            } catch (const std::exception& e) {
                throw ReplayError(path.string() + " line " + std::to_string(number) + ": " + e.what());
            }
        }
        return games;
    }

    std::vector<Position> positions(const CardDB& db, const Replay& replay, std::size_t game) {
        std::vector<Position> out;
        if (replay.result == GameResult::Ongoing) {
            return out;
        }
        Rng rng(replay.seed ^ 0x5E1F91A7ULL);
        std::optional<Position> middle;
        std::uint32_t middles = 0;
        std::uint16_t turn = 0;
        std::size_t next_action = 0;
        const auto finish_turn = [&] {
            if (middle) {
                out.push_back(*middle);
            }
            middle.reset();
            middles = 0;
        };
        const CardTable table = replay_table(db, replay);
        play_back(table, replay, [&](const GameState& before, const Action&, const GameState&) {
            const std::size_t action = next_action++;
            if (before.turn == 0 || before.decision().kind != DecisionKind::MainPhase) {
                return;
            }
            if (before.turn != turn) {
                finish_turn();
                turn = before.turn;
                out.push_back(position_at(before, game, action, true));
                return;
            }
            ++middles;
            if (rng.below(middles) == 0) {
                middle = position_at(before, game, action, false);
            }
        });
        finish_turn();
        for (Position& position : out) {
            position.result = result_for(replay.result, position.player);
        }
        return out;
    }

} // namespace pokebot
