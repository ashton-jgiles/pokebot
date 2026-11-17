#include <catch2/catch_test_macros.hpp>

#include "engine/engine.hpp"
#include "engine/invariants/invariants.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using nlohmann::json;
using pokebot::Action;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::DecisionKind;
using pokebot::EnergyType;
using pokebot::GameState;
using pokebot::kActiveSlot;

namespace {

    pokebot::Ability ability(const char* name, const char* fields) {
        const json j = json::parse(fields);
        pokebot::Ability a;
        a.name = name;
        a.text = "test ability";
        a.effects = j.value("effects", json::array());
        a.requirements = j.value("requires", json::array());
        a.once_per_turn = j.value("once_per_turn", true);
        a.one_per_name = j.value("one_per_name", false);
        a.active_only = j.value("active_only", false);
        return a;
    }

    CardDef with_abilities(const char* id, const char* name, EnergyType type, std::vector<pokebot::Ability> abilities) {
        CardDef c = test::basic_pokemon(id, name, 100);
        c.types = {type};
        c.abilities = std::move(abilities);
        return c;
    }

    // TST-1 Drawmon: "Draw Two" (draw 2), "Errand" (Active only: draw 1), "Script" (one per name: draw 1).
    // TST-2 Scriptmon: "Script" too. TST-3 Trademon: "Trade" (discard 1, draw 2).
    // TST-4 Shurikenmon (Water): "Shuriken" (Active only, discard a Water Energy from hand: 3 damage counters on 1 of theirs).
    // TST-5 Reconmon: "Recon" (look at the top 2, 1 to hand, 1 to the bottom). TST-6 Runmon: "Run" (draw 1, then shuffle itself into the deck).
    // TST-7 Brainmon (Darkness): "Brain" (needs Darkness Energy and a damaged Pokemon: move up to 3 counters to 1 of theirs).
    // TST-8 Chainmon (Darkness): "Chains" (switch in a Benched Darkness Pokemon other than Chainmon; it's Poisoned).
    // TST-9 Darkmon (Darkness). TST-10 Watermon (Water). TST-11 Darkness Energy. TST-12 Water Energy.
    // TST-13 Beach (Stadium: switch an Active Water Pokemon with a Benched one).
    CardDB sample_db() {
        using E = EnergyType;
        CardDB db;
        db.add(with_abilities("TST-1", "Drawmon", E::Colorless,
                              {ability("Draw Two", R"({"effects": [{"op": "draw", "count": 2}]})"),
                               ability("Errand", R"({"active_only": true, "effects": [{"op": "draw"}]})"),
                               ability("Script", R"({"one_per_name": true, "effects": [{"op": "draw"}]})")}));
        db.add(with_abilities("TST-2", "Scriptmon", E::Colorless, {ability("Script", R"({"one_per_name": true, "effects": [{"op": "draw"}]})")}));
        db.add(with_abilities("TST-3", "Trademon", E::Colorless,
                              {ability("Trade", R"({"requires": [{"hand_besides": 1}], "effects": [{"op": "discard_from_hand"}, {"op": "draw", "count": 2}]})")}));
        db.add(with_abilities("TST-4", "Shurikenmon", E::Water,
                              {ability("Shuriken", R"({"active_only": true, "requires": [{"hand_has": {"kind": "energy", "type": "water"}}],
                                                      "effects": [{"op": "discard_from_hand", "filter": {"kind": "energy", "type": "water"}},
                                                                  {"op": "counters_to", "count": 3}]})")}));
        db.add(with_abilities("TST-5", "Reconmon", E::Colorless, {ability("Recon", R"({"effects": [{"op": "look_at_top", "count": 2, "take": 1}]})")}));
        db.add(with_abilities("TST-6", "Runmon", E::Colorless,
                              {ability("Run", R"({"effects": [{"op": "draw"}, {"op": "if", "condition": {"drew_at_least": 1},
                                                                                "then": [{"op": "shuffle_self_into_deck"}]}]})")}));
        db.add(with_abilities("TST-7", "Brainmon", E::Darkness,
                              {ability("Brain", R"({"requires": [{"self_has_energy": {"type": "darkness"}}, {"has_damaged": true}],
                                                   "effects": [{"op": "move_counters", "count": 3}]})")}));
        db.add(with_abilities("TST-8", "Chainmon", E::Darkness,
                              {ability("Chains", R"({"requires": [{"bench_has": {"type": "darkness", "not_name": "Chainmon"}}],
                                                    "effects": [{"op": "switch_active", "filter": {"type": "darkness", "not_name": "Chainmon"}},
                                                                {"op": "special_condition", "condition": "poisoned", "player": "you"}]})")}));
        CardDef dark = test::basic_pokemon("TST-9", "Darkmon", 100);
        dark.types = {E::Darkness};
        db.add(dark);
        CardDef water = test::basic_pokemon("TST-10", "Watermon", 100);
        water.types = {E::Water};
        db.add(water);
        db.add(test::basic_energy("TST-11", "Darkness Energy", E::Darkness));
        db.add(test::basic_energy("TST-12", "Water Energy", E::Water));
        CardDef beach;
        beach.id = "TST-13";
        beach.name = "Beach";
        beach.kind = pokebot::CardKind::Trainer;
        beach.trainer_kind = pokebot::TrainerKind::Stadium;
        beach.text = "test card";
        beach.requirements = json::parse(R"([{"active_is": {"type": "water"}}, {"bench_has": {"type": "water"}}])");
        beach.effects = json::parse(R"([{"op": "switch_active", "filter": {"type": "water"}}])");
        db.add(beach);
        return db;
    }

    const pokebot::Decklist kDeck = test::deck(
        {{4, 1}, {4, 2}, {4, 3}, {4, 4}, {4, 5}, {4, 6}, {4, 7}, {4, 8}, {4, 9}, {4, 10}, {8, 11}, {8, 12}, {4, 13}});

    // Turn 2: `me` has `active` Active and an empty Bench; `them` has Darkmon Active and Watermon on the Bench.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 20);
        int me = 0;
        int them = 0;

        explicit Fixture(const char* active) {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            test::set_active(game, me, active);
            test::clear_bench(game, me);
            test::set_active(game, them, "TST-9");
            test::clear_bench(game, them);
            test::add_to_bench(game, them, "TST-10");
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        pokebot::PlayerState& mine() { return game.player(me); }

        std::vector<std::string> usable() {
            std::vector<std::string> names;
            for (const auto& action : pokebot::legal_actions(game)) {
                if (action.kind == ActionKind::UseAbility) {
                    names.push_back(game.def(action.card).abilities[action.amount].name);
                }
            }
            return names;
        }

        bool can(const char* name) {
            for (const auto& n : usable()) {
                if (n == name) {
                    return true;
                }
            }
            return false;
        }

        void use(const char* name) {
            for (const auto& action : pokebot::legal_actions(game)) {
                if (action.kind == ActionKind::UseAbility && game.def(action.card).abilities[action.amount].name == name) {
                    pokebot::apply(game, action);
                    return;
                }
            }
            FAIL("no Ability called " << name);
        }

        void check_cards() const {
            CHECK(test::cards_accounted_for(game.player(0)) == 60);
            CHECK(test::cards_accounted_for(game.player(1)) == 60);
            CHECK(pokebot::check_invariants(game).empty());
        }
    };

} // namespace

TEST_CASE("an Ability can be used once during your turn, and again next turn", "[abilities]") {
    Fixture f("TST-1");
    const auto hand = f.mine().hand.size();
    REQUIRE(f.can("Draw Two"));
    f.use("Draw Two");
    CHECK(f.mine().hand.size() == hand + 2);
    CHECK(f.game.decision().kind == DecisionKind::MainPhase);
    CHECK_FALSE(f.can("Draw Two"));
    CHECK(f.can("Errand")); // a different Ability on the same Pokemon
    f.check_cards();

    test::end_turns(f.game, 2);
    CHECK(f.can("Draw Two"));
}

TEST_CASE("an Active-only Ability isn't offered on the Bench", "[abilities]") {
    Fixture f("TST-9");
    test::add_to_bench(f.game, f.me, "TST-1");
    CHECK(f.can("Draw Two"));
    CHECK_FALSE(f.can("Errand"));
}

TEST_CASE("only one Ability of the same name per turn", "[abilities]") {
    Fixture f("TST-1");
    test::add_to_bench(f.game, f.me, "TST-2");
    CHECK(f.usable().size() == 4); // Draw Two, Errand, Script, and Scriptmon's Script
    f.use("Script");
    CHECK_FALSE(f.can("Script"));
}

TEST_CASE("an Ability's requirements and costs", "[abilities]") {
    Fixture f("TST-3");
    while (!f.mine().hand.empty()) {
        f.mine().deck.push_back(f.mine().hand.pop_back());
    }
    CHECK_FALSE(f.can("Trade")); // nothing to discard
    test::put_in_hand(f.game, f.me, "TST-11");
    REQUIRE(f.can("Trade"));
    f.use("Trade");
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard));
    CHECK(f.mine().hand.size() == 2); // discarded the Energy, drew 2
    f.check_cards();
}

TEST_CASE("damage counters from an Ability Knock Out right away", "[abilities]") {
    Fixture f("TST-4");
    test::put_in_hand(f.game, f.me, "TST-12");
    f.game.player(f.them).active->damage_counters = 8;
    f.use("Shuriken");
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards); // the cost: which Water Energy to discard
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-12"));
    REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
    pokebot::apply(f.game, Action{ActionKind::ChoosePokemon, pokebot::kNoCard, kActiveSlot, 0});
    CHECK(f.mine().prizes.size() == 5);
    REQUIRE(f.game.decision().kind == DecisionKind::PromoteActive);
    CHECK(f.game.decision().player == f.them);
    f.check_cards();
}

TEST_CASE("looking at the top 2: one to the hand, the other to the bottom", "[abilities]") {
    Fixture f("TST-5");
    auto& deck = f.mine().deck;
    const auto top = deck[deck.size() - 1];
    const auto second = deck[deck.size() - 2];
    f.use("Recon");
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    for (const auto& action : pokebot::legal_actions(f.game)) {
        CHECK((action.card == top || action.card == second)); // only the top 2 are offered
    }
    const auto taken = test::find_action(f.game, ActionKind::ChooseCard).card;
    const auto other = taken == top ? second : top;
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard));
    CHECK(f.mine().hand.contains(taken));
    CHECK(deck[0] == other); // the other one is now on the bottom
    f.check_cards();
}

TEST_CASE("an Ability that shuffles its Pokemon into the deck asks for a new Active", "[abilities]") {
    Fixture f("TST-6");
    test::add_to_bench(f.game, f.me, "TST-9");
    f.use("Run");
    CHECK_FALSE(f.mine().active.has_value());
    REQUIRE(f.game.decision().kind == DecisionKind::PromoteActive);
    CHECK(f.game.decision().player == f.me);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::Promote));
    CHECK(f.game.decision().kind == DecisionKind::MainPhase);
    f.check_cards();
}

TEST_CASE("moving damage counters from your Pokemon to the opponent's", "[abilities]") {
    Fixture f("TST-7");
    CHECK_FALSE(f.can("Brain")); // no Darkness Energy, nothing damaged
    test::attach_from_deck(f.game, f.me, kActiveSlot, "TST-11");
    CHECK_FALSE(f.can("Brain"));
    test::add_to_bench(f.game, f.me, "TST-9");
    f.mine().bench[0].damage_counters = 5;
    REQUIRE(f.can("Brain"));

    f.use("Brain");
    pokebot::apply(f.game, Action{ActionKind::ChoosePokemon, pokebot::kNoCard, pokebot::bench_slot(0), 0}); // from
    pokebot::apply(f.game, Action{ActionKind::ChoosePokemon, pokebot::kNoCard, kActiveSlot, 0});            // to
    CHECK(f.mine().bench[0].damage_counters == 2);
    CHECK(f.game.player(f.them).active->damage_counters == 3);
    f.check_cards();
}

TEST_CASE("switching in only a matching Benched Pokemon, which is then Poisoned", "[abilities]") {
    Fixture f("TST-8");
    test::add_to_bench(f.game, f.me, "TST-10"); // Water: can't be chosen
    CHECK_FALSE(f.can("Chains"));
    test::add_to_bench(f.game, f.me, "TST-9"); // Darkness
    REQUIRE(f.can("Chains"));
    f.use("Chains");
    REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
    const auto choices = pokebot::legal_actions(f.game);
    REQUIRE(choices.size() == 1);
    CHECK(choices[0].target == pokebot::bench_slot(1));
    pokebot::apply(f.game, choices[0]);
    CHECK(f.game.def(f.mine().active->top()).id == "TST-9");
    CHECK(f.mine().active->special.poison == 1);
    f.check_cards();
}

TEST_CASE("a Stadium's action, once per turn for each player", "[abilities]") {
    Fixture f("TST-10");
    f.mine().stadium = test::take_from_deck(f.game, f.me, "TST-13");
    CHECK_FALSE(test::offers(pokebot::legal_actions(f.game), ActionKind::UseStadium)); // no Benched Water Pokemon
    test::add_to_bench(f.game, f.me, "TST-10");
    REQUIRE(test::offers(pokebot::legal_actions(f.game), ActionKind::UseStadium));
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::UseStadium));
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChoosePokemon));
    CHECK_FALSE(test::offers(pokebot::legal_actions(f.game), ActionKind::UseStadium));
    f.check_cards();
}
