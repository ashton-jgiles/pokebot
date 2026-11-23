#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "agents/endgame/expectiminimax.hpp"
#include "agents/greedy/greedy_agent.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "harness/match.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using nlohmann::json;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::ExpectiminimaxOptions;
using pokebot::GameState;
using pokebot::kActiveSlot;
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
            seen();
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        void seen() { pokebot::detail::note_visible(game); }

        void power_up(int player) {
            test::attach_from_deck(game, player, kActiveSlot, "TST-2");
            seen();
        }

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

    long long visited(const SolveResult& r) {
        return r.stats.nodes + r.stats.chance_nodes + r.stats.leaves;
    }

} // namespace

TEST_CASE("a Knock Out of the last Pokemon is worth a win", "[endgame]") {
    Fixture f;
    test::clear_bench(f.game, f.them);
    f.game.player(f.them).active->damage_counters = 27;
    f.power_up(f.me);
    const SolveResult r = pokebot::expectiminimax(f.game, small(1, true));
    REQUIRE(!r.plans.empty());
    CHECK(r.value == 1.0);
    CHECK(r.plans[r.best].plan.actions.back().kind == ActionKind::Attack);
}

TEST_CASE("without pruning, every root plan's value is exact and the best is the largest", "[endgame]") {
    Fixture f;
    f.trim(f.me, 5);
    f.trim(f.them, 5);
    f.power_up(f.me);
    test::put_in_hand(f.game, f.me, "TST-6");
    f.seen();
    const SolveResult r = pokebot::expectiminimax(f.game, small(2, false));
    REQUIRE(!r.plans.empty());
    for (const auto& plan : r.plans) {
        CHECK(plan.exact);
        CHECK(plan.value >= -1.0);
        CHECK(plan.value <= 1.0);
        CHECK(plan.value <= r.value);
    }
    CHECK(r.plans[r.best].value == r.value);
    CHECK(r.stats.alpha_beta_cuts == 0);
    CHECK(r.stats.star1_cuts == 0);
    CHECK(r.stats.chance_nodes > 0);
}

TEST_CASE("alpha-beta and Star1 find the same value as the search without pruning, visiting no more nodes", "[endgame]") {
    for (const int depth : {1, 2}) {
        Fixture f;
        test::set_active(f.game, f.me, "TST-4");
        f.power_up(f.me);
        f.power_up(f.them);
        test::put_in_hand(f.game, f.me, "TST-6");
        f.trim(f.me, 5);
        f.trim(f.them, 5);
        const SolveResult brute = pokebot::expectiminimax(f.game, small(depth, false));
        const SolveResult pruned = pokebot::expectiminimax(f.game, small(depth, true));
        CHECK(std::abs(brute.value - pruned.value) < 1e-9);
        CHECK(std::abs(brute.plans[pruned.best].value - brute.value) < 1e-9);
        CHECK(visited(pruned) <= visited(brute));
        for (std::size_t i = 0; i < pruned.plans.size(); ++i) {
            if (pruned.plans[i].exact) {
                CHECK(std::abs(pruned.plans[i].value - brute.plans[i].value) < 1e-9);
            } else {
                CHECK(pruned.plans[i].value >= brute.plans[i].value - 1e-9);
                CHECK(brute.plans[i].value <= pruned.value + 1e-9);
            }
        }
    }
}

TEST_CASE("the same position solves to the same plans and values", "[endgame]") {
    Fixture f;
    f.power_up(f.me);
    test::put_in_hand(f.game, f.me, "TST-6");
    f.seen();
    const SolveResult a = pokebot::expectiminimax(f.game, small(2, true));
    const SolveResult b = pokebot::expectiminimax(f.game, small(2, true));
    REQUIRE(a.plans.size() == b.plans.size());
    CHECK(a.best == b.best);
    for (std::size_t i = 0; i < a.plans.size(); ++i) {
        CHECK(a.plans[i].value == b.plans[i].value);
        CHECK(a.plans[i].plan.actions == b.plans[i].plan.actions);
    }
    CHECK(visited(a) == visited(b));
}

TEST_CASE("on the real decks, pruned endgame searches agree with brute force and prune some nodes", "[endgame]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::size_t n = real->decks.size();
    long long cuts = 0;
    int searched = 0;
    for (std::uint64_t seed = 1; seed <= 4 && searched < 3; ++seed) {
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
            const SolveResult brute = pokebot::expectiminimax(before, small(2, false));
            const SolveResult pruned = pokebot::expectiminimax(before, small(2, true));
            CHECK(std::abs(brute.value - pruned.value) < 1e-9);
            CHECK(visited(pruned) <= visited(brute));
            cuts += pruned.stats.alpha_beta_cuts + pruned.stats.star1_cuts;
            ++searched;
        };
        pokebot::play_game(table, seed, first, second, match);
    }
    CHECK(searched > 0);
    CHECK(cuts > 0);
}
