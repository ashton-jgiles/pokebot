#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "agents/random_agent.hpp"
#include "engine/engine.hpp"
#include "harness/arena.hpp"
#include "harness/replay.hpp"
#include "support/test_cards.hpp"

using Catch::Approx;
using pokebot::ArenaDeck;
using pokebot::ArenaOptions;
using pokebot::ArenaResult;
using pokebot::CardDB;
using pokebot::Contestant;
using pokebot::GameResult;
using pokebot::Tally;

namespace {

    // Two small decks of Basics and Energy, legal (at most 4 of a name).
    CardDB sample_db() {
        CardDB db;
        db.add(test::basic_pokemon("TST-1", "Basicmon"));
        db.add(test::basic_energy("TST-2", "Darkness Energy", pokebot::EnergyType::Darkness));
        db.add(test::basic_pokemon("TST-3", "Othermon"));
        db.add(test::basic_pokemon("TST-4", "Lastmon", 90));
        return db;
    }

    std::vector<ArenaDeck> sample_decks() {
        return {ArenaDeck{"first.txt", test::deck({{4, 1}, {4, 3}, {52, 2}})}, ArenaDeck{"second.txt", test::deck({{4, 3}, {4, 4}, {52, 2}})}};
    }

    Contestant random_named(const std::string& name) {
        return Contestant{name, [](std::uint64_t seed) { return std::make_unique<pokebot::RandomAgent>(seed); }};
    }

    // An agent that gives up at its first decision.
    class FailingAgent final : public pokebot::Agent {
    public:
        std::string name() const override { return "failing"; }
        pokebot::Action choose(const pokebot::Observation&, const std::vector<pokebot::Action>&) override {
            throw std::runtime_error("this agent always fails");
        }
    };

} // namespace

TEST_CASE("the Wilson interval matches known values", "[arena]") {
    const auto half = pokebot::wilson_interval(50, 100);
    CHECK(half.low == Approx(0.4038).margin(0.0005));
    CHECK(half.high == Approx(0.5962).margin(0.0005));
    const auto most = pokebot::wilson_interval(31, 32);
    CHECK(most.low == Approx(0.843).margin(0.001));
    CHECK(most.high == Approx(0.994).margin(0.001));
    const auto none = pokebot::wilson_interval(0, 10);
    CHECK(none.low == Approx(0.0).margin(1e-12));
    CHECK(none.high == Approx(0.2775).margin(0.0005));
    const auto empty = pokebot::wilson_interval(0, 0);
    CHECK(empty.low == 0.0);
    CHECK(empty.high == 1.0);
}

TEST_CASE("a tally counts results for A from either seat, a draw as half", "[arena]") {
    Tally tally;
    tally.add(GameResult::Player0Wins, 0);
    tally.add(GameResult::Player0Wins, 1);
    tally.add(GameResult::Player1Wins, 1);
    tally.add(GameResult::Draw, 0);
    CHECK(tally.wins == 2);
    CHECK(tally.losses == 1);
    CHECK(tally.draws == 1);
    CHECK(tally.games() == 4);
    CHECK(tally.score() == Approx(2.5 / 4.0));
    CHECK(Tally{}.score() == 0.0);
}

TEST_CASE("an arena plays every deck pairing, each seed with A in both seats, and adds up", "[arena]") {
    const CardDB db = sample_db();
    const auto decks = sample_decks();
    ArenaOptions options;
    options.games = 6;
    options.threads = 3;
    options.replay_every = 1;
    const ArenaResult result = pokebot::run_arena(db, decks, random_named("A"), random_named("B"), options);

    CHECK(result.games == 4 * 6);
    REQUIRE(result.pairings.size() == 4);
    Tally sum;
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(result.pairings[i].deck_a == i / 2);
        CHECK(result.pairings[i].deck_b == i % 2);
        CHECK(result.pairings[i].tally.games() == 6);
        sum.wins += result.pairings[i].tally.wins;
        sum.losses += result.pairings[i].tally.losses;
        sum.draws += result.pairings[i].tally.draws;
    }
    CHECK(sum == result.overall);
    CHECK(result.a_first.games() + result.b_first.games() == result.games);
    long long ended = 0;
    for (const long long count : result.ends) {
        ended += count;
    }
    CHECK(ended == result.games);
    long long won = 0;
    long long lost = 0;
    for (std::size_t i = 0; i < result.ends.size(); ++i) {
        won += result.a_wins_by[i];
        lost += result.a_losses_by[i];
    }
    CHECK(won == result.overall.wins);
    CHECK(lost == result.overall.losses);
    const auto deck_out = static_cast<std::size_t>(pokebot::GameEnd::DeckOut);
    CHECK(result.a_wins_by[deck_out] == result.overall.wins); // the sample Pokemon have no attacks, so every game is a deck-out
    CHECK(result.a_losses_by[deck_out] == result.overall.losses);

    // Every game is kept: seeds come in pairs, with A as player 0 and then as player 1, each with its own deck.
    REQUIRE(result.replays.size() == 24);
    for (std::size_t i = 0; i < result.replays.size(); i += 2) {
        const auto& first = result.replays[i];
        const auto& second = result.replays[i + 1];
        CHECK(first.seed == second.seed);
        CHECK(first.players[0].agent == "A");
        CHECK(second.players[1].agent == "A");
        CHECK(first.players[0].deck_name == second.players[1].deck_name);
        CHECK(first.players[1].deck_name == second.players[0].deck_name);
    }
    CHECK(result.replays[0].players[0].deck_name == "first.txt");
    CHECK(result.replays[6].players[0].deck_name == "first.txt");  // pairing 1: A's first deck
    CHECK(result.replays[6].players[1].deck_name == "second.txt"); // against B's second
}

TEST_CASE("an arena's kept games play back to the same result", "[arena]") {
    const CardDB db = sample_db();
    ArenaOptions options;
    options.games = 4;
    options.replay_every = 3;
    const ArenaResult result = pokebot::run_arena(db, sample_decks(), random_named("A"), random_named("B"), options);
    REQUIRE(result.replays.size() == 6); // games 0, 3, ..., 15
    for (const auto& replay : result.replays) {
        INFO("seed " << replay.seed);
        const auto end = pokebot::play_back(pokebot::replay_table(db, replay), replay);
        CHECK(end.result == replay.result);
    }
}

TEST_CASE("an arena gives the same results whatever the number of threads", "[arena]") {
    const CardDB db = sample_db();
    ArenaOptions options;
    options.games = 10;
    options.seed = 40;
    options.threads = 1;
    const ArenaResult one = pokebot::run_arena(db, sample_decks(), random_named("A"), random_named("B"), options);
    options.threads = 4;
    const ArenaResult four = pokebot::run_arena(db, sample_decks(), random_named("A"), random_named("B"), options);
    CHECK(one.overall == four.overall);
    CHECK(one.a_first == four.a_first);
    CHECK(one.ends == four.ends);
    CHECK(one.turns == four.turns);
    CHECK(one.actions == four.actions);
    for (std::size_t i = 0; i < one.pairings.size(); ++i) {
        CHECK(one.pairings[i].tally == four.pairings[i].tally);
    }
    CHECK(four.threads == 4);
}

TEST_CASE("an agent's failure in one game stops the arena and is passed on", "[arena]") {
    const CardDB db = sample_db();
    const Contestant failing{"failing", [](std::uint64_t) { return std::make_unique<FailingAgent>(); }};
    ArenaOptions options;
    options.games = 4;
    options.threads = 4;
    CHECK_THROWS_WITH(pokebot::run_arena(db, sample_decks(), random_named("A"), failing, options), "this agent always fails");
}
