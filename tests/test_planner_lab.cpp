#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "harness/planner_lab.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"

using pokebot::ArenaDeck;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::PlannerLabOptions;
using pokebot::PlannerLabResult;

namespace {

    CardDef with_attack(CardDef c, const char* name, int damage) {
        pokebot::Attack a;
        a.name = name;
        a.cost = {pokebot::EnergyType::Colorless};
        a.damage = damage;
        c.attacks.push_back(a);
        return c;
    }

    CardDB sample_db() {
        CardDB db;
        db.add(with_attack(test::basic_pokemon("TST-1", "Tackler"), "Tackle", 30));
        db.add(test::basic_energy("TST-2", "Darkness Energy", pokebot::EnergyType::Darkness));
        db.add(with_attack(test::basic_pokemon("TST-3", "Tackler B"), "Tackle", 30));
        db.add(with_attack(test::evolution("TST-4", "Big Tackler", pokebot::Stage::Stage1, "Tackler"), "Big Tackle", 60));
        db.add(with_attack(test::basic_pokemon("TST-5", "Glider", 60, 0), "Tackle", 30));
        return db;
    }

    void check_consistent(const PlannerLabResult& r) {
        INFO(r.positions << " positions, " << r.finished << " finished, " << r.wins << " wins, " << r.wins_found << " found");
        CHECK(r.positions > 0);
        CHECK(r.finished > 0);
        CHECK(r.false_wins == 0);
        CHECK(r.wins_found <= r.wins);
        CHECK(r.or_closed <= r.or_open);
        CHECK(r.and_both <= r.and_closed);
        CHECK(r.and_bound <= r.and_neither);
        CHECK(r.and_closed <= r.and_neither);
        CHECK(r.bound_changed == 0);
    }

} // namespace

TEST_CASE("the planner lab measures searches in sampled positions, and the counts add up", "[planner]") {
    const CardDB db = sample_db();
    const std::vector<ArenaDeck> decks{ArenaDeck{"tacklers.txt", test::deck({{4, 1}, {4, 3}, {4, 4}, {4, 5}, {44, 2}})}};
    PlannerLabOptions options;
    options.games = 2;
    options.every = 2;
    check_consistent(pokebot::run_planner_lab(db, decks, options));
}

TEST_CASE("on the real decks, the planner lab finds every win it can and no false ones", "[planner]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    std::vector<ArenaDeck> decks;
    for (const auto& list : real->decks) {
        decks.push_back(ArenaDeck{"deck.txt", list});
    }
    PlannerLabOptions options;
    options.games = 1;
    options.every = 10;
    options.limit = 400;
    const PlannerLabResult r = pokebot::run_planner_lab(real->db, decks, options);
    check_consistent(r);
    CHECK(r.wins_found == r.wins);
}
