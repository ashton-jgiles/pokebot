#include <set>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "engine/action.hpp"
#include "engine/game_state.hpp"

using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardIndex;
using pokebot::CardKind;
using pokebot::CardTable;
using pokebot::DeckEntry;
using pokebot::Decklist;
using pokebot::EnergyKind;
using pokebot::EnergyType;
using pokebot::FixedVector;
using pokebot::GameState;
using pokebot::Rng;
using pokebot::Stage;

namespace {

    CardDB sample_db() {
        CardDef mon;
        mon.id = "TST-1";
        mon.name = "Testmon";
        mon.kind = CardKind::Pokemon;
        mon.stage = Stage::Basic;
        mon.hp = 60;
        mon.types = {EnergyType::Colorless};

        CardDef energy;
        energy.id = "TST-6";
        energy.name = "Darkness Energy";
        energy.kind = CardKind::Energy;
        energy.energy_kind = EnergyKind::Basic;
        energy.provides = {EnergyType::Darkness};

        CardDB db;
        db.add(mon);
        db.add(energy);
        return db;
    }

    // 4 Testmon + 56 Darkness Energy = 60 cards.
    Decklist sample_deck() {
        return Decklist{{DeckEntry{4, "Testmon", "TST", 1}, DeckEntry{56, "Darkness Energy", "TST", 6}}};
    }

} // namespace

TEST_CASE("FixedVector keeps order and enforces its capacity", "[engine]") {
    FixedVector<int, 3> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    CHECK(v.full());
    CHECK_THROWS_AS(v.push_back(4), std::length_error);

    v.erase_at(0);
    REQUIRE(v.size() == 2);
    CHECK(v[0] == 2);
    CHECK(v[1] == 3);

    CHECK(v.remove(3));
    CHECK_FALSE(v.remove(9));
    CHECK(v.pop_back() == 2);
    CHECK(v.empty());
    CHECK_THROWS_AS(v.pop_back(), std::out_of_range);
}

TEST_CASE("FixedVector equality ignores unused slots", "[engine]") {
    FixedVector<int, 3> a;
    FixedVector<int, 3> b;
    a.push_back(1);
    a.push_back(2);
    a.pop_back(); // the 2 is still in memory, but not part of the list
    b.push_back(1);
    CHECK(a == b);
}

TEST_CASE("Rng repeats exactly for the same seed", "[engine]") {
    Rng a(42);
    Rng b(42);
    for (int i = 0; i < 100; ++i) {
        CHECK(a.next() == b.next());
    }
    CHECK(Rng(42).next() != Rng(43).next());
}

TEST_CASE("Rng::below stays in range and reaches every value", "[engine]") {
    Rng rng(1);
    std::array<int, 6> counts{};
    for (int i = 0; i < 6000; ++i) {
        const auto value = rng.below(6);
        REQUIRE(value < 6);
        ++counts[value];
    }
    for (const int count : counts) {
        CHECK(count > 800); // about 1000 expected for each
    }
}

TEST_CASE("shuffle is a repeatable permutation", "[engine]") {
    FixedVector<CardIndex, 60> original;
    for (int i = 0; i < 60; ++i) {
        original.push_back(static_cast<CardIndex>(i));
    }
    auto a = original;
    auto b = original;
    Rng first(7);
    Rng second(7);
    first.shuffle(a);
    second.shuffle(b);

    CHECK(a == b);              // same seed, same order
    CHECK_FALSE(a == original); // actually shuffled
    const std::set<int> seen(a.begin(), a.end());
    CHECK(seen.size() == 60); // nothing lost or duplicated
}

TEST_CASE("a copied GameState is equal and independent", "[engine]") {
    GameState game;
    game.player(0).hand.push_back(5);

    GameState copy = game;
    CHECK(copy == game);

    copy.player(0).hand.push_back(6);
    CHECK_FALSE(copy == game);
    CHECK(game.player(0).hand.size() == 1);

    INFO("sizeof(GameState) = " << sizeof(GameState));
    CHECK(sizeof(GameState) < 4096);
}

TEST_CASE("card instances belong to the right player", "[engine]") {
    CHECK(pokebot::owner_of(0) == 0);
    CHECK(pokebot::owner_of(59) == 0);
    CHECK(pokebot::owner_of(60) == 1);
    CHECK(pokebot::owner_of(119) == 1);
    CHECK(pokebot::opponent_of(0) == 1);
}

TEST_CASE("CardTable numbers cards in decklist order", "[engine]") {
    const CardDB db = sample_db();
    const CardTable table(db, sample_deck(), sample_deck());
    CHECK(table.def(0).name == "Testmon");
    CHECK(table.def(3).name == "Testmon");
    CHECK(table.def(4).name == "Darkness Energy");
    CHECK(table.def(59).name == "Darkness Energy");
    CHECK(table.def(60).name == "Testmon"); // player 1's first card
}

TEST_CASE("CardTable refuses an illegal deck", "[engine]") {
    const CardDB db = sample_db();
    Decklist short_deck = sample_deck();
    short_deck.entries[1].count = 55; // 59 cards
    CHECK_THROWS_AS(CardTable(db, short_deck, sample_deck()), std::invalid_argument);
}