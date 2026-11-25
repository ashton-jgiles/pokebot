#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "agents/planner/action_class.hpp"
#include "agents/planner/and_tree.hpp"
#include "agents/evaluation/evaluation.hpp"
#include "agents/planner/plan.hpp"
#include "agents/random/random_agent.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "harness/match.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using Catch::Approx;
using pokebot::Action;
using pokebot::ActionClass;
using pokebot::ActionKind;
using pokebot::and_tree_search;
using pokebot::AndTreeOptions;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::GameState;
using pokebot::PlanScorer;

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
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 3}, {4, 4}, {48, 2}});

    pokebot::Weights only(std::initializer_list<pokebot::Feature> features) {
        pokebot::Weights weights{};
        for (const pokebot::Feature feature : features) {
            weights[static_cast<std::size_t>(feature)] = 1.0;
        }
        return weights;
    }

    pokebot::Weights with_chosen() {
        pokebot::Weights weights = pokebot::default_weights();
        weights[static_cast<std::size_t>(pokebot::Feature::Evolved)] = 0.8;
        weights[static_cast<std::size_t>(pokebot::Feature::DeckOut)] = 1.1;
        return weights;
    }

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

        pokebot::AndTreeResult search(AndTreeOptions options = {}, const pokebot::Weights& weights = pokebot::default_weights()) const {
            PlanScorer scorer(me, 7, 3, weights);
            return and_tree_search(game, me, scorer, options);
        }
    };

    bool has(const pokebot::Plan& plan, ActionKind kind) {
        return std::any_of(plan.actions.begin(), plan.actions.end(), [&](const Action& action) { return action.kind == kind; });
    }

} // namespace

TEST_CASE("the and-tree's best plan evolves, attaches and attacks for the Knock Out", "[planner]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-4");
    const auto result = f.search();
    REQUIRE(!result.plans.empty());
    const pokebot::Plan& best = result.plans.front().plan;
    CHECK(best.actions.back().kind == ActionKind::Attack);
    CHECK(has(best, ActionKind::Evolve));
    CHECK(has(best, ActionKind::AttachEnergy));
    CHECK(pokebot::apply_plan(f.game, best).player(f.me).prizes.size() == 5);
    CHECK_FALSE(result.out_of_budget);
}

TEST_CASE("the and-tree's best plan is worth at least any one action that ends a plan now", "[planner]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-3");
    f.give("TST-4");
    const auto result = f.search();
    REQUIRE(!result.plans.empty());
    PlanScorer scorer(f.me, 7);
    for (const Action& action : pokebot::legal_actions(f.game)) {
        const auto tried = pokebot::try_action(f.game, f.me, action);
        if (tried.kind != ActionClass::Deterministic) {
            CHECK(scorer.value(f.game, action, tried) <= result.plans.front().value);
        }
    }
    PlanScorer again(f.me, 7);
    CHECK(again.value(f.game, result.plans.front().plan) == result.plans.front().value);
}

TEST_CASE("the and-tree keeps the top K plans, best first, one for each end state", "[planner]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-3");
    AndTreeOptions five;
    five.keep = 5;
    const auto top = f.search(five);
    REQUIRE(top.plans.size() == 5);
    for (std::size_t i = 1; i < top.plans.size(); ++i) {
        CHECK(top.plans[i - 1].value >= top.plans[i].value);
    }

    AndTreeOptions all;
    all.keep = 0;
    const auto every = f.search(all);
    CHECK(every.plans.size() > top.plans.size());
    CHECK(every.pruned == 0);
    CHECK(every.plans.front().value == top.plans.front().value);
}

TEST_CASE("the closed set and f_bound expand fewer nodes and find the same best plan", "[planner]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-3");
    f.give("TST-4");
    const auto run = [&](bool closed_set, bool bound) {
        AndTreeOptions options;
        options.closed_set = closed_set;
        options.bound = bound;
        options.budget = 20000;
        const auto result = f.search(options);
        REQUIRE_FALSE(result.out_of_budget);
        return result;
    };
    const auto both = run(true, true);
    const auto closed_only = run(true, false);
    const auto bound_only = run(false, true);
    const auto neither = run(false, false);
    INFO("expanded: both " << both.expanded << ", closed set only " << closed_only.expanded << ", f_bound only " << bound_only.expanded
                           << ", neither " << neither.expanded << "; pruned " << both.pruned);
    CHECK(closed_only.expanded < neither.expanded);
    CHECK(both.expanded <= closed_only.expanded);
    CHECK(bound_only.expanded <= neither.expanded);
    CHECK(both.plans.front().value == neither.plans.front().value);
    CHECK(closed_only.plans.front().value == neither.plans.front().value);
    CHECK(bound_only.plans.front().value == neither.plans.front().value);
}

TEST_CASE("f_bound finds the same best plan with the chosen candidates weighted", "[planner]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-3");
    f.give("TST-4");
    AndTreeOptions options;
    options.budget = 20000;
    const auto bounded = f.search(options, with_chosen());
    options.bound = false;
    const auto open = f.search(options, with_chosen());
    REQUIRE_FALSE(bounded.out_of_budget);
    REQUIRE_FALSE(open.out_of_budget);
    CHECK(bounded.expanded <= open.expanded);
    CHECK(bounded.plans.front().value == open.plans.front().value);
    CHECK(has(bounded.plans.front().plan, ActionKind::Evolve));
}

TEST_CASE("the upper bound's evolved and deck-out terms", "[planner]") {
    Fixture f;
    const auto evolved = only({pokebot::Feature::Evolved});
    CHECK(pokebot::upper_bound(f.game, f.me, evolved) == 0.0);
    f.give("TST-4");
    CHECK(pokebot::upper_bound(f.game, f.me, evolved) == Approx(std::tanh(1.0 / 6.0)));

    const auto deck_out = only({pokebot::Feature::DeckOut});
    auto& mine = f.game.player(f.me);
    auto& theirs = f.game.player(f.them);
    REQUIRE(mine.deck.size() > 8);
    REQUIRE(theirs.deck.size() > 8);
    CHECK(pokebot::upper_bound(f.game, f.me, deck_out) == 0.0);
    while (theirs.deck.size() > 4) {
        theirs.discard.push_back(theirs.deck.pop_back());
    }
    CHECK(pokebot::upper_bound(f.game, f.me, deck_out) == Approx(std::tanh(5.0 / 8.0)));
    while (mine.deck.size() > 4) {
        mine.discard.push_back(mine.deck.pop_back());
    }
    CHECK(pokebot::upper_bound(f.game, f.me, deck_out) == Approx(std::tanh(2.0 / 8.0)));
}

TEST_CASE("the upper bound is at least e, and 1 when a Knock Out could win the game", "[planner]") {
    Fixture f;
    f.give("TST-2");
    const auto weights = pokebot::default_weights();
    CHECK(pokebot::upper_bound(f.game, f.me, weights) >= pokebot::evaluate(f.game, f.me));

    f.game.player(f.them).active->damage_counters = 3;
    auto& prizes = f.game.player(f.me).prizes;
    while (prizes.size() > 1) {
        f.game.player(f.me).discard.push_back(prizes.pop_back());
    }
    f.seen();
    CHECK(pokebot::upper_bound(f.game, f.me, weights) == 1.0);
}

TEST_CASE("a weight on a feature without a term turns the upper bound off", "[planner]") {
    Fixture f;
    auto weights = with_chosen();
    CHECK(pokebot::upper_bound(f.game, f.me, weights) < 1.0);
    weights[static_cast<std::size_t>(pokebot::Feature::BoardHp)] = 0.1;
    CHECK(pokebot::upper_bound(f.game, f.me, weights) == 1.0);
}

TEST_CASE("on the real decks, the upper bound is at least e, and the best plan is legal and worth what it says", "[planner]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::size_t n = real->decks.size();
    const std::vector<pokebot::Weights> both{pokebot::default_weights(), with_chosen()};
    int searches = 0;
    int compared = 0;
    for (std::uint64_t seed = 0; seed < 3; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table = CardTable(real->db, real->decks[i % n], real->decks[(i / n) % n]).shuffled(~seed);
        pokebot::BiasedRandomAgent agent(seed);
        GameState game = pokebot::new_game(table, seed);
        int main_phases = 0;
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            const int player = game.decision().player;
            if (game.turn > 0) {
                for (const pokebot::Weights& weights : both) {
                    REQUIRE(pokebot::upper_bound(game, player, weights) >= pokebot::evaluate(game, player, weights));
                }
            }
            if (game.turn > 0 && game.decision().kind == pokebot::DecisionKind::MainPhase && ++main_phases % 10 == 0) {
                INFO("seed " << seed << ", step " << step);
                AndTreeOptions options;
                options.budget = 300;
                const pokebot::Weights& weights = both[static_cast<std::size_t>(main_phases / 10) % both.size()];
                PlanScorer bounded_scorer(player, seed, 3, weights);
                const auto bounded = and_tree_search(game, player, bounded_scorer, options);
                REQUIRE(!bounded.plans.empty());
                ++searches;
                const auto& best = bounded.plans.front();
                GameState state = game;
                for (const Action& action : best.plan.actions) {
                    pokebot::apply(state, action);
                }
                PlanScorer check(player, seed, 3, weights);
                CHECK(check.value(game, best.plan) == best.value);

                options.bound = false;
                PlanScorer open_scorer(player, seed, 3, weights);
                const auto open = and_tree_search(game, player, open_scorer, options);
                if (!bounded.out_of_budget && !open.out_of_budget) {
                    CHECK(best.value == open.plans.front().value);
                    ++compared;
                }
            }
            pokebot::apply_legal(game, pokebot::ask(agent, game, actions));
        }
    }
    INFO(searches << " searches, " << compared << " compared with f_bound off");
    CHECK(searches > 5);
}
