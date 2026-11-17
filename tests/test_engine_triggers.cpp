#include <catch2/catch_test_macros.hpp>

#include "engine/engine.hpp"
#include "engine/invariants/invariants.hpp"
#include "engine/rules/rules.hpp"
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
using pokebot::TrainerKind;

namespace {

    pokebot::Ability triggered(const char* name, const char* requirements, const char* effects, bool one_per_name = false) {
        pokebot::Ability a;
        a.name = name;
        a.text = "test ability";
        a.trigger = pokebot::Trigger::PlayedToBench;
        a.one_per_name = one_per_name;
        a.requirements = json::parse(requirements);
        a.effects = json::parse(effects);
        return a;
    }

    CardDef trainer(const char* id, const char* name, TrainerKind kind, const char* effects) {
        CardDef c;
        c.id = id;
        c.name = name;
        c.kind = pokebot::CardKind::Trainer;
        c.trainer_kind = kind;
        c.text = "test card";
        c.effects = json::parse(effects);
        return c;
    }

    // TST-1 Catchmon: "Catch" (when played to the Bench: you may search for a Supporter; one per name).
    // TST-2 Sinkmon: "Sink" (when played to the Bench, with a Stadium in play: you may discard it).
    // TST-3 Verniermon: "Vernier" (when played to the Bench: you may switch it into the Active Spot, then move Energy to it).
    // TST-4 Plainmon (Colorless). TST-5 Darkmon (Darkness). TST-6 Energy. TST-7 Helper (Supporter: draw 1).
    // TST-8 Ruins (Stadium: a Basic non-Darkness Pokemon put on its owner's Bench during their turn gets 2 damage counters).
    // TST-9 Poffin (Item: put a Basic from the deck onto the Bench).
    CardDB sample_db() {
        using E = EnergyType;
        CardDB db;
        CardDef catcher = test::basic_pokemon("TST-1", "Catchmon", 100);
        catcher.abilities = {triggered("Catch", "[]", R"([{"op": "may", "effects": [{"op": "search_deck", "filter": {"trainer": "supporter"}}]}])", true)};
        CardDef sinker = test::basic_pokemon("TST-2", "Sinkmon", 100);
        sinker.abilities = {triggered("Sink", R"([{"stadium_in_play": true}])", R"([{"op": "may", "effects": [{"op": "discard_stadium"}]}])")};
        CardDef vernier = test::basic_pokemon("TST-3", "Verniermon", 100);
        vernier.abilities = {triggered("Vernier", "[]", R"([{"op": "may", "effects": [{"op": "switch_to_active"},
            {"op": "move_attached_energy", "from": "your_pokemon", "count": 60, "up_to": true, "to": "self"}]}])")};
        CardDef dark = test::basic_pokemon("TST-5", "Darkmon", 100);
        dark.types = {E::Darkness};
        for (const auto& card : {catcher, sinker, vernier, dark}) {
            db.add(card);
        }
        db.add(test::basic_pokemon("TST-4", "Plainmon", 100));
        db.add(test::basic_energy("TST-6", "Darkness Energy", E::Darkness));
        db.add(trainer("TST-7", "Helper", TrainerKind::Supporter, R"([{"op": "draw"}])"));
        CardDef ruins = trainer("TST-8", "Ruins", TrainerKind::Stadium,
                                R"([{"op": "if", "condition": {"self_is": {"stage": "basic", "not_type": "darkness"}}, "then": [{"op": "counters_on_self", "count": 2}]}])");
        ruins.trigger = pokebot::Trigger::PutOnBench;
        db.add(ruins);
        db.add(trainer("TST-9", "Poffin", TrainerKind::Item, R"([{"op": "search_deck", "to": "bench", "filter": {"name": "Plainmon"}}])"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {4, 4}, {4, 5}, {24, 6}, {4, 7}, {4, 8}, {8, 9}});

    // Turn 2: `me` has Plainmon Active with one Energy and an empty Bench.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 22);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            test::set_active(game, me, "TST-4");
            test::clear_bench(game, me);
            test::attach_from_deck(game, me, kActiveSlot, "TST-6");
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        pokebot::PlayerState& mine() { return game.player(me); }

        void bench(const char* id) {
            test::put_in_hand(game, me, id);
            pokebot::apply(game, test::find_action(game, ActionKind::PlayBasic, id));
        }

        void check_cards() const {
            CHECK(test::cards_accounted_for(game.player(0)) == 60);
            CHECK(test::cards_accounted_for(game.player(1)) == 60);
            CHECK(pokebot::check_invariants(game).empty());
        }
    };

} // namespace

TEST_CASE("playing a Pokemon to the Bench from hand triggers its Ability", "[triggers]") {
    Fixture f;
    f.bench("TST-1");
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseYesNo);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::AnswerYes));
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    const auto helper = test::find_action(f.game, ActionKind::ChooseCard, "TST-7").card;
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-7"));
    CHECK(f.mine().hand.contains(helper));
    CHECK(f.game.decision().kind == DecisionKind::MainPhase);
    f.check_cards();
}

TEST_CASE("a triggered Ability isn't offered as an Ability to use", "[triggers]") {
    Fixture f;
    f.bench("TST-1");
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::AnswerNo));
    CHECK_FALSE(test::offers(pokebot::legal_actions(f.game), ActionKind::UseAbility));
}

TEST_CASE("only one Ability of that name per turn, even when triggered", "[triggers]") {
    Fixture f;
    f.bench("TST-1");
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::AnswerNo));
    f.bench("TST-1");
    CHECK(f.game.decision().kind == DecisionKind::MainPhase); // no second "Catch"
}

TEST_CASE("an Ability with unmet requirements doesn't trigger", "[triggers]") {
    Fixture f;
    f.bench("TST-2");
    CHECK(f.game.decision().kind == DecisionKind::MainPhase); // no Stadium to discard

    Fixture g;
    const auto stadium = test::take_from_deck(g.game, g.them, "TST-8");
    g.game.player(g.them).stadium = stadium;
    g.bench("TST-2");
    REQUIRE(g.game.decision().kind == DecisionKind::ChooseYesNo);
    pokebot::apply(g.game, test::find_action(g.game, ActionKind::AnswerYes));
    CHECK(pokebot::rules::stadium_in_play(g.game) == pokebot::kNoCard);
    CHECK(g.game.player(g.them).discard.contains(stadium)); // its owner's discard pile
    g.check_cards();
}

TEST_CASE("switching the new Pokemon into the Active Spot and moving Energy to it", "[triggers]") {
    Fixture f;
    f.bench("TST-3");
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::AnswerYes));
    CHECK(f.game.def(f.mine().active->top()).id == "TST-3");
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-6")); // the only Energy: the choice ends
    CHECK(f.mine().active->energy.size() == 1);
    CHECK(f.mine().bench[0].energy.empty());
    f.check_cards();
}

TEST_CASE("a Stadium's trigger: 2 damage counters on a Basic non-Darkness Pokemon put on the Bench", "[triggers]") {
    Fixture f;
    f.mine().stadium = test::take_from_deck(f.game, f.me, "TST-8");
    f.bench("TST-4");
    CHECK(f.mine().bench[0].damage_counters == 2);
    f.bench("TST-5"); // Darkness: none
    CHECK(f.mine().bench[1].damage_counters == 0);

    test::put_in_hand(f.game, f.me, "TST-9"); // from the deck counts too
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-9"));
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-4"));
    CHECK(f.mine().bench[2].damage_counters == 2);
    CHECK(f.game.decision().kind == DecisionKind::MainPhase);
    f.check_cards();
}

TEST_CASE("Pokemon put on the Bench by Poffin don't trigger \"when you play this Pokemon\"", "[triggers]") {
    Fixture f;
    test::put_in_hand(f.game, f.me, "TST-9");
    // Only playing a Basic from hand raises PlayedToBench; a Pokemon put on the Bench from the deck doesn't.
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-9"));
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-4"));
    CHECK(f.game.decision().kind == DecisionKind::MainPhase);
}

TEST_CASE("trigger names are checked when the cards load", "[triggers]") {
    CHECK(pokebot::trigger_from_string("played_to_bench") == pokebot::Trigger::PlayedToBench);
    CHECK_THROWS_AS(pokebot::trigger_from_string("whenever"), pokebot::CardDataError);
}
