#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "agents/evaluation/evaluation.hpp"
#include "cards/db/card_db.hpp"
#include "harness/replay.hpp"

namespace pokebot {

    struct Position {
        std::size_t game = 0;
        std::size_t action = 0;
        std::uint16_t turn = 0;
        bool start = true;
        int player = 0;
        bool went_first = false;
        int prizes = 0;
        int opponent_prizes = 0;
        Features features{};
        double result = 0.5;
    };

    void save_games(const std::filesystem::path& path, const std::vector<Replay>& games);
    std::vector<Replay> load_games(const std::filesystem::path& path);
    std::vector<Position> positions(const CardDB& db, const Replay& replay, std::size_t game = 0);

} // namespace pokebot
