#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "agents/planner/action_class.hpp"
#include "agents/planner/or_tree.hpp"
#include "agents/planner/plan.hpp"
#include "agents/random/random_agent.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "harness/match.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using pokebot::Action;
using pokebot::ActionClass;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::GameState;
using pokebot::Goal;
using pokebot::GoalKind;
using pokebot::kActiveSlot;
using pokebot::or_tree_search;

namespace {

    pokebot::Attack attack(const char* name, int damage) {
        pokebot::Attack a;
        a.name = name;
        a.cost = {pokebot::EnergyType::Colorless};
        a.damage = damage;
        return a;
    }

    CardDef with_attack(CardDef c, const char* name, int damage) {
        c.attacks.push_back(attack(name, damage));
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

        void damage_theirs(int counters) {
            game.player(them).active->damage_counters = static_cast<std::uint16_t>(counters);
        }

        pokebot::OrTreeResult search(GoalKind kind, pokebot::OrTreeOptions options = {}) const {
            return or_tree_search(game, me, Goal{kind, 1}, options);
        }

        GameState play(const pokebot::Plan& plan) const {
            GameState state = game;
            for (std::size_t i = 0; i < plan.actions.size(); ++i) {
                const auto tried = pokebot::try_action(state, me, plan.actions[i]);
                CHECK(tried.kind == (i + 1 < plan.actions.size() ? ActionClass::Deterministic : ActionClass::Final));
                state = tried.after;
            }
            return state;
        }
    };

    std::vector<ActionKind> kinds(const pokebot::Plan& plan) {
        std::vector<ActionKind> out;
        for (const Action& action : plan.actions) {
            out.push_back(action.kind);
        }
        return out;
    }

} // namespace

TEST_CASE("the or-tree finds attaching Energy and attacking for a Knock Out", "[planner]") {
    Fixture f;
    f.give("TST-2");
    f.damage_theirs(3);
    const auto result = f.search(GoalKind::KnockOut);
    REQUIRE(result.found);
    CHECK(kinds(result.plan) == std::vector<ActionKind>{ActionKind::AttachEnergy, ActionKind::Attack});
    const GameState after = f.play(result.plan);
    CHECK(after.player(f.me).prizes.size() == 5);
}

TEST_CASE("the or-tree finds evolve, attach, attack (the proposal's demonstration)", "[planner]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-4");
    const auto result = f.search(GoalKind::KnockOut);
    REQUIRE(result.found);
    CHECK(kinds(result.plan) == std::vector<ActionKind>{ActionKind::Evolve, ActionKind::AttachEnergy, ActionKind::Attack});
    f.play(result.plan);
}

TEST_CASE("the or-tree finds a switch, then the attack the other Pokemon can make", "[planner]") {
    Fixture f;
    test::set_active(f.game, f.me, "TST-5");
    test::attach_from_deck(f.game, f.me, pokebot::bench_slot(0), "TST-2");
    f.seen();
    f.damage_theirs(3);
    const auto result = f.search(GoalKind::KnockOut);
    REQUIRE(result.found);
    CHECK(kinds(result.plan).front() == ActionKind::Retreat);
    CHECK(kinds(result.plan).back() == ActionKind::Attack);
    f.play(result.plan);
}

TEST_CASE("the or-tree's goals: a win by the last Prize card, and k Prize cards", "[planner]") {
    Fixture f;
    f.give("TST-2");
    f.damage_theirs(3);
    auto& prizes = f.game.player(f.me).prizes;
    while (prizes.size() > 1) {
        f.game.player(f.me).discard.push_back(prizes.pop_back());
    }
    f.seen();
    CHECK(f.search(GoalKind::Win).found);
    CHECK(f.search(GoalKind::Prizes).found);
    CHECK(f.play(f.search(GoalKind::Win).plan).is_over());
    CHECK_FALSE(or_tree_search(f.game, f.me, Goal{GoalKind::Prizes, 2}).found);
}

TEST_CASE("the or-tree reports no plan when none reaches the goal, and the closed set saves nodes", "[planner]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-3");
    const auto with = f.search(GoalKind::KnockOut);
    CHECK_FALSE(with.found);
    CHECK_FALSE(with.out_of_budget);
    CHECK(with.closed > 0);

    pokebot::OrTreeOptions open_all;
    open_all.closed_set = false;
    const auto without = f.search(GoalKind::KnockOut, open_all);
    CHECK_FALSE(without.found);
    CHECK(without.expanded > with.expanded);

    pokebot::OrTreeOptions tiny;
    tiny.budget = 1;
    const auto stopped = f.search(GoalKind::KnockOut, tiny);
    CHECK(stopped.out_of_budget);
    CHECK(stopped.expanded == 1);
}

TEST_CASE("on the real decks, every plan the or-tree finds reaches its goal when played", "[planner]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::size_t n = real->decks.size();
    pokebot::OrTreeOptions options;
    options.budget = 100;
    int searched = 0;
    int found = 0;
    for (std::uint64_t seed = 0; seed < 8; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table = CardTable(real->db, real->decks[i % n], real->decks[(i / n) % n]).shuffled(~seed);
        pokebot::BiasedRandomAgent agent(seed);
        GameState game = pokebot::new_game(table, seed);
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            const bool main_phase = game.decision().kind == pokebot::DecisionKind::MainPhase;
            if (game.turn > 0 && main_phase && step % 4 == 0) {
                const int player = game.decision().player;
                const int prizes_before = static_cast<int>(game.player(player).prizes.size());
                const auto result = or_tree_search(game, player, Goal{GoalKind::KnockOut, 1}, options);
                ++searched;
                if (result.found) {
                    ++found;
                    INFO("seed " << seed << ", step " << step);
                    GameState state = game;
                    for (const Action& action : result.plan.actions) {
                        pokebot::apply(state, action);
                    }
                    CHECK(static_cast<int>(state.player(player).prizes.size()) < prizes_before);
                }
            }
            pokebot::apply_legal(game, pokebot::ask(agent, game, actions));
        }
    }
    INFO(found << " of " << searched << " searches found a Knock Out");
    CHECK(searched > 50);
    CHECK(found > 0);
}
