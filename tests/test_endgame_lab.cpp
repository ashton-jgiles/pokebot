#include <algorithm>
#include <cstddef>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "harness/endgame_lab.hpp"
#include "support/real_decks.hpp"

using pokebot::count_placements;

TEST_CASE("placements are counted by card, not by copy", "[endgame]") {
    CHECK(count_placements({2, 1}, 1) == 2.0);
    CHECK(count_placements({2, 2}, 2) == 3.0);
    CHECK(count_placements({1, 1, 1, 1, 1}, 2) == 10.0);
    CHECK(count_placements({1, 1, 1}, 1, 1) == 6.0);
    CHECK(count_placements({3}, 1, 1) == 1.0);
    CHECK(count_placements({2}, 2, 1) == 0.0);
    CHECK(count_placements({}, 0, 0) == 1.0);
}

TEST_CASE("on the real decks, the endgame lab counts worlds and measures two-segment trees, the same for the same seed", "[endgame]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    std::vector<pokebot::ArenaDeck> decks;
    for (const auto& list : real->decks) {
        decks.push_back(pokebot::ArenaDeck{"deck", list});
    }
    pokebot::EndgameLabOptions options;
    options.games = 2;
    options.every = 25;
    options.tree_prizes = 6;
    options.plans = 4;
    options.and_budget = 100;
    options.solver_worlds = 2;
    const pokebot::EndgameLabResult result = pokebot::run_endgame_lab(real->db, decks, options);
    REQUIRE(!result.positions.empty());
    long long trees = 0;
    int deepest = 0;
    for (const pokebot::EndgamePosition& p : result.positions) {
        CHECK(p.own_worlds >= 1.0);
        CHECK(p.opponent_worlds >= 1.0);
        CHECK(p.hidden_hand >= 0);
        CHECK(p.min_prizes >= 1);
        CHECK(p.min_prizes <= 6);
        if (p.measured) {
            ++trees;
            CHECK(p.tree.plans <= 4);
            CHECK(p.tree.chance >= p.tree.opponent_searches);
            CHECK(p.tree.leaves >= p.tree.plans - p.tree.unplayable);
            CHECK(p.tree.value >= -1.0);
            CHECK(p.tree.value <= 1.0);
            CHECK(p.solve.value >= -1.0);
            CHECK(p.solve.value <= 1.0);
            CHECK(p.solve.depth <= 2);
            deepest = std::max(deepest, p.solve.depth);
        }
    }
    CHECK(trees > 0);
    CHECK(deepest == 2);
    CHECK(result.games == 2);
    const pokebot::EndgameLabResult again = pokebot::run_endgame_lab(real->db, decks, options);
    REQUIRE(again.positions.size() == result.positions.size());
    for (std::size_t i = 0; i < result.positions.size(); ++i) {
        CHECK(again.positions[i].opponent_worlds == result.positions[i].opponent_worlds);
        CHECK(again.positions[i].tree.leaves == result.positions[i].tree.leaves);
        CHECK(again.positions[i].tree.value == result.positions[i].tree.value);
    }
    const pokebot::EndgameSummary summary = pokebot::summarize(result, result.positions.front().min_prizes);
    CHECK(summary.positions > 0);
    CHECK(summary.median_total >= summary.median_opponent);
}
