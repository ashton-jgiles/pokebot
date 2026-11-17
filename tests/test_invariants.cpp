#include <string>

#include <catch2/catch_test_macros.hpp>

#include "engine/engine.hpp"
#include "engine/invariants/invariants.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using pokebot::CardDB;
using pokebot::CardTable;
using pokebot::EnergyType;
using pokebot::GameState;

namespace {

    CardDB sample_db() {
        CardDB db;
        db.add(test::basic_pokemon("TST-1", "Testmon"));
        db.add(test::evolution("TST-2", "Testvolve", pokebot::Stage::Stage1, "Testmon"));
        db.add(test::basic_energy("TST-6", "Darkness Energy", EnergyType::Darkness));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {52, 6}});

    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 3);

        Fixture() { test::play_setup(game); }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;
    };

    bool mentions(const std::vector<std::string>& problems, const std::string& text) {
        for (const auto& problem : problems) {
            if (problem.find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

} // namespace

TEST_CASE("a freshly set-up game breaks no invariants", "[invariants]") {
    Fixture f;
    CHECK(pokebot::check_invariants(f.game).empty());
}

TEST_CASE("a duplicated or missing card is caught", "[invariants]") {
    Fixture f;
    auto& player = f.game.player(0);
    player.hand.push_back(player.deck.back()); // now in the deck and the hand
    CHECK(mentions(pokebot::check_invariants(f.game), "appears 2 times"));

    Fixture g;
    g.game.player(0).deck.pop_back(); // gone entirely
    CHECK(mentions(pokebot::check_invariants(g.game), "appears 0 times"));
}

TEST_CASE("a card in the wrong player's zone is caught", "[invariants]") {
    Fixture f;
    const auto card = f.game.player(1).deck.pop_back();
    f.game.player(0).hand.push_back(card);
    CHECK(mentions(pokebot::check_invariants(f.game), "belongs to player 1"));
}

TEST_CASE("broken Pokemon in play are caught", "[invariants]") {
    Fixture f;
    auto& active = *f.game.player(0).active;

    SECTION("damage past HP that wasn't Knocked Out") {
        active.damage_counters = 99;
        CHECK(mentions(pokebot::check_invariants(f.game), "should have been Knocked Out"));
    }
    SECTION("a non-Energy card attached as Energy") {
        active.energy.push_back(test::take_from_deck(f.game, 0, "TST-1"));
        CHECK(mentions(pokebot::check_invariants(f.game), "non-Energy"));
    }
    SECTION("an evolution that doesn't match") {
        test::set_active(f.game, 0, "TST-1");
        f.game.player(0).active->stack.push_back(test::take_from_deck(f.game, 0, "TST-1")); // Testmon on Testmon
        CHECK(mentions(pokebot::check_invariants(f.game), "doesn't evolve from"));
    }
}

TEST_CASE("a missing Active Pokemon is caught unless one is being chosen", "[invariants]") {
    Fixture f;
    auto& player = f.game.player(0);
    for (const auto card : player.active->stack) {
        player.deck.push_back(card);
    }
    for (const auto card : player.active->energy) {
        player.deck.push_back(card);
    }
    player.active.reset();
    CHECK(mentions(pokebot::check_invariants(f.game), "no Active Pokemon"));

    f.game.pending.push_back(pokebot::Decision{pokebot::DecisionKind::PromoteActive, 0, 0, 0});
    CHECK_FALSE(mentions(pokebot::check_invariants(f.game), "no Active Pokemon"));
}
