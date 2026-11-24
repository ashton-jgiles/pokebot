#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/endgame/endgame_agent.hpp"
#include "agents/flat_mc/deck_model.hpp"
#include "agents/random/random_agent.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "engine/observation/observation.hpp"
#include "harness/match.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using pokebot::ArchetypeLibrary;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::DeckModel;
using pokebot::EndgameAgent;
using pokebot::EndgameAgentOptions;
using pokebot::GameState;

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
        ArchetypeLibrary library;
        int me = 0;
        int them = 0;

        Fixture() {
            library.add("sample", "main", kDeck, db);
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

    EndgameAgentOptions small(int prizes, long long max_nodes) {
        EndgameAgentOptions o;
        o.prizes = prizes;
        o.is_mcts.search.iterations = 10;
        o.is_mcts.search.macros.searcher_budget = 100;
        o.is_mcts.search.macros.searcher_plans = 6;
        o.is_mcts.search.macros.opponent_budget = 25;
        o.is_mcts.search.macros.opponent_plans = 3;
        o.solver.worlds = 2;
        o.solver.max_depth = 2;
        o.solver.max_nodes = max_nodes;
        o.solver.search.root_budget = 100;
        o.solver.search.root_plans = 6;
        o.solver.search.budget = 25;
        o.solver.search.plans = 3;
        return o;
    }

} // namespace

TEST_CASE("in the endgame, the agent plays the solver's best plan to the end", "[endgame]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-4");
    const DeckModel model(f.library);
    EndgameAgent agent(1, model, small(6, 0));
    f.play_turn(agent);
    CHECK(agent.stats().endgame == 1);
    CHECK(agent.stats().played == 1);
    CHECK(agent.stats().fallbacks == 0);
    REQUIRE(!agent.analysis().plans.empty());
    CHECK(agent.analysis().depth == 2);
    CHECK(agent.stats().followed == static_cast<long long>(agent.analysis().plans[agent.analysis().best].plan.actions.size()));
    CHECK(agent.is_mcts().search_stats().searches == 0);
}

TEST_CASE("outside the endgame, the agent plays as IS-MCTS", "[endgame]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-4");
    const DeckModel model(f.library);
    EndgameAgent agent(1, model, small(1, 0));
    f.play_turn(agent);
    CHECK(agent.stats().endgame == 0);
    CHECK(agent.solver().stats().solves == 0);
    CHECK(agent.is_mcts().search_stats().searches == 1);
}

TEST_CASE("in the endgame, the agent takes a win", "[endgame]") {
    Fixture f;
    test::clear_bench(f.game, f.them);
    test::set_active(f.game, f.me, "TST-5");
    test::attach_from_deck(f.game, f.me, pokebot::bench_slot(0), "TST-2");
    f.game.player(f.them).active->damage_counters = 3;
    f.seen();
    const DeckModel model(f.library);
    EndgameAgent agent(1, model, small(6, 0));
    f.play_turn(agent);
    CHECK(f.game.result == (f.me == 0 ? pokebot::GameResult::Player0Wins : pokebot::GameResult::Player1Wins));
    CHECK(agent.stats().played == 1);
    CHECK(agent.analysis().value == 1.0);
}

TEST_CASE("when the solver can't finish depth 1 in its budget, the agent falls back on IS-MCTS", "[endgame]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-4");
    const DeckModel model(f.library);
    EndgameAgent agent(1, model, small(6, 1));
    f.play_turn(agent);
    CHECK(agent.stats().endgame == 1);
    CHECK(agent.stats().fallbacks == 1);
    CHECK(agent.stats().played == 0);
    CHECK(agent.is_mcts().search_stats().searches == 1);
}

TEST_CASE("when the solve doesn't reach the minimum depth, the agent falls back on IS-MCTS", "[endgame]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-4");
    const DeckModel model(f.library);
    EndgameAgentOptions options = small(6, 0);
    options.solver.max_depth = 1;
    options.min_depth = 2;
    EndgameAgent agent(1, model, options);
    f.play_turn(agent);
    CHECK(agent.analysis().depth == 1);
    CHECK(agent.stats().fallbacks == 1);
    CHECK(agent.stats().played == 0);
    CHECK(agent.is_mcts().search_stats().searches == 1);
}

TEST_CASE("the endgame agent plays whole games on the sample cards, the same for the same seed", "[endgame]") {
    const CardDB db = sample_db();
    ArchetypeLibrary library;
    library.add("sample", "main", kDeck, db);
    const DeckModel model(library);
    const CardTable table(db, kDeck, kDeck);
    const auto play = [&](std::uint64_t seed) {
        EndgameAgent agent(seed, model, small(3, 2000));
        pokebot::RandomAgent random(seed);
        return pokebot::play_game(table, seed, agent, random);
    };
    for (std::uint64_t seed = 0; seed < 2; ++seed) {
        INFO("seed " << seed);
        const auto record = play(seed);
        CHECK(record.end != pokebot::GameEnd::ActionLimit);
        CHECK(play(seed).moves == record.moves);
    }
}

TEST_CASE("on the real decks, the endgame agent plays whole games against random", "[endgame]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::filesystem::path root = POKEBOT_SOURCE_DIR;
    const ArchetypeLibrary library = ArchetypeLibrary::load(root / "data/archetypes", real->db);
    const DeckModel model(library);
    EndgameAgentOptions options = small(3, 2000);
    options.is_mcts.or_budget = 50;
    long long endgame = 0;
    for (std::uint64_t seed = 3; seed < 5; ++seed) {
        INFO("seed " << seed);
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table(real->db, real->decks[i % real->decks.size()], real->decks[(i + 1) % real->decks.size()]);
        EndgameAgent agent(seed, model, options);
        pokebot::RandomAgent random(seed);
        const auto record = pokebot::play_game(table, seed, agent, random);
        CHECK(record.end != pokebot::GameEnd::ActionLimit);
        endgame += agent.stats().endgame;
    }
    CHECK(endgame > 0);
}

TEST_CASE("the endgame agent's counters come first, then IS-MCTS's", "[endgame]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-4");
    const DeckModel model(f.library);
    EndgameAgent agent(1, model, small(6, 0));
    f.play_turn(agent);
    const auto counters = agent.counters();
    REQUIRE(!counters.empty());
    CHECK(counters.front() == std::pair<std::string, double>{"endgame decisions", 1.0});
    CHECK(std::any_of(counters.begin(), counters.end(), [](const auto& c) { return c.first == "searches"; }));
    CHECK(std::any_of(counters.begin(), counters.end(), [](const auto& c) { return c.first == "solves reaching depth 2" && c.second == 1.0; }));
}
