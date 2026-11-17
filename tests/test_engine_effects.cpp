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

    // TST-1 Effectmon (200 HP) has one attack per effect under test, each costing [C].
    // TST-2 Targetmon (100 HP). TST-3 Darkmon (Darkness). TST-4 Darkness Energy.
    CardDB sample_db() {
        using E = EnergyType;
        CardDef effectmon = test::basic_pokemon("TST-1", "Effectmon", 200);
        const auto with = [](const char* name, int damage, const char* effects) {
            auto a = test::attack(name, {E::Colorless}, damage);
            a.effects = json::parse(effects);
            return a;
        };
        effectmon.attacks = {
            with("Collect", 0, R"([{"op": "draw"}])"),
            with("Summon", 0, R"([{"op": "search_deck", "count": 2, "filter": {"kind": "pokemon"}}])"),
            with("Gamble", 0, R"([{"op": "may", "effects": [{"op": "search_deck"}]}])"),
            with("Loop", 30, R"([{"op": "move_attached_energy", "to": "hand"}])"),
            with("Swap", 0, R"([{"op": "switch_self"}])"),
            with("Roar", 0, R"([{"op": "discard_hand"}, {"op": "draw", "count": 6}])"),
        };
        CardDef dark = test::basic_pokemon("TST-3", "Darkmon");
        dark.types = {E::Darkness};

        CardDB db;
        db.add(effectmon);
        db.add(test::basic_pokemon("TST-2", "Targetmon", 100));
        db.add(dark);
        db.add(test::basic_energy("TST-4", "Darkness Energy", E::Darkness));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {48, 4}});

    Action attack(std::uint8_t index) {
        return Action{ActionKind::Attack, pokebot::kNoCard, kActiveSlot, index};
    }

    // Turn 2: `me` has Effectmon with one Energy and Targetmon on the Bench; `them` has Targetmon Active
    // and one on the Bench.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 4);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            test::set_active(game, me, "TST-1");
            test::attach_from_deck(game, me, kActiveSlot, "TST-4");
            test::clear_bench(game, me);
            test::add_to_bench(game, me, "TST-2");
            test::set_active(game, them, "TST-2");
            test::clear_bench(game, them);
            test::add_to_bench(game, them, "TST-2");
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        pokebot::PlayerState& mine() { return game.player(me); }

        void check_cards() const {
            CHECK(test::cards_accounted_for(game.player(0)) == 60);
            CHECK(test::cards_accounted_for(game.player(1)) == 60);
            CHECK(pokebot::check_invariants(game).empty());
        }
    };

} // namespace

TEST_CASE("Collect draws a card and the turn ends", "[effects]") {
    Fixture f;
    const auto hand = f.mine().hand.size();
    pokebot::apply(f.game, attack(0));
    CHECK(f.mine().hand.size() == hand + 1);
    CHECK(f.game.current_player == f.them);
    f.check_cards();
}

TEST_CASE("searching the deck asks for each card, then puts them in hand", "[effects]") {
    Fixture f;
    const auto hand = f.mine().hand.size();
    const auto deck = f.mine().deck.size();
    pokebot::apply(f.game, attack(1));

    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    CHECK(f.game.decision().player == f.me);
    auto actions = pokebot::legal_actions(f.game);
    CHECK(test::offers(actions, ActionKind::ChooseDone)); // "up to": choosing none is allowed
    CHECK_FALSE(test::offers(actions, ActionKind::AttachEnergy));
    CHECK(pokebot::check_invariants(f.game).empty());

    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-2"));
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-3"));

    // Two chosen: the search is over, the attack is over, and it's their turn.
    CHECK(f.mine().hand.size() == hand + 2);
    CHECK(f.mine().deck.size() == deck - 2);
    CHECK(f.game.current_player == f.them);
    f.check_cards();
}

TEST_CASE("only cards that match the filter can be chosen", "[effects]") {
    Fixture f;
    pokebot::apply(f.game, attack(1));
    for (const auto& action : pokebot::legal_actions(f.game)) {
        if (action.kind == ActionKind::ChooseCard) {
            CHECK(f.game.def(action.card).kind == pokebot::CardKind::Pokemon);
        }
    }
}

TEST_CASE("choosing nothing ends the search", "[effects]") {
    Fixture f;
    const auto hand = f.mine().hand.size();
    pokebot::apply(f.game, attack(1));
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseDone));
    CHECK(f.mine().hand.size() == hand);
    CHECK(f.game.current_player == f.them);
    f.check_cards();
}

TEST_CASE("may asks yes or no, and no skips the effect", "[effects]") {
    Fixture f;
    pokebot::apply(f.game, attack(2));
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseYesNo);

    SECTION("no") {
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::AnswerNo));
        CHECK(f.game.current_player == f.them);
    }
    SECTION("yes") {
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::AnswerYes));
        CHECK(f.game.decision().kind == DecisionKind::ChooseCards);
    }
    f.check_cards();
}

TEST_CASE("an Energy attached to the attacker can be put into the hand after the damage", "[effects]") {
    Fixture f;
    pokebot::apply(f.game, attack(3));
    CHECK(f.game.player(f.them).active->damage_counters == 3); // damage comes first
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    CHECK_FALSE(test::offers(pokebot::legal_actions(f.game), ActionKind::ChooseDone)); // "put an Energy": not optional

    const auto hand = f.mine().hand.size();
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-4"));
    CHECK(f.mine().active->energy.empty());
    CHECK(f.mine().hand.size() == hand + 1);
    f.check_cards();
}

TEST_CASE("switching the attacker with a Benched Pokemon", "[effects]") {
    Fixture f;
    const auto attacker = f.mine().active->id();
    pokebot::apply(f.game, attack(4));
    REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);

    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChoosePokemon));
    CHECK(f.game.def(f.mine().active->top()).id == "TST-2");
    CHECK(f.mine().bench[0].id() == attacker);
    CHECK(f.game.current_player == f.them);
    f.check_cards();
}

TEST_CASE("discarding the hand and drawing 6", "[effects]") {
    Fixture f;
    const auto discard = f.mine().discard.size();
    const auto hand = f.mine().hand.size();
    pokebot::apply(f.game, attack(5));
    CHECK(f.mine().hand.size() == 6);
    CHECK(f.mine().discard.size() == discard + hand);
    f.check_cards();
}

TEST_CASE("a Knock Out waits until the attack's effect is finished", "[effects]") {
    Fixture f;
    f.game.player(f.them).active->damage_counters = 7; // Loop's 30 damage Knocks Out Targetmon
    pokebot::apply(f.game, attack(3));

    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    CHECK(f.game.player(f.them).active.has_value()); // still there: the attack isn't over
    CHECK(pokebot::check_invariants(f.game).empty());

    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard));
    CHECK(f.game.decision().kind == DecisionKind::PromoteActive);
    CHECK(f.game.decision().player == f.them);
    f.check_cards();
}

TEST_CASE("a game copied in the middle of an effect carries on the same way", "[effects]") {
    Fixture f;
    pokebot::apply(f.game, attack(1));
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);

    GameState copy = f.game;
    const Action choice = test::find_action(f.game, ActionKind::ChooseCard, "TST-3");
    for (GameState* game : {&f.game, &copy}) {
        pokebot::apply(*game, choice);
        pokebot::apply(*game, test::find_action(*game, ActionKind::ChooseDone));
    }
    CHECK(copy == f.game);
}
