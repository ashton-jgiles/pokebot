#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine/engine.hpp"
#include "engine/internal.hpp"
#include "support/test_cards.hpp"

using pokebot::Action;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardTable;
using pokebot::DecisionKind;
using pokebot::EnergyType;
using pokebot::GameState;
using pokebot::PlayerState;

namespace {

    // TST-1..TST-5: five different Basic Pokemon. TST-6: Darkness Energy.
    CardDB sample_db() {
        CardDB db;
        db.add(test::basic_pokemon("TST-1", "Testmon"));
        db.add(test::basic_pokemon("TST-2", "Bravomon"));
        db.add(test::basic_pokemon("TST-3", "Charliemon"));
        db.add(test::basic_pokemon("TST-4", "Deltamon"));
        db.add(test::basic_pokemon("TST-5", "Echomon"));
        db.add(test::basic_energy("TST-6", "Darkness Energy", EnergyType::Darkness));
        return db;
    }

    // Every card a player owns must be in exactly one place.
    std::size_t cards_accounted_for(const PlayerState& p) {
        std::size_t n = p.deck.size() + p.hand.size() + p.prizes.size() + p.discard.size() + p.lost_zone.size();
        if (p.active) {
            n += p.active->stack.size() + p.active->energy.size();
        }
        for (const auto& pokemon : p.bench) {
            n += pokemon.stack.size() + pokemon.energy.size();
        }
        return n;
    }

    // Plays the first legal action until setup is over.
    void finish_setup(GameState& game) {
        for (int step = 0; step < 100 && game.turn == 0; ++step) {
            pokebot::apply(game, pokebot::legal_actions(game).front());
        }
        REQUIRE(game.turn == 1);
    }

} // namespace

TEST_CASE("a new game deals 7 cards with a Basic and starts with the turn-order choice", "[engine][setup]") {
    const CardDB db = sample_db();
    const CardTable table(db, test::deck({{4, 1}, {56, 6}}), test::deck({{4, 1}, {56, 6}}));
    const GameState game = pokebot::new_game(table, 1);

    CHECK(game.decision().kind == DecisionKind::ChooseTurnOrder);
    for (int p = 0; p < 2; ++p) {
        const PlayerState& player = game.player(p);
        CHECK(player.hand.size() == 7);
        CHECK(player.deck.size() == 53);
        CHECK(player.prizes.empty());
        bool has_basic = false;
        for (const auto card : player.hand) {
            has_basic = has_basic || pokebot::detail::is_basic_pokemon(game, card);
        }
        CHECK(has_basic);
    }
}

TEST_CASE("the same seed gives the same game", "[engine][setup]") {
    const CardDB db = sample_db();
    const CardTable table(db, test::deck({{4, 1}, {56, 6}}), test::deck({{4, 1}, {56, 6}}));
    CHECK(pokebot::new_game(table, 5) == pokebot::new_game(table, 5));
    CHECK_FALSE(pokebot::new_game(table, 5) == pokebot::new_game(table, 6));
}

TEST_CASE("the coin-flip winner can choose to go second", "[engine][setup]") {
    const CardDB db = sample_db();
    const CardTable table(db, test::deck({{4, 1}, {56, 6}}), test::deck({{4, 1}, {56, 6}}));
    GameState game = pokebot::new_game(table, 3);
    const int winner = game.decision().player;

    pokebot::apply(game, Action{ActionKind::ChooseTurnOrder, pokebot::kNoCard, pokebot::kActiveSlot, 1});
    CHECK(game.first_player == pokebot::opponent_of(winner));
}

TEST_CASE("setup ends with 6 Prize cards and the first player's turn", "[engine][setup]") {
    const CardDB db = sample_db();
    const CardTable table(db, test::deck({{4, 1}, {56, 6}}), test::deck({{4, 1}, {56, 6}}));
    GameState game = pokebot::new_game(table, 11);
    finish_setup(game);

    CHECK(game.current_player == game.first_player);
    CHECK(game.decision().kind == DecisionKind::MainPhase);
    CHECK(game.decision().player == game.first_player);
    for (int p = 0; p < 2; ++p) {
        CHECK(game.player(p).prizes.size() == 6);
        CHECK(game.player(p).active.has_value());
        CHECK(cards_accounted_for(game.player(p)) == 60);
    }
}

TEST_CASE("each distinct Basic is offered once, and the Bench holds at most 5", "[engine][setup]") {
    const CardDB db = sample_db();
    // 4 of each of five Basics: opening hands are full of Basics.
    const auto many_basics = test::deck({{4, 1}, {4, 2}, {4, 3}, {4, 4}, {4, 5}, {40, 6}});
    const CardTable table(db, many_basics, many_basics);
    GameState game = pokebot::new_game(table, 21);
    pokebot::apply(game, pokebot::legal_actions(game).front()); // turn order

    const auto actives = pokebot::legal_actions(game);
    for (std::size_t i = 0; i < actives.size(); ++i) {
        for (std::size_t j = i + 1; j < actives.size(); ++j) {
            CHECK(game.def(actives[i].card).id != game.def(actives[j].card).id);
        }
    }

    finish_setup(game);
    for (int p = 0; p < 2; ++p) {
        CHECK(game.player(p).bench.size() <= 5);
    }
}

TEST_CASE("mulligans give the opponent extra draws", "[engine][setup]") {
    const CardDB db = sample_db();
    // Player 1 has a single Basic, so it mulligans often.
    const CardTable table(db, test::deck({{4, 1}, {56, 6}}), test::deck({{1, 1}, {59, 6}}));

    GameState game;
    bool found = false;
    for (std::uint64_t seed = 1; seed < 500 && !found; ++seed) {
        game = pokebot::new_game(table, seed);
        found = game.player(1).mulligans > game.player(0).mulligans;
    }
    REQUIRE(found);
    const int bonus = game.player(1).mulligans - game.player(0).mulligans;

    // Play setup, taking the largest bonus draw offered.
    bool offered_bonus = false;
    for (int step = 0; step < 100 && game.turn == 0; ++step) {
        auto actions = pokebot::legal_actions(game);
        if (game.decision().kind == DecisionKind::MulliganDraws) {
            offered_bonus = true;
            CHECK(game.decision().player == 0);
            CHECK(game.decision().amount == bonus);
            const std::size_t hand_before = game.player(0).hand.size();
            const Action most = actions.back();
            pokebot::apply(game, most);
            CHECK(game.player(0).hand.size() == hand_before + most.amount);
        } else {
            pokebot::apply(game, actions.front());
        }
    }
    REQUIRE(game.turn == 1);
    CHECK(offered_bonus);
    CHECK(cards_accounted_for(game.player(0)) == 60);
    CHECK(cards_accounted_for(game.player(1)) == 60);
}

TEST_CASE("equal mulligans give nobody extra draws", "[engine][setup]") {
    const CardDB db = sample_db();
    // Both players have a single Basic, so both mulligan often.
    const auto one_basic = test::deck({{1, 1}, {59, 6}});
    const CardTable table(db, one_basic, one_basic);

    GameState game;
    bool found = false;
    for (std::uint64_t seed = 1; seed < 2000 && !found; ++seed) {
        game = pokebot::new_game(table, seed);
        found = game.player(0).mulligans > 0 && game.player(0).mulligans == game.player(1).mulligans;
    }
    REQUIRE(found);
    for (const auto& decision : game.pending) {
        CHECK(decision.kind != DecisionKind::MulliganDraws);
    }
}

TEST_CASE("random setups with the real decks always reach turn 1 with every card accounted for", "[engine][setup]") {
    const std::filesystem::path root = POKEBOT_SOURCE_DIR;
    if (!std::filesystem::exists(root / "data/cards/generated")) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const CardDB db = CardDB::load(root / "data/cards/generated", root / "data/cards/effects");
    std::vector<pokebot::Decklist> decks;
    for (const auto& file : std::filesystem::directory_iterator(root / "data/decks")) {
        if (file.path().extension() == ".txt") {
            decks.push_back(pokebot::load_decklist(file.path()));
        }
    }
    REQUIRE(decks.size() >= 2);

    pokebot::Rng chooser(99);
    for (std::uint64_t seed = 0; seed < 1000; ++seed) {
        const auto& deck0 = decks[seed % decks.size()];
        const auto& deck1 = decks[(seed / decks.size()) % decks.size()];
        const CardTable table(db, deck0, deck1);
        GameState game = pokebot::new_game(table, seed);

        for (int step = 0; step < 100 && game.turn == 0; ++step) {
            const auto actions = pokebot::legal_actions(game);
            REQUIRE_FALSE(actions.empty());
            pokebot::apply(game, actions[chooser.below(static_cast<std::uint32_t>(actions.size()))]);
        }

        INFO("seed " << seed);
        REQUIRE(game.turn == 1);
        REQUIRE(game.decision().kind == DecisionKind::MainPhase);
        for (int p = 0; p < 2; ++p) {
            REQUIRE(game.player(p).active.has_value());
            REQUIRE(game.player(p).prizes.size() == 6);
            REQUIRE(game.player(p).bench.size() <= 5);
            REQUIRE(cards_accounted_for(game.player(p)) == 60);
        }
    }
}

TEST_CASE("an illegal action is rejected", "[engine][setup]") {
    const CardDB db = sample_db();
    const CardTable table(db, test::deck({{4, 1}, {56, 6}}), test::deck({{4, 1}, {56, 6}}));
    GameState game = pokebot::new_game(table, 1);
    CHECK_THROWS_AS(pokebot::apply(game, Action{ActionKind::EndTurn}), std::invalid_argument);
}

TEST_CASE("apply_legal plays the same game as apply", "[engine][setup]") {
    const CardDB db = sample_db();
    const CardTable table(db, test::deck({{4, 1}, {56, 6}}), test::deck({{4, 1}, {56, 6}}));
    GameState checked = pokebot::new_game(table, 3);
    GameState trusted = checked;
    for (int step = 0; step < 300 && !checked.is_over(); ++step) {
        const auto actions = pokebot::legal_actions(checked);
        const Action action = actions[static_cast<std::size_t>(step) % actions.size()];
        pokebot::apply(checked, action);
        pokebot::apply_legal(trusted, action);
        REQUIRE(checked == trusted);
    }
}