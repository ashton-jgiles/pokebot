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
namespace rules = pokebot::rules;

namespace {

    CardDef trainer(const char* id, const char* name, TrainerKind kind, const char* fields) {
        const json j = json::parse(fields);
        CardDef c;
        c.id = id;
        c.name = name;
        c.kind = pokebot::CardKind::Trainer;
        c.trainer_kind = kind;
        c.text = "test card";
        c.requirements = j.value("requires", json::array());
        c.effects = j.value("effects", json::array());
        c.passive = j.value("passive", json::array());
        return c;
    }

    // TST-1 Basicmon -> TST-2 Midmon (Stage 1) -> TST-3 Topmon (Stage 2). TST-4 Plainmon. TST-5 Teramon ex (Tera).
    // TST-6 Energy. TST-7 Candy (Rare Candy). TST-8 Tome (Transformation Tome). TST-9 Zero (Area Zero Underdepths).
    // TST-10 Other Stadium.
    CardDB sample_db() {
        CardDB db;
        db.add(test::basic_pokemon("TST-1", "Basicmon", 70));
        db.add(test::evolution("TST-2", "Midmon", pokebot::Stage::Stage1, "Basicmon", 100));
        db.add(test::evolution("TST-3", "Topmon", pokebot::Stage::Stage2, "Midmon", 150));
        db.add(test::basic_pokemon("TST-4", "Plainmon", 90));
        CardDef tera = test::basic_pokemon("TST-5", "Teramon ex", 200);
        tera.rules.ex = true;
        tera.rules.tera = true;
        db.add(tera);
        db.add(test::basic_energy("TST-6", "Darkness Energy", EnergyType::Darkness));
        db.add(trainer("TST-7", "Candy", TrainerKind::Item, R"({"requires": [{"custom": "rare_candy"}], "effects": [{"op": "custom", "name": "rare_candy"}]})"));
        db.add(trainer("TST-8", "Tome", TrainerKind::Item,
                       R"({"requires": [{"hand_has": {"name": "Tome"}}, {"custom": "transformation_tome"}],
                           "effects": [{"op": "custom", "name": "transformation_tome"}]})"));
        db.add(trainer("TST-9", "Zero", TrainerKind::Stadium, R"({"passive": [{"modify": "bench_size", "set": 8, "if_tera_in_play": true, "applies_to": "all"}]})"));
        db.add(trainer("TST-10", "Other", TrainerKind::Stadium, "{}"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {4, 4}, {4, 5}, {24, 6}, {4, 7}, {4, 8}, {4, 9}, {4, 10}});

    // Turn 4 (past everyone's first turn): `me` has Basicmon Active (played earlier) and an empty Bench.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 28);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 3);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            test::set_active(game, me, "TST-1");
            test::clear_bench(game, me);
            auto& p = game.player(me);
            for (const auto card : p.hand) { // the hand may already hold a Stage 2 or a Tome
                p.deck.push_back(card);
            }
            p.hand.clear();
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        pokebot::PlayerState& mine() { return game.player(me); }

        bool can_play(const char* id) {
            for (const auto& action : pokebot::legal_actions(game)) {
                if (action.kind == ActionKind::PlayTrainer && game.def(action.card).id == id) {
                    return true;
                }
            }
            return false;
        }

        void check_cards() const {
            CHECK(test::cards_accounted_for(game.player(0)) == 60);
            CHECK(test::cards_accounted_for(game.player(1)) == 60);
            CHECK(pokebot::check_invariants(game).empty());
        }
    };

} // namespace

TEST_CASE("Rare Candy evolves a Basic straight to Stage 2", "[custom]") {
    Fixture f;
    test::put_in_hand(f.game, f.me, "TST-7");
    CHECK_FALSE(f.can_play("TST-7")); // no Stage 2 in hand
    const auto topmon = test::put_in_hand(f.game, f.me, "TST-3");
    REQUIRE(f.can_play("TST-7"));

    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-7"));
    REQUIRE(f.game.decision().kind == DecisionKind::CustomChoice);
    const auto choices = pokebot::legal_actions(f.game);
    REQUIRE(choices.size() == 1);
    CHECK(choices[0].card == topmon);
    CHECK(choices[0].target == kActiveSlot);
    pokebot::apply(f.game, choices[0]);

    CHECK(f.mine().active->stack.size() == 2);
    CHECK(f.mine().active->top() == topmon);
    CHECK(f.game.decision().kind == DecisionKind::MainPhase);
    f.check_cards();
}

TEST_CASE("Rare Candy can't be used on the first turns or on a Pokemon played this turn", "[custom]") {
    {
        Fixture f;
        test::put_in_hand(f.game, f.me, "TST-7");
        test::put_in_hand(f.game, f.me, "TST-3");
        f.mine().active->turn_played = f.game.turn; // put into play this turn
        CHECK_FALSE(f.can_play("TST-7"));
    }
    {
        Fixture f;
        test::put_in_hand(f.game, f.me, "TST-7");
        test::put_in_hand(f.game, f.me, "TST-3");
        f.game.turn = 2;
        f.mine().active->turn_played = 0;
        CHECK_FALSE(f.can_play("TST-7"));
    }
}

TEST_CASE("Transformation Tome: two at once, swapping a Basic in play with one from the discard pile", "[custom]") {
    Fixture f;
    test::attach_from_deck(f.game, f.me, kActiveSlot, "TST-6");
    f.mine().active->damage_counters = 3;
    const auto old_basic = f.mine().active->id();
    const auto plainmon = test::take_from_deck(f.game, f.me, "TST-4");
    f.mine().discard.push_back(plainmon);

    test::put_in_hand(f.game, f.me, "TST-8");
    CHECK_FALSE(f.can_play("TST-8")); // needs a second Tome
    test::put_in_hand(f.game, f.me, "TST-8");
    REQUIRE(f.can_play("TST-8"));

    const auto discard = f.mine().discard.size();
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-8"));
    REQUIRE(f.game.decision().kind == DecisionKind::CustomChoice);
    pokebot::apply(f.game, Action{ActionKind::ChooseCard, plainmon, kActiveSlot, 0});

    CHECK(f.mine().active->id() == plainmon);
    CHECK(f.mine().active->energy.size() == 1);   // the attached cards stay
    CHECK(f.mine().active->damage_counters == 3); // and so does the damage
    CHECK(f.mine().discard.contains(old_basic));
    CHECK(f.mine().discard.size() == discard + 2); // both Tomes in, Basicmon in, Plainmon out
    f.check_cards();
}

TEST_CASE("Transformation Tome Knocks Out the new Basic if its damage is more than its HP", "[custom]") {
    Fixture f;
    test::set_active(f.game, f.me, "TST-4"); // Plainmon, 90 HP
    f.mine().active->damage_counters = 8;
    test::add_to_bench(f.game, f.me, "TST-4");
    const auto basicmon = test::take_from_deck(f.game, f.me, "TST-1"); // 70 HP
    f.mine().discard.push_back(basicmon);
    test::put_in_hand(f.game, f.me, "TST-8");
    test::put_in_hand(f.game, f.me, "TST-8");

    const auto prizes = f.game.player(f.them).prizes.size();
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-8"));
    pokebot::apply(f.game, Action{ActionKind::ChooseCard, basicmon, kActiveSlot, 0});

    CHECK_FALSE(f.mine().active.has_value());
    CHECK(f.mine().discard.contains(basicmon));
    CHECK(f.game.player(f.them).prizes.size() == prizes - 1);
    CHECK(f.game.decision().kind == DecisionKind::PromoteActive);
    f.check_cards();
}

TEST_CASE("Area Zero Underdepths: 8 Bench spaces with a Tera Pokemon in play", "[custom]") {
    Fixture f;
    f.mine().stadium = test::take_from_deck(f.game, f.me, "TST-9");
    CHECK(rules::bench_size(f.game, f.me) == 5);
    test::add_to_bench(f.game, f.me, "TST-5");
    CHECK(rules::bench_size(f.game, f.me) == 8);
    CHECK(rules::bench_size(f.game, f.them) == 5); // no Tera Pokemon of theirs
    f.check_cards();
}

TEST_CASE("when Area Zero leaves play, a player over the limit discards down to 5", "[custom]") {
    Fixture f;
    f.mine().stadium = test::take_from_deck(f.game, f.me, "TST-9");
    for (const char* id : {"TST-5", "TST-1", "TST-4", "TST-4", "TST-5", "TST-1"}) {
        test::add_to_bench(f.game, f.me, id);
    }
    REQUIRE(f.mine().bench.size() == 6);
    CHECK(pokebot::check_invariants(f.game).empty());

    test::put_in_hand(f.game, f.me, "TST-10");
    f.game.flags.stadium_played = false;
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-10"));
    REQUIRE(f.game.decision().kind == DecisionKind::DiscardBenched);
    CHECK(f.game.decision().amount == 1);
    CHECK(pokebot::check_invariants(f.game).empty()); // over the limit, but the discard is pending
    pokebot::apply(f.game, Action{ActionKind::DiscardBenched, pokebot::kNoCard, pokebot::bench_slot(5), 0});
    CHECK(f.mine().bench.size() == 5);
    CHECK(f.game.decision().kind == DecisionKind::MainPhase);
    f.check_cards();
}

TEST_CASE("custom effects are checked when the cards load", "[custom]") {
    CHECK_THROWS_AS(pokebot::compile_trainer(json::parse(R"([{"op": "custom", "name": "time_travel"}])")), pokebot::CardDataError);
    CHECK_THROWS_AS(pokebot::compile_conditions(json::parse(R"([{"custom": "time_travel"}])")), pokebot::CardDataError);
    CHECK_THROWS_AS(pokebot::compile_passive(json::parse(R"([{"modify": "bench_size", "add": 3}])")), pokebot::CardDataError);
}
