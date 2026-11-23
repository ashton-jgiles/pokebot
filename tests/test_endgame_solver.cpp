#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "agents/endgame/endgame_solver.hpp"
#include "agents/endgame/expectiminimax.hpp"
#include "agents/greedy/greedy_agent.hpp"
#include "agents/world.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "engine/observation/observation.hpp"
#include "harness/match.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using nlohmann::json;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::EndgameOptions;
using pokebot::EndgameResult;
using pokebot::EndgameSolver;
using pokebot::ExpectiminimaxOptions;
using pokebot::GameState;
using pokebot::kActiveSlot;
using pokebot::Plan;
using pokebot::SolveResult;

namespace {

    CardDef attacker(const char* id, const char* name, int damage, const char* effects) {
        CardDef c = test::basic_pokemon(id, name, 300);
        pokebot::Attack a;
        a.name = "Attack";
        a.cost = {pokebot::EnergyType::Colorless};
        a.damage = damage;
        a.effects = json::parse(effects);
        c.attacks.push_back(a);
        return c;
    }

    CardDef item(const char* id, const char* name, const char* effects) {
        CardDef c;
        c.id = id;
        c.name = name;
        c.kind = pokebot::CardKind::Trainer;
        c.trainer_kind = pokebot::TrainerKind::Item;
        c.text = "test card";
        c.effects = json::parse(effects);
        return c;
    }

    CardDB sample_db() {
        CardDB db;
        db.add(attacker("TST-1", "Tackler", 30, "[]"));
        db.add(test::basic_energy("TST-2", "Darkness Energy", pokebot::EnergyType::Darkness));
        db.add(attacker("TST-3", "Tackler B", 30, "[]"));
        db.add(attacker("TST-4", "Coin Tackler", 30, R"([{"op": "flip_coins"}, {"op": "if", "condition": {"heads_at_least": 1}, "then": [{"op": "damage"}]}])"));
        db.add(item("TST-6", "Draw Two", R"([{"op": "draw", "count": 2}])"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 3}, {4, 4}, {4, 6}, {44, 2}});

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
            test::set_active(game, me, "TST-4");
            test::attach_from_deck(game, me, kActiveSlot, "TST-2");
            test::attach_from_deck(game, them, kActiveSlot, "TST-2");
            test::put_in_hand(game, me, "TST-6");
            seen();
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        void seen() { pokebot::detail::note_visible(game); }

        void trim(int player, std::size_t keep) {
            auto& p = game.player(player);
            while (p.deck.size() > keep) {
                p.discard.push_back(p.deck.pop_back());
            }
            seen();
        }
    };

    ExpectiminimaxOptions small(int depth, bool prune) {
        ExpectiminimaxOptions options;
        options.depth = depth;
        options.root_budget = 100;
        options.root_plans = 6;
        options.budget = 25;
        options.plans = 3;
        options.prune = prune;
        return options;
    }

    EndgameOptions solver_options(int worlds, int depth, long long max_nodes) {
        EndgameOptions options;
        options.worlds = worlds;
        options.max_depth = depth;
        options.max_nodes = max_nodes;
        options.search = small(depth, true);
        return options;
    }

    bool near(double a, double b) {
        return std::abs(a - b) < 1e-9;
    }

} // namespace

TEST_CASE("one world through solve_worlds is the same search as expectiminimax", "[endgame]") {
    Fixture f;
    f.trim(f.me, 5);
    f.trim(f.them, 5);
    const ExpectiminimaxOptions options = small(2, true);
    const SolveResult one = pokebot::solve_worlds({&f.game}, pokebot::root_moves(f.game, options), options);
    const SolveResult direct = pokebot::expectiminimax(f.game, options);
    REQUIRE(one.plans.size() == direct.plans.size());
    CHECK(one.best == direct.best);
    CHECK(near(one.value, direct.value));
    CHECK(pokebot::visited(one.stats) == pokebot::visited(direct.stats));
}

TEST_CASE("two copies of one world give the same values as the world alone", "[endgame]") {
    Fixture f;
    f.trim(f.me, 5);
    f.trim(f.them, 5);
    const ExpectiminimaxOptions options = small(2, false);
    const std::vector<Plan> plans = pokebot::root_moves(f.game, options);
    const SolveResult one = pokebot::solve_worlds({&f.game}, plans, options);
    const SolveResult two = pokebot::solve_worlds({&f.game, &f.game}, plans, options);
    REQUIRE(one.plans.size() == two.plans.size());
    for (std::size_t i = 0; i < one.plans.size(); ++i) {
        CHECK(near(one.plans[i].value, two.plans[i].value));
    }
}

TEST_CASE("over several worlds, pruning at the root and below agrees with the search without it", "[endgame]") {
    Fixture f;
    const pokebot::Observation view = pokebot::observe(f.game, f.me);
    pokebot::Rng deal(9);
    std::vector<std::unique_ptr<pokebot::World>> worlds;
    std::vector<const GameState*> games;
    for (int i = 0; i < 3; ++i) {
        worlds.push_back(std::make_unique<pokebot::World>(view, deal));
        games.push_back(&worlds.back()->game());
    }
    std::vector<Plan> plans;
    for (const Plan& plan : pokebot::root_moves(*games.front(), small(2, true))) {
        if (std::all_of(games.begin(), games.end(), [&](const GameState* game) {
                GameState walk = *game;
                return pokebot::apply_plan_checked(walk, plan);
            })) {
            plans.push_back(plan);
        }
    }
    REQUIRE(plans.size() > 1);
    const SolveResult brute = pokebot::solve_worlds(games, plans, small(2, false));
    const SolveResult pruned = pokebot::solve_worlds(games, plans, small(2, true));
    CHECK(near(brute.value, pruned.value));
    CHECK(near(brute.plans[pruned.best].value, brute.value));
    CHECK(pokebot::visited(pruned.stats) <= pokebot::visited(brute.stats));
}

TEST_CASE("a node budget stops the search, and the result says it isn't complete", "[endgame]") {
    Fixture f;
    ExpectiminimaxOptions options = small(2, true);
    options.max_nodes = 5;
    const SolveResult r = pokebot::solve_worlds({&f.game}, pokebot::root_moves(f.game, options), options);
    CHECK(!r.complete);
    CHECK(pokebot::visited(r.stats) <= 10);
}

TEST_CASE("the solver deepens to its depth without a budget, and stops sooner with a small one", "[endgame]") {
    Fixture f;
    const pokebot::Observation view = pokebot::observe(f.game, f.me);
    EndgameSolver unlimited(nullptr, 1, solver_options(2, 2, 0));
    const EndgameResult full = unlimited.solve(view);
    CHECK(full.depth == 2);
    REQUIRE(!full.plans.empty());
    CHECK(full.plans[full.best].value == full.value);
    CHECK(unlimited.stats().iterations == 2);
    EndgameSolver limited(nullptr, 1, solver_options(2, 2, 40));
    const EndgameResult partial = limited.solve(view);
    CHECK(partial.depth < 2);
    CHECK(partial.visited <= 50);
}

TEST_CASE("the solver takes a Knock Out of the last Pokemon", "[endgame]") {
    Fixture f;
    test::set_active(f.game, f.me, "TST-1");
    test::attach_from_deck(f.game, f.me, kActiveSlot, "TST-2");
    test::clear_bench(f.game, f.them);
    f.game.player(f.them).active->damage_counters = 27;
    f.seen();
    EndgameSolver solver(nullptr, 3, solver_options(3, 2, 0));
    const EndgameResult r = solver.solve(pokebot::observe(f.game, f.me));
    REQUIRE(!r.plans.empty());
    CHECK(r.value == 1.0);
    CHECK(r.plans[r.best].plan.actions.back().kind == ActionKind::Attack);
}

TEST_CASE("the same seed solves the same", "[endgame]") {
    Fixture f;
    const pokebot::Observation view = pokebot::observe(f.game, f.me);
    EndgameSolver a(nullptr, 7, solver_options(2, 2, 0));
    EndgameSolver b(nullptr, 7, solver_options(2, 2, 0));
    const EndgameResult ra = a.solve(view);
    const EndgameResult rb = b.solve(view);
    REQUIRE(ra.plans.size() == rb.plans.size());
    CHECK(ra.best == rb.best);
    CHECK(ra.value == rb.value);
    CHECK(ra.visited == rb.visited);
}

TEST_CASE("on the real decks, the solver searches endgames from the searcher's observation", "[endgame]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::size_t n = real->decks.size();
    int solved = 0;
    for (std::uint64_t seed = 1; seed <= 4 && solved < 2; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table(real->db, real->decks[i % n], real->decks[(i + 1) % n]);
        pokebot::GreedyAgent first(seed * 2, 1);
        pokebot::GreedyAgent second(seed * 2 + 1, 1);
        bool done = false;
        pokebot::MatchOptions match;
        match.check_invariants = false;
        match.on_action = [&](const GameState& before, const pokebot::Action&, const GameState&) {
            const auto prizes = std::min(before.player(0).prizes.size(), before.player(1).prizes.size());
            if (done || before.turn == 0 || prizes > 2 || before.decision().kind != pokebot::DecisionKind::MainPhase) {
                return;
            }
            done = true;
            EndgameSolver solver(nullptr, seed, solver_options(2, 2, 0));
            const EndgameResult r = solver.solve(pokebot::observe(before, before.decision().player));
            CHECK(r.value >= -1.0);
            CHECK(r.value <= 1.0);
            if (r.plans.size() > 1) {
                CHECK(r.depth == 2);
                CHECK(r.plans[r.best].value == r.value);
            }
            CHECK(solver.stats().solves == 1);
            ++solved;
        };
        pokebot::play_game(table, seed, first, second, match);
    }
    CHECK(solved > 0);
}
