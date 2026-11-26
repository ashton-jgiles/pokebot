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

    CardDB sample_db() {
        using E = EnergyType;
        const auto with = [](const char* name, int damage, const char* effects) {
            auto a = test::attack(name, {E::Colorless}, damage);
            a.effects = json::parse(effects);
            return a;
        };
        CardDef medic = test::basic_pokemon("TST-1", "Medicmon", 200);
        medic.types = {E::Fire};
        medic.attacks = {
            with("Recover", 30, R"([{"op": "heal_self", "amount": 30}])"),
            with("Patch", 0, R"([{"op": "heal_to", "amount": 50}])"),
            with("Aid", 0, R"([{"op": "heal_to", "each": true, "amount": 20}])"),
            with("Reckless", 60, R"([{"op": "damage_self", "amount": 30}])"),
            with("Spray", 0, R"([{"op": "damage_to", "each": true, "on": "bench", "amount": 20}])"),
            with("Pierce", 40, R"([{"op": "ignore_weakness"}, {"op": "ignore_resistance"}, {"op": "damage"}])"),
            with("Hunt", 20, R"([{"op": "if", "condition": {"opponent_active_is": {"rule_box": true}}, "then": [{"op": "add_damage", "amount": 50}]}, {"op": "damage"}])"),
            with("Finish", 10, R"([{"op": "if", "condition": {"opponent_active_damaged": true}, "then": [{"op": "add_damage", "amount": 30}]}, {"op": "damage"}])"),
            with("Tally", 0, R"([{"op": "set_damage", "per": 10, "for_each": {"of": "in_play"}}, {"op": "damage"}])"),
            with("Sum", 0, R"([{"op": "set_damage", "per": 10, "for_each": {"of": "active_damage_counters", "player": "opponent"}}, {"op": "damage"}])"),
            with("Strip", 0, R"([{"op": "discard_target_energy", "on": "active"}])"),
        };
        CardDef resist = test::basic_pokemon("TST-2", "Resistmon", 200);
        resist.types = {E::Grass};
        resist.weakness = pokebot::Weakness{E::Fire, 2};
        resist.resistance = pokebot::Resistance{E::Fire, 30};
        CardDef rule = test::basic_pokemon("TST-3", "Rulemon ex", 200);
        rule.rules.ex = true;
        CardDef potion;
        potion.id = "TST-5";
        potion.name = "Potion";
        potion.kind = pokebot::CardKind::Trainer;
        potion.trainer_kind = pokebot::TrainerKind::Item;
        potion.text = "test card";
        potion.requirements = json::parse(R"([{"has_damaged": true}])");
        potion.effects = json::parse(R"([{"op": "heal_to", "amount": 30}])");

        CardDB db;
        for (const auto& card : {medic, resist, rule, potion}) {
            db.add(card);
        }
        db.add(test::basic_energy("TST-4", "Fire Energy", E::Fire));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {44, 4}, {4, 5}});

    Action attack(std::uint8_t index) {
        return Action{ActionKind::Attack, pokebot::kNoCard, kActiveSlot, index};
    }

    // Turn 2. `me`: Medicmon Active with 1 Energy, Medicmon on the Bench.
    // `them`: Resistmon Active (Weakness and Resistance Fire); Bench 0 is Rulemon ex, Bench 1 is Resistmon.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 14);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            test::set_active(game, me, "TST-1");
            test::clear_bench(game, me);
            test::add_to_bench(game, me, "TST-1");
            test::attach_from_deck(game, me, kActiveSlot, "TST-4");
            test::set_active(game, them, "TST-2");
            test::clear_bench(game, them);
            test::add_to_bench(game, them, "TST-3");
            test::add_to_bench(game, them, "TST-2");
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        pokebot::PlayerState& mine() { return game.player(me); }
        pokebot::PlayerState& theirs() { return game.player(them); }

        void check_cards() const {
            CHECK(test::cards_accounted_for(game.player(0)) == 60);
            CHECK(test::cards_accounted_for(game.player(1)) == 60);
            CHECK(pokebot::check_invariants(game).empty());
        }
    };

} // namespace

TEST_CASE("healing this Pokemon, a chosen damaged one, or each of yours", "[damage_effects]") {
    {
        Fixture f;
        f.mine().active->damage_counters = 5;
        pokebot::apply(f.game, attack(0));
        CHECK(f.mine().active->damage_counters == 2);
        f.check_cards();
    }
    {
        Fixture f;
        f.mine().bench[0].damage_counters = 3;
        pokebot::apply(f.game, attack(1));
        REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
        const auto actions = pokebot::legal_actions(f.game);
        REQUIRE(actions.size() == 1);
        CHECK(actions[0].target == pokebot::bench_slot(0));
        pokebot::apply(f.game, actions[0]);
        CHECK(f.mine().bench[0].damage_counters == 0);
        f.check_cards();
    }
    {
        Fixture f;
        f.mine().active->damage_counters = 4;
        f.mine().bench[0].damage_counters = 1;
        pokebot::apply(f.game, attack(2));
        CHECK(f.game.decision().kind != DecisionKind::ChoosePokemon);
        CHECK(f.mine().active->damage_counters == 2);
        CHECK(f.mine().bench[0].damage_counters == 0);
        f.check_cards();
    }
}

TEST_CASE("an Item that heals can be played only with a damaged Pokemon", "[damage_effects]") {
    Fixture f;
    test::put_in_hand(f.game, f.me, "TST-5");
    CHECK_FALSE(test::offers(pokebot::legal_actions(f.game), ActionKind::PlayTrainer));
    f.mine().bench[0].damage_counters = 2;
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-5"));
    REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
    pokebot::apply(f.game, pokebot::legal_actions(f.game)[0]);
    CHECK(f.mine().bench[0].damage_counters == 0);
    f.check_cards();
}

TEST_CASE("damage to itself goes on the attacker, without Weakness or Resistance", "[damage_effects]") {
    Fixture f;
    pokebot::apply(f.game, attack(3));
    CHECK(f.mine().active->damage_counters == 3);
    CHECK(f.theirs().active->damage_counters == 9);
    f.check_cards();
}

TEST_CASE("each target: every Benched Pokemon at once, with no choice", "[damage_effects]") {
    Fixture f;
    pokebot::apply(f.game, attack(4));
    CHECK(f.game.decision().kind != DecisionKind::ChoosePokemon);
    CHECK(f.theirs().bench[0].damage_counters == 2);
    CHECK(f.theirs().bench[1].damage_counters == 2);
    CHECK(f.theirs().active->damage_counters == 0);
    f.check_cards();
}

TEST_CASE("an attack can ignore Weakness and Resistance", "[damage_effects]") {
    Fixture f;
    pokebot::apply(f.game, attack(5));
    CHECK(f.theirs().active->damage_counters == 4);
    f.check_cards();
}

TEST_CASE("conditions on the opponent's Active Pokemon", "[damage_effects]") {
    {
        Fixture f;
        test::set_active(f.game, f.them, "TST-3");
        pokebot::apply(f.game, attack(6));
        CHECK(f.theirs().active->damage_counters == 7);
    }
    {
        Fixture f;
        pokebot::apply(f.game, attack(6));
        CHECK(f.theirs().active->damage_counters == 1);
    }
    {
        Fixture f;
        test::set_active(f.game, f.them, "TST-3");
        pokebot::apply(f.game, attack(7));
        CHECK(f.theirs().active->damage_counters == 1);
    }
    {
        Fixture f;
        test::set_active(f.game, f.them, "TST-3");
        f.theirs().active->damage_counters = 2;
        pokebot::apply(f.game, attack(7));
        CHECK(f.theirs().active->damage_counters == 6);
        f.check_cards();
    }
}

TEST_CASE("counting your Pokemon in play and the counters on the opponent's Active", "[damage_effects]") {
    {
        Fixture f;
        test::set_active(f.game, f.them, "TST-3");
        pokebot::apply(f.game, attack(8));
        CHECK(f.theirs().active->damage_counters == 2);
    }
    {
        Fixture f;
        test::set_active(f.game, f.them, "TST-3");
        f.theirs().active->damage_counters = 3;
        pokebot::apply(f.game, attack(9));
        CHECK(f.theirs().active->damage_counters == 6);
        f.check_cards();
    }
}

TEST_CASE("discarding an Energy from the opponent's Active needs no choice of Pokemon", "[damage_effects]") {
    Fixture f;
    test::attach_from_deck(f.game, f.them, kActiveSlot, "TST-4");
    test::attach_from_deck(f.game, f.them, pokebot::bench_slot(0), "TST-4");
    const auto discard = f.theirs().discard.size();
    pokebot::apply(f.game, attack(10));
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-4"));
    CHECK(f.theirs().active->energy.empty());
    CHECK(f.theirs().bench[0].energy.size() == 1);
    CHECK(f.theirs().discard.size() == discard + 1);
    f.check_cards();
}
