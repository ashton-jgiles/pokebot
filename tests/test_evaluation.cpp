#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "agents/evaluation/evaluation.hpp"
#include "agents/random/random_agent.hpp"
#include "agents/world.hpp"
#include "engine/engine.hpp"
#include "engine/observation/observation.hpp"
#include "harness/match.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using Catch::Approx;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::evaluate;
using pokebot::Feature;
using pokebot::features;
using pokebot::GameState;
using pokebot::kActiveSlot;
using pokebot::Rng;

namespace {

    double at(const pokebot::Features& phi, Feature feature) {
        return phi[static_cast<std::size_t>(feature)];
    }

    // A 60 HP Basic whose only attack does 30 for one Energy.
    CardDef tackler(const char* id, const char* name) {
        CardDef c = test::basic_pokemon(id, name);
        pokebot::Attack tackle;
        tackle.name = "Tackle";
        tackle.cost = {pokebot::EnergyType::Colorless};
        tackle.damage = 30;
        c.attacks.push_back(tackle);
        return c;
    }

    // Four Tacklers under different names (a deck may hold only 4 of each), and Energy for them.
    CardDB sample_db() {
        CardDB db;
        db.add(tackler("TST-1", "Tackler"));
        db.add(test::basic_energy("TST-2", "Darkness Energy", pokebot::EnergyType::Darkness));
        db.add(tackler("TST-3", "Tackler B"));
        db.add(tackler("TST-4", "Tackler C"));
        db.add(tackler("TST-5", "Tackler D"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 3}, {4, 4}, {4, 5}, {44, 2}});

    // Turn 4, each player with one Tackler, Active, no damage and no Energy.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 2);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 3);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            for (const int p : {me, them}) {
                test::clear_bench(game, p);
                test::set_active(game, p, "TST-1");
            }
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;
    };

} // namespace

TEST_CASE("a finished game scores 1 for the winner, -1 for the loser and 0 for a draw", "[evaluation]") {
    Fixture f;
    GameState game = f.game;
    game.result = pokebot::GameResult::Player0Wins;
    CHECK(evaluate(game, 0) == 1.0);
    CHECK(evaluate(game, 1) == -1.0);
    game.result = pokebot::GameResult::Player1Wins;
    CHECK(evaluate(game, 0) == -1.0);
    CHECK(evaluate(game, 1) == 1.0);
    game.result = pokebot::GameResult::Draw;
    CHECK(evaluate(game, 0) == 0.0);
    CHECK_THROWS_AS(pokebot::utility(f.game, 0), std::logic_error);
}

TEST_CASE("an even position scores 0, and each feature is i's side minus the opponent's", "[evaluation]") {
    Fixture f;
    CHECK(evaluate(f.game, f.me) == Approx(0.0).margin(0.15)); // only hand and deck sizes differ
    const auto phi = features(f.game, f.me);
    CHECK(at(phi, Feature::Prizes) == 0.0);
    CHECK(at(phi, Feature::ActiveHp) == 0.0);
    CHECK(at(phi, Feature::TurnsToKnockout) == 0.0);
    CHECK(at(phi, Feature::ReadyAttackers) == 0.0);
    CHECK(at(phi, Feature::Energy) == 0.0);
    CHECK(at(phi, Feature::Bench) == 0.0);
    CHECK(at(phi, Feature::Conditions) == 0.0);
    CHECK(at(phi, Feature::PrizesAtRisk) == 0.0);

    pokebot::Weights zero{};
    CHECK(evaluate(f.game, f.me, zero) == 0.0);
}

TEST_CASE("taking a Prize card, damaging the opponent and attaching Energy each score higher", "[evaluation]") {
    Fixture f;
    const double before = evaluate(f.game, f.me);

    GameState prize = f.game;
    auto& mine = prize.player(f.me);
    mine.hand.push_back(mine.prizes.back());
    mine.prizes.erase_at(mine.prizes.size() - 1);
    CHECK(at(features(prize, f.me), Feature::Prizes) == Approx(1.0 / 6.0));
    CHECK(evaluate(prize, f.me) > before);
    CHECK(evaluate(prize, f.them) < -before);

    GameState damaged = f.game;
    damaged.player(f.them).active->damage_counters = 3;
    const auto phi = features(damaged, f.me);
    CHECK(at(phi, Feature::ActiveHp) == Approx(0.5));
    CHECK(at(phi, Feature::TurnsToKnockout) == Approx(1.0 / 5.0)); // 2 Tackles for them, 1 for you
    CHECK(evaluate(damaged, f.me) > before);

    GameState powered = f.game;
    test::attach_from_deck(powered, f.me, kActiveSlot, "TST-2");
    CHECK(at(features(powered, f.me), Feature::ReadyAttackers) == Approx(1.0 / 3.0));
    CHECK(at(features(powered, f.me), Feature::Energy) == Approx(1.0 / 10.0));
    CHECK(evaluate(powered, f.me) > before);

    GameState poisoned = f.game;
    poisoned.player(f.me).active->special.poison = 1;
    CHECK(at(features(poisoned, f.me), Feature::Conditions) == Approx(-1.0 / 3.0));
    CHECK(evaluate(poisoned, f.me) < before);
}

TEST_CASE("a player choosing a new Active is scored by their Benched Pokemon with the most HP left", "[evaluation]") {
    Fixture f;
    GameState game = f.game;
    auto& theirs = game.player(f.them);
    test::add_to_bench(game, f.them, "TST-3");
    test::add_to_bench(game, f.them, "TST-4");
    theirs.bench[0].damage_counters = 4;
    theirs.bench[1].damage_counters = 2;
    for (const auto card : theirs.active->stack) {
        theirs.discard.push_back(card);
    }
    theirs.active.reset();
    CHECK(at(features(game, f.me), Feature::ActiveHp) == Approx(1.0 - 4.0 / 6.0));
}

TEST_CASE("cards in hand count up to 7 and cards in the deck up to 30", "[evaluation]") {
    Fixture f;
    GameState game = f.game;
    auto& mine = game.player(f.me);
    auto& theirs = game.player(f.them);
    const auto hand_of = [](pokebot::PlayerState& p, std::size_t size) { // moves cards between hand and deck
        while (p.hand.size() > size) {
            p.deck.push_back(p.hand.back());
            p.hand.erase_at(p.hand.size() - 1);
        }
        while (p.hand.size() < size) {
            p.hand.push_back(p.deck.back());
            p.deck.erase_at(p.deck.size() - 1);
        }
    };
    const auto deck_of = [](pokebot::PlayerState& p, std::size_t size) { // discards from the deck
        while (p.deck.size() > size) {
            p.discard.push_back(p.deck.back());
            p.deck.erase_at(p.deck.size() - 1);
        }
    };
    const auto hand = [&] {
        return at(features(game, f.me), Feature::Hand);
    };
    const auto deck = [&] {
        return at(features(game, f.me), Feature::Deck);
    };

    hand_of(mine, 7);
    hand_of(theirs, 7);
    CHECK(hand() == 0.0);
    hand_of(mine, 10);
    CHECK(hand() == 0.0); // the 3 extra cards are worth nothing
    hand_of(mine, 5);
    CHECK(hand() == Approx(-2.0 / 7.0));

    REQUIRE(mine.deck.size() >= 35);
    REQUIRE(theirs.deck.size() >= 30);
    deck_of(mine, 35);
    deck_of(theirs, 30);
    CHECK(deck() == 0.0); // past 30, more cards don't count
    deck_of(mine, 20);
    CHECK(deck() == Approx(-10.0 / 30.0));
}

TEST_CASE("in random games the two players' features are opposites and stay in [-1, 1]", "[evaluation]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    Rng chooser(6);
    for (std::uint64_t seed = 0; seed < 30; ++seed) {
        GameState game = pokebot::new_game(table, seed);
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            if (game.turn > 0) {
                INFO("seed " << seed << ", step " << step);
                const auto zero = features(game, 0);
                const auto one = features(game, 1);
                for (std::size_t i = 0; i < pokebot::kFeatureCount; ++i) {
                    INFO(pokebot::to_string(static_cast<Feature>(i)));
                    REQUIRE(zero[i] == -one[i]);
                    REQUIRE(std::abs(zero[i]) <= 1.0);
                }
                const double e = evaluate(game, 0);
                REQUIRE(std::abs(e) < 1.0);
                REQUIRE(evaluate(game, 1) == Approx(-e));
            }
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply(game, actions[chooser.below(static_cast<std::uint32_t>(actions.size()))]);
        }
    }
}

TEST_CASE("on the real decks, a world gives the same evaluation as the game it came from", "[evaluation]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::size_t n = real->decks.size();
    Rng rng(4);
    int compared = 0;
    for (std::uint64_t seed = 0; seed < 48; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table = CardTable(real->db, real->decks[i % n], real->decks[(i / n) % n]).shuffled(~seed);
        pokebot::BiasedRandomAgent agent(seed);
        GameState game = pokebot::new_game(table, seed);
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            if (game.turn > 0) {
                for (int viewer = 0; viewer < pokebot::kPlayers; ++viewer) {
                    INFO("seed " << seed << ", step " << step << ", viewer " << viewer);
                    const pokebot::World world(pokebot::observe(game, viewer), rng);
                    for (const int player : {viewer, pokebot::opponent_of(viewer)}) {
                        REQUIRE(features(world.game(), player) == features(game, player));
                        REQUIRE(evaluate(world.game(), player) == evaluate(game, player));
                    }
                    ++compared;
                }
            }
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply_legal(game, pokebot::ask(agent, game, actions));
        }
    }
    CHECK(compared > 1000);
}
