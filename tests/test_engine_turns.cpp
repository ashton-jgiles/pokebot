#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "engine/engine.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using pokebot::Action;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardTable;
using pokebot::DecisionKind;
using pokebot::EnergyType;
using pokebot::GameResult;
using pokebot::GameState;
using pokebot::TurnFlags;

namespace {

    const Action kEndTurn{ActionKind::EndTurn, pokebot::kNoCard, pokebot::kActiveSlot, 0};

    CardDB sample_db() {
        CardDB db;
        db.add(test::basic_pokemon("TST-1", "Testmon"));
        db.add(test::basic_energy("TST-6", "Darkness Energy", EnergyType::Darkness));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {56, 6}});

} // namespace

TEST_CASE("ending the turn is always one of the choices", "[engine][turns]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    GameState game = pokebot::new_game(table, 1);
    test::play_setup(game);

    for (int turn = 0; turn < 6; ++turn) {
        const auto actions = pokebot::legal_actions(game);
        REQUIRE_FALSE(actions.empty());
        CHECK(actions.back() == kEndTurn);
        pokebot::apply(game, kEndTurn);
    }
}

TEST_CASE("ending the turn passes it to the opponent, who draws a card", "[engine][turns]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    GameState game = pokebot::new_game(table, 2);
    test::play_setup(game);

    const int second = pokebot::opponent_of(game.current_player);
    const auto hand_before = game.player(second).hand.size();
    const auto deck_before = game.player(second).deck.size();

    pokebot::apply(game, kEndTurn);

    CHECK(game.current_player == second);
    CHECK(game.turn == 2);
    CHECK(game.decision().kind == DecisionKind::MainPhase);
    CHECK(game.decision().player == second);
    CHECK(game.player(second).hand.size() == hand_before + 1);
    CHECK(game.player(second).deck.size() == deck_before - 1);
}

TEST_CASE("turns alternate between the players", "[engine][turns]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    GameState game = pokebot::new_game(table, 3);
    test::play_setup(game);

    const int first = game.first_player;
    for (int turn = 1; turn <= 10; ++turn) {
        CHECK(game.turn == turn);
        CHECK(game.current_player == (turn % 2 == 1 ? first : pokebot::opponent_of(first)));
        pokebot::apply(game, kEndTurn);
    }
}

TEST_CASE("once-per-turn flags reset at the start of each turn", "[engine][turns]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    GameState game = pokebot::new_game(table, 4);
    test::play_setup(game);

    game.flags.energy_attached = true; // as if the player had attached and retreated
    game.flags.retreated = true;
    pokebot::apply(game, kEndTurn);
    CHECK(game.flags == TurnFlags{});
}

TEST_CASE("a player who can't draw at the start of their turn loses", "[engine][turns]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    GameState game = pokebot::new_game(table, 5);
    test::play_setup(game);

    for (int turns = 0; turns < 200 && !game.is_over(); ++turns) {
        pokebot::apply(game, kEndTurn);
    }
    REQUIRE(game.is_over());

    // begin_turn makes the loser the current player before checking the deck.
    const int loser = game.current_player;
    CHECK(game.player(loser).deck.empty());
    CHECK(game.result == (loser == 0 ? GameResult::Player1Wins : GameResult::Player0Wins));
    CHECK(pokebot::legal_actions(game).empty());
    CHECK_THROWS_AS(pokebot::apply(game, kEndTurn), std::logic_error);
    for (int p = 0; p < 2; ++p) {
        CHECK(test::cards_accounted_for(game.player(p)) == 60);
    }
}