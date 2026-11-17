#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine/engine.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardTable;
using pokebot::DecisionKind;
using pokebot::EnergyType;
using pokebot::GameState;
using pokebot::kActiveSlot;
using pokebot::Stage;

namespace {

    // TST-1 Testmon (Basic, retreat 1) -> TST-2 Testvolve (Stage 1) -> TST-3 Testmax (Stage 2).
    // TST-4 Freemon (Basic, free retreat). TST-6 Darkness Energy, TST-7 Fire Energy.
    CardDB sample_db() {
        CardDB db;
        db.add(test::basic_pokemon("TST-1", "Testmon", 60, 1));
        db.add(test::evolution("TST-2", "Testvolve", Stage::Stage1, "Testmon"));
        db.add(test::evolution("TST-3", "Testmax", Stage::Stage2, "Testvolve"));
        db.add(test::basic_pokemon("TST-4", "Freemon", 60, 0));
        db.add(test::basic_pokemon("TST-5", "Benchmon", 60, 1));
        db.add(test::basic_energy("TST-6", "Darkness Energy", EnergyType::Darkness));
        db.add(test::basic_energy("TST-7", "Fire Energy", EnergyType::Fire));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {4, 4}, {4, 5}, {20, 6}, {20, 7}});

    // A game just past setup, on turn 1. Members are built in order, so `table` can point at `db`.
    // Not copyable: the game points at the table and the table at the database.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 7);

        Fixture() { test::play_setup(game); }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;
    };

    void check_all_cards(const GameState& game) {
        CHECK(test::cards_accounted_for(game.player(0)) == 60);
        CHECK(test::cards_accounted_for(game.player(1)) == 60);
    }

} // namespace

TEST_CASE("Basic Pokemon can be benched until the Bench is full", "[engine][actions]") {
    Fixture f;
    GameState& game = f.game;
    const int me = game.current_player;
    test::clear_bench(game, me);
    // Fill 4 Bench spots with Freemon and Benchmon; some copies may be among the Prize cards.
    for (const char* id : {"TST-4", "TST-4", "TST-4", "TST-4", "TST-5", "TST-5", "TST-5", "TST-5"}) {
        if (game.player(me).bench.size() < 4 && test::has_card(game, me, id)) {
            test::add_to_bench(game, me, id);
        }
    }
    REQUIRE(game.player(me).bench.size() == 4);
    test::put_in_hand(game, me, "TST-1");
    test::put_in_hand(game, me, "TST-1");

    // Two copies in hand are one choice.
    int testmon_choices = 0;
    for (const auto& action : pokebot::legal_actions(game)) {
        if (action.kind == ActionKind::PlayBasic && game.def(action.card).id == "TST-1") {
            ++testmon_choices;
        }
    }
    CHECK(testmon_choices == 1);

    pokebot::apply(game, test::find_action(game, ActionKind::PlayBasic, "TST-1"));
    CHECK(game.player(me).bench.size() == 5);
    CHECK(game.player(me).bench.back().turn_played == game.turn);
    CHECK_FALSE(test::offers(pokebot::legal_actions(game), ActionKind::PlayBasic));
    check_all_cards(game);
}

TEST_CASE("only one Energy can be attached per turn", "[engine][actions]") {
    Fixture f;
    GameState& game = f.game;
    const int me = game.current_player;
    test::put_in_hand(game, me, "TST-6");
    test::put_in_hand(game, me, "TST-6");
    test::put_in_hand(game, me, "TST-7");

    // Each (Energy card, Pokemon) pair is offered once, even with two Darkness Energy in hand.
    std::set<std::pair<std::string, int>> pairs;
    for (const auto& action : pokebot::legal_actions(game)) {
        if (action.kind == ActionKind::AttachEnergy) {
            CHECK(pairs.insert({game.def(action.card).id, action.target}).second);
        }
    }

    const auto energy_before = game.player(me).active->energy.size();
    pokebot::apply(game, test::find_action(game, ActionKind::AttachEnergy, "TST-6", kActiveSlot));
    CHECK(game.player(me).active->energy.size() == energy_before + 1);
    CHECK(game.flags.energy_attached);
    CHECK_FALSE(test::offers(pokebot::legal_actions(game), ActionKind::AttachEnergy));

    test::end_turns(game, 2); // back to this player's next turn
    CHECK(test::offers(pokebot::legal_actions(game), ActionKind::AttachEnergy));
    check_all_cards(game);
}

TEST_CASE("nobody evolves on their first turn", "[engine][actions]") {
    Fixture f;
    GameState& game = f.game;

    for (int turn = 1; turn <= 2; ++turn) {
        const int me = game.current_player;
        test::set_active(game, me, "TST-1");
        test::put_in_hand(game, me, "TST-2");
        CHECK_FALSE(test::offers(pokebot::legal_actions(game), ActionKind::Evolve));
        test::end_turns(game, 1);
    }

    // Turn 3: the first player's second turn. Their Active has been in play since setup.
    const int me = game.current_player;
    test::set_active(game, me, "TST-1");
    test::put_in_hand(game, me, "TST-2");
    CHECK(test::offers(pokebot::legal_actions(game), ActionKind::Evolve));
}

TEST_CASE("a Pokemon can't evolve the turn it was put into play", "[engine][actions]") {
    Fixture f;
    GameState& game = f.game;
    test::end_turns(game, 2);
    const int me = game.current_player;
    test::set_active(game, me, "TST-1");
    test::clear_bench(game, me);
    test::put_in_hand(game, me, "TST-1");
    test::put_in_hand(game, me, "TST-2");

    pokebot::apply(game, test::find_action(game, ActionKind::PlayBasic, "TST-1"));
    for (const auto& action : pokebot::legal_actions(game)) {
        if (action.kind == ActionKind::Evolve) {
            CHECK(action.target == kActiveSlot); // never the Testmon just benched
        }
    }
}

TEST_CASE("evolving keeps Energy and damage, and the new Stage waits a turn", "[engine][actions]") {
    Fixture f;
    GameState& game = f.game;
    test::end_turns(game, 2);
    const int me = game.current_player;
    test::set_active(game, me, "TST-1");
    test::attach_from_deck(game, me, kActiveSlot, "TST-6");
    game.player(me).active->damage_counters = 2;
    test::put_in_hand(game, me, "TST-2");
    test::put_in_hand(game, me, "TST-3");

    pokebot::apply(game, test::find_action(game, ActionKind::Evolve, "TST-2", kActiveSlot));
    const auto& active = *game.player(me).active;
    CHECK(active.stack.size() == 2);
    CHECK(game.def(active.top()).id == "TST-2");
    CHECK(active.energy.size() == 1);
    CHECK(active.damage_counters == 2);
    CHECK(active.turn_played == game.turn);
    for (const auto& action : pokebot::legal_actions(game)) {
        if (action.kind == ActionKind::Evolve) {
            CHECK(action.target != kActiveSlot); // Testmax must wait (a Benched Testmon may still evolve)
        }
    }

    test::end_turns(game, 2);
    pokebot::apply(game, test::find_action(game, ActionKind::Evolve, "TST-3", kActiveSlot));
    CHECK(game.def(game.player(me).active->top()).id == "TST-3");
    check_all_cards(game);
}

TEST_CASE("retreating with a choice of Energy asks which to discard", "[engine][actions]") {
    Fixture f;
    GameState& game = f.game;
    const int me = game.current_player;
    test::set_active(game, me, "TST-1"); // Retreat Cost 1
    test::attach_from_deck(game, me, kActiveSlot, "TST-6");
    test::attach_from_deck(game, me, kActiveSlot, "TST-7");
    test::clear_bench(game, me);
    test::add_to_bench(game, me, "TST-4");

    pokebot::apply(game, test::find_action(game, ActionKind::Retreat));
    REQUIRE(game.decision().kind == DecisionKind::RetreatDiscard);
    CHECK(pokebot::legal_actions(game).size() == 2); // Darkness or Fire

    pokebot::apply(game, test::find_action(game, ActionKind::DiscardEnergy, "TST-7"));
    CHECK(game.decision().kind == DecisionKind::MainPhase);
    CHECK(game.def(game.player(me).active->top()).id == "TST-4");
    const auto& benched = game.player(me).bench[0];
    CHECK(game.def(benched.top()).id == "TST-1");
    REQUIRE(benched.energy.size() == 1);
    CHECK(game.def(benched.energy[0]).id == "TST-6");
    CHECK(game.def(game.player(me).discard.back()).id == "TST-7");
    CHECK(game.flags.retreated);
    CHECK_FALSE(test::offers(pokebot::legal_actions(game), ActionKind::Retreat));
    check_all_cards(game);
}

TEST_CASE("retreating with no real choice discards automatically", "[engine][actions]") {
    Fixture f;
    GameState& game = f.game;
    const int me = game.current_player;
    test::set_active(game, me, "TST-1");
    test::attach_from_deck(game, me, kActiveSlot, "TST-6");
    test::attach_from_deck(game, me, kActiveSlot, "TST-6"); // two identical Energy
    test::clear_bench(game, me);
    test::add_to_bench(game, me, "TST-4");
    const auto discard_before = game.player(me).discard.size();

    pokebot::apply(game, test::find_action(game, ActionKind::Retreat));
    CHECK(game.decision().kind == DecisionKind::MainPhase);
    CHECK(game.player(me).discard.size() == discard_before + 1);
    CHECK(game.player(me).bench[0].energy.size() == 1);
    check_all_cards(game);
}

TEST_CASE("free retreat swaps without discarding", "[engine][actions]") {
    Fixture f;
    GameState& game = f.game;
    const int me = game.current_player;
    test::set_active(game, me, "TST-4"); // Retreat Cost 0
    test::clear_bench(game, me);
    test::add_to_bench(game, me, "TST-1");
    const auto discard_before = game.player(me).discard.size();

    pokebot::apply(game, test::find_action(game, ActionKind::Retreat));
    CHECK(game.def(game.player(me).active->top()).id == "TST-1");
    CHECK(game.player(me).discard.size() == discard_before);
}

TEST_CASE("retreating needs enough Energy and a Benched Pokemon", "[engine][actions]") {
    Fixture f;
    GameState& game = f.game;
    const int me = game.current_player;
    test::set_active(game, me, "TST-1"); // cost 1, no Energy
    test::clear_bench(game, me);
    test::add_to_bench(game, me, "TST-4");
    CHECK_FALSE(test::offers(pokebot::legal_actions(game), ActionKind::Retreat));

    test::attach_from_deck(game, me, kActiveSlot, "TST-6");
    test::clear_bench(game, me);
    CHECK_FALSE(test::offers(pokebot::legal_actions(game), ActionKind::Retreat)); // nobody to switch in
}
TEST_CASE("random games with the real decks keep every card accounted for", "[engine][actions]") {
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

    pokebot::Rng chooser(2024);
    long long actions_played = 0;
    long long attacks = 0, attack_chances = 0, prizes_taken = 0;
    int by_prizes = 0, by_no_pokemon = 0, by_deck_out = 0, draws = 0;
    for (std::uint64_t seed = 0; seed < 200; ++seed) {
        const CardTable table(db, decks[seed % decks.size()], decks[(seed / decks.size()) % decks.size()]);
        GameState game = pokebot::new_game(table, seed);

        // Games end by Prize cards, no Pokemon left, or deck-out.
        for (int step = 0; step < 20000 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            REQUIRE_FALSE(actions.empty());
            if (test::offers(actions, ActionKind::Attack)) {
                ++attack_chances;
            }
            const auto& chosen = actions[chooser.below(static_cast<std::uint32_t>(actions.size()))];
            if (chosen.kind == ActionKind::Attack) {
                ++attacks;
            }
            pokebot::apply(game, chosen);
            ++actions_played;

            INFO("seed " << seed << ", turn " << game.turn);
            for (int p = 0; p < 2; ++p) {
                REQUIRE(test::cards_accounted_for(game.player(p)) == 60);
                REQUIRE(game.player(p).bench.size() <= 5);
            }
        }
        REQUIRE(game.is_over());
        prizes_taken += 12 - static_cast<long long>(game.player(0).prizes.size() + game.player(1).prizes.size());

        if (game.result == pokebot::GameResult::Draw) {
            ++draws;
        } else {
            const int winner = game.result == pokebot::GameResult::Player0Wins ? 0 : 1;
            const auto& loser = game.player(pokebot::opponent_of(winner));
            if (game.player(winner).prizes.empty()) {
                ++by_prizes;
            } else if (!loser.active && loser.bench.empty()) {
                ++by_no_pokemon;
            } else {
                ++by_deck_out;
            }
        }
    }
    INFO(actions_played << " actions played; games won by Prize cards: " << by_prizes << ", no Pokemon left: " << by_no_pokemon
                        << ", deck-out: " << by_deck_out << ", draws: " << draws << "; turns with an attack available: " << attack_chances
                        << ", attacks made: " << attacks << ", Prize cards taken: " << prizes_taken);
    CHECK(actions_played > 0);
}
