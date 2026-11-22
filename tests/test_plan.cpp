#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "agents/planner/action_class.hpp"
#include "agents/evaluation/evaluation.hpp"
#include "agents/planner/plan.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using Catch::Approx;
using nlohmann::json;
using pokebot::Action;
using pokebot::ActionClass;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::GameState;
using pokebot::kActiveSlot;
using pokebot::Plan;
using pokebot::PlanScorer;

namespace {

    CardDef attacker(const char* id, const char* name, const char* attack, const char* effects) {
        CardDef c = test::basic_pokemon(id, name);
        pokebot::Attack a;
        a.name = attack;
        a.cost = {pokebot::EnergyType::Colorless};
        a.damage = 30;
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

    // TST-1 and TST-3 Tacklers (60 HP, Tackle: 30), TST-2 Energy, TST-4 Gambler (Gamble: 30, or 60 on
    // heads), TST-6 Draw Two, TST-7 Seek (search the deck for a Pokemon).
    CardDB sample_db() {
        CardDB db;
        db.add(attacker("TST-1", "Tackler", "Tackle", "[]"));
        db.add(test::basic_energy("TST-2", "Darkness Energy", pokebot::EnergyType::Darkness));
        db.add(attacker("TST-3", "Tackler B", "Tackle", "[]"));
        db.add(attacker("TST-4", "Gambler", "Gamble",
                        R"([{"op": "flip_coins"}, {"op": "if", "condition": {"heads_at_least": 1}, "then": [{"op": "add_damage", "amount": 30}]}, {"op": "damage"}])"));
        db.add(item("TST-6", "Draw Two", R"([{"op": "draw", "count": 2}])"));
        db.add(item("TST-7", "Seek", R"([{"op": "search_deck", "filter": {"kind": "pokemon"}}])"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 3}, {4, 4}, {4, 6}, {4, 7}, {40, 2}});

    // Turn 4: each player has a Tackler Active and one on the Bench, with shuffled decks; you hold 3 cards.
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
            while (hand.size() > 3) { // a small hand, so one more or one fewer card shows in e
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

        Action find(ActionKind kind, const std::string& id = "", int target = -1) const {
            return test::find_action(game, kind, id, target);
        }
    };

    // The game after `actions` and `player`'s follow-up choices.
    GameState played(GameState game, int player, const std::vector<Action>& actions) {
        for (const Action& action : actions) {
            pokebot::apply_legal(game, action);
        }
        pokebot::finish_choices(game, player, pokebot::default_weights());
        return game;
    }

} // namespace

TEST_CASE("apply_plan plays a plan's actions in order", "[planner]") {
    Fixture f;
    f.give("TST-2");
    const Action attach = f.find(ActionKind::AttachEnergy, "TST-2", kActiveSlot);
    GameState by_hand = f.game;
    pokebot::apply(by_hand, attach);
    const Action attack = test::find_action(by_hand, ActionKind::Attack);
    pokebot::apply(by_hand, attack);
    CHECK(pokebot::apply_plan(f.game, Plan{{attach, attack}, ActionClass::Final}) == by_hand);
}

TEST_CASE("a final plan's value is e after it, however many samples", "[planner]") {
    Fixture f;
    f.give("TST-2");
    const Action attach = f.find(ActionKind::AttachEnergy, "TST-2", kActiveSlot);
    GameState powered = f.game;
    pokebot::apply(powered, attach);
    const Plan plan{{attach, test::find_action(powered, ActionKind::Attack)}, ActionClass::Final};

    const double expected = pokebot::evaluate(pokebot::apply_plan(f.game, plan), f.me);
    PlanScorer one(f.me, 1, 1);
    PlanScorer many(f.me, 2, 8);
    CHECK(one.value(f.game, plan) == expected);
    CHECK(many.value(f.game, plan) == expected);
}

TEST_CASE("an uncertain plan is scored in sampled worlds, which agree on what e can see", "[planner]") {
    Fixture f;
    f.give("TST-6");
    const Action draw = f.find(ActionKind::PlayTrainer, "TST-6");
    REQUIRE(pokebot::try_action(f.game, f.me, draw).kind == ActionClass::Uncertain);
    // Which cards are drawn differs from world to world, but e counts only how many there are.
    PlanScorer scorer(f.me, 3, 4);
    CHECK(scorer.value(f.game, Plan{{draw}, ActionClass::Uncertain}) == Approx(pokebot::evaluate(played(f.game, f.me, {draw}), f.me)));

    // Seek is scored after taking a Pokemon, not partway through the search.
    f.give("TST-7");
    const Action seek = f.find(ActionKind::PlayTrainer, "TST-7");
    const double searched = pokebot::evaluate(played(f.game, f.me, {seek}), f.me);
    GameState partway = f.game;
    pokebot::apply_legal(partway, seek);
    REQUIRE(pokebot::follows_up(partway, f.me));
    CHECK(scorer.value(f.game, Plan{{seek}, ActionClass::Uncertain}) == Approx(searched));
    CHECK(searched != Approx(pokebot::evaluate(partway, f.me)));
}

TEST_CASE("a plan that rests on a coin flip is averaged over its outcomes", "[planner]") {
    Fixture f;
    test::set_active(f.game, f.me, "TST-4");
    test::attach_from_deck(f.game, f.me, kActiveSlot, "TST-2");
    f.seen(); // their Tackler has 60 HP: heads Knocks it Out, tails leaves it 30
    const Action gamble = f.find(ActionKind::Attack);
    REQUIRE(pokebot::try_action(f.game, f.me, gamble).kind == ActionClass::Uncertain);
    const Plan plan{{gamble}, ActionClass::Uncertain};

    std::vector<double> single;
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        PlanScorer scorer(f.me, seed, 1);
        single.push_back(scorer.value(f.game, plan));
    }
    const auto [low, high] = std::minmax_element(single.begin(), single.end());
    REQUIRE(*low < *high); // both outcomes came up

    PlanScorer averaged(f.me, 99, 16);
    const double value = averaged.value(f.game, plan);
    CHECK(value > *low);
    CHECK(value < *high);
    CHECK(averaged.value(f.game, plan) == value); // the same worlds again for the same state
    PlanScorer again(f.me, 99, 16);
    CHECK(again.value(f.game, plan) == value); // and the same seed deals the same worlds
}