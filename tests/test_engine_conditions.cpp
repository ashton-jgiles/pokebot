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
using pokebot::SpecialCondition;

namespace {

    // TST-1 Attackmon (100 HP, Retreat 1): Tackle [C] 30, Sleepy Song [C] 10 (Asleep), Toxic [C] (Poisoned, 2 counters).
    // TST-2 Targetmon (100 HP). TST-3 Evomon evolves from Attackmon. TST-4 Darkness Energy.
    CardDB sample_db() {
        using E = EnergyType;
        CardDef attacker = test::basic_pokemon("TST-1", "Attackmon", 100);
        attacker.attacks = {test::attack("Tackle", {E::Colorless}, 30), test::attack("Sleepy Song", {E::Colorless}, 10),
                            test::attack("Toxic", {E::Colorless}, 0)};
        attacker.attacks[1].effects = json::parse(R"([{"op": "special_condition", "condition": "asleep"}])");
        attacker.attacks[2].effects = json::parse(R"([{"op": "special_condition", "condition": "poisoned", "count": 2}])");

        CardDB db;
        db.add(attacker);
        db.add(test::basic_pokemon("TST-2", "Targetmon", 100));
        db.add(test::evolution("TST-3", "Evomon", pokebot::Stage::Stage1, "Attackmon"));
        db.add(test::basic_energy("TST-4", "Darkness Energy", E::Darkness));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {48, 4}});

    Action attack(std::uint8_t index) {
        return Action{ActionKind::Attack, pokebot::kNoCard, kActiveSlot, index};
    }

    // Turn 2: both players have a Pokemon Active and Targetmon on the Bench; `me` has Attackmon with one Energy.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 6);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            for (const int p : {me, them}) {
                test::set_active(game, p, p == me ? "TST-1" : "TST-2");
                test::clear_bench(game, p);
                test::add_to_bench(game, p, "TST-2");
            }
            test::attach_from_deck(game, me, kActiveSlot, "TST-4");
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        pokebot::PokemonInPlay& active(int player) { return *game.player(player).active; }

        void give(int player, SpecialCondition condition, int poison = 1) {
            pokebot::detail::apply_special_condition(game, active(player), condition, poison);
        }

        void check_cards() const {
            CHECK(test::cards_accounted_for(game.player(0)) == 60);
            CHECK(test::cards_accounted_for(game.player(1)) == 60);
            CHECK(pokebot::check_invariants(game).empty());
        }
    };

} // namespace

TEST_CASE("Asleep and Paralyzed Pokemon can't attack or retreat; Confused ones can retreat", "[conditions]") {
    for (const auto condition : {SpecialCondition::Asleep, SpecialCondition::Paralyzed}) {
        Fixture f;
        f.give(f.me, condition);
        const auto actions = pokebot::legal_actions(f.game);
        CHECK_FALSE(test::offers(actions, ActionKind::Attack));
        CHECK_FALSE(test::offers(actions, ActionKind::Retreat));
    }
    Fixture f;
    f.give(f.me, SpecialCondition::Confused);
    const auto actions = pokebot::legal_actions(f.game);
    CHECK(test::offers(actions, ActionKind::Attack));
    CHECK(test::offers(actions, ActionKind::Retreat));
}

TEST_CASE("an attack's effect gives the Defending Pokemon a Special Condition", "[conditions]") {
    // The turn ends right after the attack, and the Checkup may wake it on heads, so try a few seeds.
    int asleep = 0;
    for (int seed = 0; seed < 10; ++seed) {
        Fixture f;
        for (int i = 0; i < seed; ++i) {
            f.game.rng.coin_flip();
        }
        pokebot::apply(f.game, attack(1));
        CHECK(f.active(f.them).damage_counters == 1);
        asleep += f.active(f.them).special.asleep ? 1 : 0;
        f.check_cards();
    }
    CHECK(asleep > 0);
}

TEST_CASE("Asleep, Confused and Paralyzed replace each other; Burned and Poisoned stack", "[conditions]") {
    Fixture f;
    f.give(f.them, SpecialCondition::Asleep);
    f.give(f.them, SpecialCondition::Burned);
    f.give(f.them, SpecialCondition::Poisoned);
    f.give(f.them, SpecialCondition::Confused);
    const auto& special = f.active(f.them).special;
    CHECK_FALSE(special.asleep);
    CHECK(special.confused);
    CHECK(special.burned);
    CHECK(special.poison == 1);
    f.check_cards();
}

TEST_CASE("only the Active Pokemon can have a Special Condition", "[conditions]") {
    Fixture f;
    pokebot::detail::apply_special_condition(f.game, f.game.player(f.them).bench[0], SpecialCondition::Poisoned, 1);
    CHECK_FALSE(f.game.player(f.them).bench[0].special.any());

    f.game.player(f.them).bench[0].special.burned = true;
    CHECK_FALSE(pokebot::check_invariants(f.game).empty());
}

TEST_CASE("retreating or evolving removes Special Conditions", "[conditions]") {
    {
        Fixture f;
        f.give(f.me, SpecialCondition::Confused);
        f.give(f.me, SpecialCondition::Poisoned);
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::Retreat));
        CHECK_FALSE(f.game.player(f.me).bench[0].special.any());
        f.check_cards();
    }
    {
        Fixture f;
        f.game.turn = 3; // evolving is allowed
        f.give(f.me, SpecialCondition::Burned);
        test::put_in_hand(f.game, f.me, "TST-3");
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::Evolve, "TST-3", kActiveSlot));
        CHECK_FALSE(f.active(f.me).special.any());
    }
}

TEST_CASE("Poisoned and Burned put damage counters on at Pokemon Checkup", "[conditions]") {
    Fixture f;
    f.give(f.them, SpecialCondition::Poisoned, 2);
    f.give(f.them, SpecialCondition::Burned);
    test::end_turns(f.game, 1);
    CHECK(f.active(f.them).damage_counters == 4); // 2 for Poison, 2 for Burn
    CHECK(f.active(f.them).special.poison == 2);
    f.check_cards();
}

TEST_CASE("Toxic poisons for 2 damage counters", "[conditions]") {
    Fixture f;
    pokebot::apply(f.game, attack(2));
    CHECK(f.active(f.them).special.poison == 2);
    CHECK(f.active(f.them).damage_counters == 2); // the Checkup at the end of this turn
}

TEST_CASE("Poison can Knock Out at Pokemon Checkup", "[conditions]") {
    Fixture f;
    f.active(f.them).damage_counters = 9;
    f.give(f.them, SpecialCondition::Poisoned);
    test::end_turns(f.game, 1);
    CHECK(f.game.player(f.me).prizes.size() == 5);
    REQUIRE(f.game.decision().kind == DecisionKind::PromoteActive);
    CHECK(f.game.decision().player == f.them);
    f.check_cards();
}

TEST_CASE("Asleep and Burned are each removed on heads at Pokemon Checkup", "[conditions]") {
    int woke = 0;
    int stayed = 0;
    int cooled = 0;
    for (int seed = 0; seed < 40; ++seed) {
        Fixture f;
        for (int i = 0; i < seed; ++i) {
            f.game.rng.coin_flip();
        }
        f.give(f.them, SpecialCondition::Asleep);
        f.give(f.them, SpecialCondition::Burned);
        pokebot::detail::special_conditions_checkup(f.game);
        (f.active(f.them).special.asleep ? stayed : woke) += 1;
        cooled += f.active(f.them).special.burned ? 0 : 1;
        CHECK(f.active(f.them).damage_counters == 2);
    }
    CHECK(woke > 0);
    CHECK(stayed > 0);
    CHECK(cooled > 0);
    CHECK(cooled < 40);
}

TEST_CASE("Paralyzed wears off at the Checkup after its owner's next turn", "[conditions]") {
    Fixture f;
    f.give(f.them, SpecialCondition::Paralyzed); // during my turn
    test::end_turns(f.game, 1);                  // Checkup after my turn: still Paralyzed
    CHECK(f.active(f.them).special.paralyzed);
    CHECK_FALSE(test::offers(pokebot::legal_actions(f.game), ActionKind::Attack));
    test::end_turns(f.game, 1); // Checkup after their turn: recovered
    CHECK_FALSE(f.active(f.them).special.paralyzed);
}

TEST_CASE("a Confused Pokemon's attack does nothing on tails and it takes 3 damage counters", "[conditions]") {
    int failed = 0;
    int hit = 0;
    for (int seed = 0; seed < 30; ++seed) {
        Fixture f;
        for (int i = 0; i < seed; ++i) {
            f.game.rng.coin_flip();
        }
        f.give(f.me, SpecialCondition::Confused);
        pokebot::apply(f.game, attack(0));
        const bool tails = f.active(f.me).damage_counters == 3;
        if (tails) {
            ++failed;
            CHECK(f.active(f.them).damage_counters == 0);
        } else {
            ++hit;
            CHECK(f.active(f.them).damage_counters == 3);
        }
        CHECK(f.game.current_player == f.them); // the turn ends either way
        f.check_cards();
    }
    CHECK(failed > 0);
    CHECK(hit > 0);
}
