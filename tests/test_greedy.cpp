#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "agents/greedy_agent.hpp"
#include "agents/random_agent.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "engine/observation/observation.hpp"
#include "harness/match.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using pokebot::Action;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::GameState;
using pokebot::GreedyAgent;
using pokebot::kActiveSlot;
using pokebot::observe;

namespace {

    // A Basic whose only attack does 30 for one Energy.
    CardDef tackler(const char* id, const char* name, int hp) {
        CardDef c = test::basic_pokemon(id, name, hp);
        pokebot::Attack tackle;
        tackle.name = "Tackle";
        tackle.cost = {pokebot::EnergyType::Colorless};
        tackle.damage = 30;
        c.attacks.push_back(tackle);
        return c;
    }

    // Tacklers with 60 HP under four names (a deck may hold only 4 of each), a sturdier 100 HP one, and Energy.
    CardDB sample_db() {
        CardDB db;
        db.add(tackler("TST-1", "Tackler", 60));
        db.add(test::basic_energy("TST-2", "Darkness Energy", pokebot::EnergyType::Darkness));
        db.add(tackler("TST-3", "Tackler B", 60));
        db.add(tackler("TST-4", "Tackler C", 60));
        db.add(tackler("TST-5", "Tackler D", 60));
        db.add(tackler("TST-6", "Big Tackler", 100));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 3}, {4, 4}, {4, 5}, {4, 6}, {40, 2}});

    // Turn 4, each player with one 60 HP Tackler Active, no damage, no Energy and a Benched Tackler.
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
            }
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        Action choose(GreedyAgent& agent) {
            pokebot::detail::note_visible(game); // the cards moved by hand above are in view
            return agent.choose(observe(game, me), pokebot::legal_actions(game));
        }
    };

    bool is(const Action& action, ActionKind kind) {
        return action.kind == kind;
    }

} // namespace

TEST_CASE("greedy takes a Knock Out when it can", "[greedy]") {
    Fixture f;
    test::attach_from_deck(f.game, f.me, kActiveSlot, "TST-2");
    f.game.flags.energy_attached = true;
    f.game.player(f.them).active->damage_counters = 3;
    GreedyAgent agent(1);
    CHECK(is(f.choose(agent), ActionKind::Attack));
}

TEST_CASE("greedy attaches Energy rather than ending the turn", "[greedy]") {
    Fixture f;
    test::put_in_hand(f.game, f.me, "TST-2");
    GreedyAgent agent(1);
    const Action action = f.choose(agent);
    CHECK_FALSE(is(action, ActionKind::EndTurn));

    pokebot::detail::note_visible(f.game);
    const pokebot::Observation view = observe(f.game, f.me);
    const auto actions = pokebot::legal_actions(f.game);
    const auto scores = agent.scores(view, actions);
    REQUIRE(scores.size() == actions.size());
    double attach = -2.0;
    double end = -2.0;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        if (is(actions[i], ActionKind::AttachEnergy) && actions[i].target == kActiveSlot) {
            attach = scores[i];
        }
        if (is(actions[i], ActionKind::EndTurn)) {
            end = scores[i];
        }
    }
    CHECK(attach > end);
}

TEST_CASE("greedy scores an Ability by what it does, not by the choice it leads to", "[greedy]") {
    // Trader's Trade: discard a card from your hand, then draw 2. With an empty deck it only loses a card.
    CardDB db = sample_db();
    CardDef trader = test::basic_pokemon("TST-7", "Trader", 100);
    pokebot::Ability trade;
    trade.name = "Trade";
    trade.text = "test ability";
    trade.effects = nlohmann::json::parse(R"([{"op": "discard_from_hand"}, {"op": "draw", "count": 2}])");
    trader.abilities.push_back(trade);
    db.add(trader);
    const pokebot::Decklist deck = test::deck({{4, 1}, {4, 3}, {4, 4}, {4, 5}, {4, 7}, {40, 2}});
    const CardTable table(db, deck, deck);
    GameState game = pokebot::new_game(table, 2);
    test::play_setup(game);
    test::end_turns(game, 3);
    const int me = game.current_player;
    test::clear_bench(game, me);
    test::set_active(game, me, "TST-7");
    auto& mine = game.player(me);
    while (mine.hand.size() > 4) { // a small hand, so every card in it counts
        mine.discard.push_back(mine.hand.back());
        mine.hand.erase_at(mine.hand.size() - 1);
    }
    for (const auto card : mine.deck) {
        mine.discard.push_back(card);
    }
    mine.deck.clear();
    auto& theirs = game.player(pokebot::opponent_of(me));
    while (theirs.hand.size() < 10) { // a full hand, so their draw after you end the turn changes nothing
        theirs.hand.push_back(theirs.deck.back());
        theirs.deck.erase_at(theirs.deck.size() - 1);
    }
    pokebot::detail::note_visible(game);

    const pokebot::Observation view = observe(game, me);
    const auto actions = pokebot::legal_actions(game);
    GreedyAgent agent(5);
    const auto scores = agent.scores(view, actions);
    bool found = false;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        if (is(actions[i], ActionKind::UseAbility)) {
            found = true;
            CHECK(scores[i] < pokebot::evaluate(game, me) - 0.01); // the discard counts, though it's chosen after
        }
    }
    REQUIRE(found);
    CHECK_FALSE(is(agent.choose(view, actions), ActionKind::UseAbility));
}

TEST_CASE("during setup greedy goes first and puts down its Basics with the most HP", "[greedy]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        GameState game = pokebot::new_game(table, seed);
        GreedyAgent agent(seed);
        while (game.turn == 0) {
            const auto actions = pokebot::legal_actions(game);
            const pokebot::Decision decision = game.decision();
            const Action action = pokebot::ask(agent, game, actions);
            if (decision.kind == pokebot::DecisionKind::ChooseTurnOrder) {
                CHECK(action.amount == 0);
            }
            if (decision.kind == pokebot::DecisionKind::SetupActive) {
                for (const Action& other : actions) {
                    CHECK(game.def(other.card).hp <= game.def(action.card).hp);
                }
            }
            if (decision.kind == pokebot::DecisionKind::SetupBench && actions.size() > 1) {
                CHECK_FALSE(is(action, ActionKind::SetupDone)); // a Basic is left to put down
            }
            pokebot::apply(game, action);
        }
    }
}

TEST_CASE("the same seed makes greedy play the same game", "[greedy]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    const auto play = [&] {
        GreedyAgent first(3);
        GreedyAgent second(4);
        return pokebot::play_game(table, 9, first, second).moves;
    };
    CHECK(play() == play());
}

TEST_CASE("greedy plays whole games on the sample cards without breaking an invariant", "[greedy]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        GreedyAgent greedy(seed);
        pokebot::RandomAgent random(seed);
        INFO("seed " << seed);
        const auto record = pokebot::play_game(table, seed, greedy, random); // checks invariants after every action
        CHECK(record.end != pokebot::GameEnd::ActionLimit);
    }
}

TEST_CASE("on the real decks, greedy beats random in most games from either seat", "[greedy]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::size_t n = real->decks.size();
    int wins = 0;
    int games = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table = CardTable(real->db, real->decks[i % n], real->decks[(i / n) % n]).shuffled(~seed);
        const int seat = static_cast<int>(seed % 2);
        GreedyAgent greedy(seed);
        pokebot::RandomAgent random(seed);
        INFO("seed " << seed);
        const auto record = seat == 0 ? pokebot::play_game(table, seed, greedy, random) : pokebot::play_game(table, seed, random, greedy);
        const auto won = seat == 0 ? pokebot::GameResult::Player0Wins : pokebot::GameResult::Player1Wins;
        wins += record.result == won ? 1 : 0;
        ++games;
    }
    INFO(wins << " of " << games);
    CHECK(wins * 4 >= games * 3);
}
