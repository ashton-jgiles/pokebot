#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/random/random_agent.hpp"
#include "harness/match.hpp"
#include "harness/self_play.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"

using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::GameResult;
using pokebot::GameState;
using pokebot::Position;
using pokebot::Replay;

namespace {

    CardDef with_attack(CardDef c, int damage) {
        c.attacks.push_back(test::attack("Tackle", {pokebot::EnergyType::Colorless}, damage));
        return c;
    }

    CardDB sample_db() {
        CardDB db;
        db.add(with_attack(test::basic_pokemon("TST-1", "Tackler"), 30));
        db.add(test::basic_energy("TST-2", "Darkness Energy", pokebot::EnergyType::Darkness));
        db.add(with_attack(test::basic_pokemon("TST-3", "Tackler B"), 30));
        db.add(with_attack(test::basic_pokemon("TST-4", "Tackler C"), 30));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 3}, {4, 4}, {48, 2}});

    Replay play(const CardDB& db, const pokebot::Decklist& deck0, const pokebot::Decklist& deck1, std::uint64_t seed) {
        const CardTable table(db, deck0, deck1);
        pokebot::BiasedRandomAgent first(seed * 2);
        pokebot::BiasedRandomAgent second(seed * 2 + 1);
        const auto record = pokebot::play_game(table, seed, first, second);
        Replay replay;
        replay.seed = seed;
        replay.players[0] = {"one.txt", "biased", deck0};
        replay.players[1] = {"two.txt", "biased", deck1};
        replay.actions = record.moves;
        replay.result = record.result;
        return replay;
    }

    struct MainPhase {
        std::size_t action = 0;
        std::uint16_t turn = 0;
        int player = 0;
        pokebot::Features features{};
    };

    std::vector<MainPhase> main_phases(const CardDB& db, const Replay& replay) {
        std::vector<MainPhase> out;
        std::size_t next_action = 0;
        const CardTable table = pokebot::replay_table(db, replay);
        pokebot::play_back(table, replay, [&](const GameState& before, const pokebot::Action&, const GameState&) {
            const std::size_t action = next_action++;
            if (before.turn > 0 && before.decision().kind == pokebot::DecisionKind::MainPhase) {
                const int player = before.decision().player;
                out.push_back(MainPhase{action, before.turn, player, pokebot::features(before, player)});
            }
        });
        return out;
    }

    struct TempFile {
        std::filesystem::path path;

        explicit TempFile(const std::string& name) : path(std::filesystem::temp_directory_path() / ("pokebot-" + name + ".jsonl")) { remove(); }
        ~TempFile() { remove(); }
        TempFile(const TempFile&) = delete;
        TempFile& operator=(const TempFile&) = delete;

        void remove() const {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    };

} // namespace

TEST_CASE("self-play games survive saving and loading, and a file isn't overwritten", "[self_play]") {
    const CardDB db = sample_db();
    const std::vector<Replay> games{play(db, kDeck, kDeck, 1), play(db, kDeck, kDeck, 2)};
    const TempFile file("games-saved");
    pokebot::save_games(file.path, games);
    CHECK(pokebot::load_games(file.path) == games);
    CHECK_THROWS_AS(pokebot::save_games(file.path, games), pokebot::ReplayError);
    CHECK(pokebot::load_games(file.path) == games);
}

TEST_CASE("a games file with a bad line is rejected", "[self_play]") {
    const TempFile file("games-bad");
    {
        std::ofstream out(file.path, std::ios::binary);
        out << pokebot::to_json(play(sample_db(), kDeck, kDeck, 1)).dump() << "\n{}\n";
    }
    CHECK_THROWS_AS(pokebot::load_games(file.path), pokebot::ReplayError);
    CHECK_THROWS_AS(pokebot::load_games(file.path.parent_path() / "pokebot-no-such-games.jsonl"), pokebot::ReplayError);
}

TEST_CASE("each turn gives its first main phase and at most one later, with that moment's features", "[self_play]") {
    const CardDB db = sample_db();
    for (std::uint64_t seed = 1; seed <= 5; ++seed) {
        INFO("seed " << seed);
        const Replay replay = play(db, kDeck, kDeck, seed);
        const std::vector<MainPhase> all = main_phases(db, replay);
        std::map<std::uint16_t, std::vector<MainPhase>> by_turn;
        for (const MainPhase& m : all) {
            by_turn[m.turn].push_back(m);
        }
        std::map<std::uint16_t, int> starts;
        std::map<std::uint16_t, int> middles;
        for (const Position& p : pokebot::positions(db, replay, 7)) {
            CHECK(p.game == 7);
            REQUIRE(by_turn.count(p.turn) == 1);
            const auto& turn = by_turn[p.turn];
            CHECK(p.player == turn.front().player);
            if (p.start) {
                ++starts[p.turn];
                CHECK(p.action == turn.front().action);
                CHECK(p.features == turn.front().features);
            } else {
                ++middles[p.turn];
                bool found = false;
                for (std::size_t i = 1; i < turn.size(); ++i) {
                    found = found || (turn[i].action == p.action && turn[i].features == p.features);
                }
                CHECK(found);
            }
        }
        for (const auto& [turn, decisions] : by_turn) {
            CHECK(starts[turn] == 1);
            CHECK(middles[turn] == (decisions.size() > 1 ? 1 : 0));
        }
    }
}

TEST_CASE("a position's result is the game's, for the player to move", "[self_play]") {
    const CardDB db = sample_db();
    Replay replay = play(db, kDeck, kDeck, 3);
    REQUIRE(replay.result != GameResult::Ongoing);
    const auto rows = pokebot::positions(db, replay);
    REQUIRE(!rows.empty());
    for (const Position& p : rows) {
        const double expected = replay.result == GameResult::Draw ? 0.5 : ((replay.result == GameResult::Player0Wins) == (p.player == 0) ? 1.0 : 0.0);
        CHECK(p.result == expected);
    }
    replay.result = GameResult::Ongoing;
    CHECK(pokebot::positions(db, replay).empty());
}

TEST_CASE("the later main phase is picked evenly, and the same each time", "[self_play]") {
    const CardDB db = sample_db();
    double sum = 0.0;
    double variance = 0.0;
    int picked = 0;
    for (std::uint64_t seed = 1; seed <= 200; ++seed) {
        const Replay replay = play(db, kDeck, kDeck, seed);
        std::map<std::uint16_t, std::vector<std::size_t>> later;
        std::uint16_t turn = 0;
        for (const MainPhase& m : main_phases(db, replay)) {
            if (m.turn == turn) {
                later[m.turn].push_back(m.action);
            }
            turn = m.turn;
        }
        const auto rows = pokebot::positions(db, replay);
        const auto again = pokebot::positions(db, replay);
        REQUIRE(rows.size() == again.size());
        for (std::size_t r = 0; r < rows.size(); ++r) {
            CHECK(rows[r].action == again[r].action);
            const auto& choices = later[rows[r].turn];
            if (rows[r].start || choices.size() < 2) {
                continue;
            }
            const auto at = std::find(choices.begin(), choices.end(), rows[r].action);
            REQUIRE(at != choices.end());
            const double last = static_cast<double>(choices.size() - 1);
            sum += static_cast<double>(at - choices.begin()) / last - 0.5;
            variance += (last + 2.0) / (12.0 * last);
            ++picked;
        }
    }
    CAPTURE(picked, sum, variance);
    REQUIRE(picked >= 30);
    CHECK(std::abs(sum) < 4.0 * std::sqrt(variance));
}

TEST_CASE("on the real decks, positions have features in range and both players' results", "[self_play]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const Replay replay = play(real->db, real->decks[0], real->decks[1], 4);
    const auto rows = pokebot::positions(real->db, replay);
    REQUIRE(!rows.empty());
    bool seen[2] = {false, false};
    for (const Position& p : rows) {
        seen[p.player] = true;
        CHECK(p.prizes >= 0);
        CHECK(p.prizes <= 6);
        CHECK(p.opponent_prizes <= 6);
        for (const double value : p.features) {
            CHECK(value >= -1.0);
            CHECK(value <= 1.0);
        }
    }
    CHECK(seen[0]);
    CHECK(seen[1]);
}
