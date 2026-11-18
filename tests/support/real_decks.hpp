#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include "cards/db/card_db.hpp"
#include "core/decklist.hpp"

namespace test {

    struct RealDecks {
        pokebot::CardDB db;
        std::vector<pokebot::Decklist> decks;
    };

    // The project's real card data and decks, or nothing if pokebot-import hasn't been run.
    inline std::unique_ptr<RealDecks> load_real_decks() {
        const std::filesystem::path root = POKEBOT_SOURCE_DIR;
        if (!std::filesystem::exists(root / "data/cards/generated")) {
            return nullptr;
        }
        auto real = std::make_unique<RealDecks>();
        real->db = pokebot::CardDB::load(root / "data/cards/generated", root / "data/cards/effects");
        for (const auto& file : std::filesystem::directory_iterator(root / "data/decks")) {
            if (file.path().extension() == ".txt") {
                real->decks.push_back(pokebot::load_decklist(file.path()));
            }
        }
        return real;
    }

} // namespace test
