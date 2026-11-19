#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/planner_agent.hpp"
#include "agents/random_agent.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "engine/observation/observation.hpp"
#include "harness/match.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using pokebot::Action;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::GameState;
using pokebot::PlannerAgent;

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

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 3}, {4, 4}, {4, 5}, {44, 2}});

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
                test::add_to_bench(game, p, "TST-3");
                pokebot::detail::shuffle_deck(game, p);
            }
            auto& hand = game.player(me).hand;
            while (!hand.empty()) {
                game.player(me).discard.push_back(hand.pop_back());
            }
            seen();
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        void seen() { pokebot::detail::note_visible(game); }

        void give(const char* id) {
            test::put_in_hand(game, me, id);
            seen();
        }

        void play_turn(pokebot::Agent& agent) {
            const auto turn = game.turn;
            while (!game.is_over() && game.turn == turn && game.decision().player == me) {
                const auto actions = pokebot::legal_actions(game);
                pokebot::apply(game, agent.choose(pokebot::observe(game, me), actions));
            }
        }
    };

} // namespace

TEST_CASE("the planner takes a win two actions away", "[planner]") {
    Fixture f;
    test::clear_bench(f.game, f.them);
    test::set_active(f.game, f.me, "TST-5");
    test::attach_from_deck(f.game, f.me, pokebot::bench_slot(0), "TST-2");
    f.game.player(f.them).active->damage_counters = 3;
    f.seen();
    PlannerAgent planner(1);
    f.play_turn(planner);
    CHECK(f.game.result == (f.me == 0 ? pokebot::GameResult::Player0Wins : pokebot::GameResult::Player1Wins));
    CHECK(planner.stats().wins == 1);
}

TEST_CASE("with no win, the planner plays the and-tree's best plan to the end", "[planner]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-4");
    PlannerAgent planner(1);
    f.play_turn(planner);
    CHECK(f.game.player(f.me).prizes.size() == 5);
    CHECK(planner.stats().plans == 1);
    CHECK(planner.stats().wins == 0);
    CHECK(planner.stats().followed == 3);
}

TEST_CASE("the planner plans again when the game isn't what its plan expected", "[planner]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-4");
    PlannerAgent planner(1);
    pokebot::apply(f.game, planner.choose(pokebot::observe(f.game, f.me), pokebot::legal_actions(f.game)));
    CHECK(planner.stats().plans == 1);
    f.give("TST-2");
    pokebot::apply(f.game, planner.choose(pokebot::observe(f.game, f.me), pokebot::legal_actions(f.game)));
    CHECK(planner.stats().plans == 2);
}

TEST_CASE("the planner plays whole games on the sample cards, the same for the same seed", "[planner]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    const auto play = [&](std::uint64_t seed) {
        PlannerAgent planner(seed);
        pokebot::RandomAgent random(seed);
        return pokebot::play_game(table, seed, planner, random);
    };
    for (std::uint64_t seed = 0; seed < 4; ++seed) {
        INFO("seed " << seed);
        const auto record = play(seed);
        CHECK(record.end != pokebot::GameEnd::ActionLimit);
        CHECK(play(seed).moves == record.moves);
    }
}

TEST_CASE("on the real decks, the planner plays whole games against random", "[planner]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    pokebot::PlannerOptions options;
    options.or_budget = 50;
    options.and_budget = 100;
    for (std::uint64_t seed = 0; seed < 2; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table(real->db, real->decks[i % real->decks.size()], real->decks[(i + 1) % real->decks.size()]);
        PlannerAgent planner(seed, options);
        pokebot::RandomAgent random(seed);
        INFO("seed " << seed);
        const auto record = pokebot::play_game(table, seed, planner, random);
        CHECK(record.end != pokebot::GameEnd::ActionLimit);
        CHECK(planner.stats().plans > 0);
    }
}
