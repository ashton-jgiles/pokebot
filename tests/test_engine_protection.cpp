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

    CardDef with_passive(const char* id, const char* name, int hp, const char* passive) {
        CardDef c = test::basic_pokemon(id, name, hp);
        pokebot::Ability ability;
        ability.name = "Guard";
        ability.text = "test ability";
        ability.passive = json::parse(passive);
        c.abilities = {ability};
        return c;
    }

    CardDB sample_db() {
        using E = EnergyType;
        const auto with = [](const char* name, int damage, const char* effects) {
            auto a = test::attack(name, {E::Colorless}, damage);
            a.effects = json::parse(effects);
            return a;
        };
        CardDef shield = test::basic_pokemon("TST-1", "Shieldmon", 200);
        shield.attacks = {
            with("Hit", 50, "[]"),
            with("Brace", 10, R"([{"op": "lasting", "effect": "takes_less", "amount": 30, "until": "opponents_next_turn"}])"),
            with("Wall", 10, R"([{"op": "lasting", "effect": "prevent_damage", "prevent_effects": true, "until": "opponents_next_turn"}])"),
            with("Guard", 10, R"([{"op": "lasting", "effect": "prevent_damage", "amount": 40, "until": "opponents_next_turn"}])"),
            with("Weaken", 10, R"([{"op": "lasting", "effect": "deals_less", "amount": 20, "on": "defending", "until": "opponents_next_turn"}])"),
            with("Mark", 10, R"([{"op": "lasting", "effect": "takes_more", "amount": 30, "on": "defending", "until": "your_next_turn"}])"),
            with("Charge", 10, R"([{"op": "lasting", "effect": "damage_bonus", "amount": 30, "until": "your_next_turn"}])"),
            with("Daze", 10, R"([{"op": "lasting", "effect": "attack_flip", "on": "defending", "until": "opponents_next_turn"}])"),
            with("Jab", 30, "[]"),
            with("Sting", 0, R"([{"op": "special_condition", "condition": "poisoned"}, {"op": "counters_to", "on": "active", "count": 2}])"),
            with("Splash", 0, R"([{"op": "damage_to", "each": true, "on": "bench", "amount": 20}])"),
        };
        CardDef big = test::basic_pokemon("TST-3", "Bigmon ex", 200);
        big.rules.ex = true;
        big.attacks = {with("Hit", 50, "[]")};
        CardDef armor;
        armor.id = "TST-6";
        armor.name = "Armor";
        armor.kind = pokebot::CardKind::Trainer;
        armor.trainer_kind = pokebot::TrainerKind::Tool;
        armor.text = "test card";
        armor.passive = json::parse(R"([{"modify": "damage_taken", "add": -30}])");

        CardDB db;
        for (const auto& card : {shield, big, armor,
                                 with_passive("TST-2", "Benchwall", 100, R"([{"modify": "prevent_damage", "if_benched": true}, {"modify": "prevent_effects", "if_benched": true}])"),
                                 with_passive("TST-4", "Exguard", 100, R"([{"modify": "prevent_damage", "from": {"rule_box": true}}])"),
                                 with_passive("TST-7", "Auramon", 100, R"([{"modify": "damage_dealt", "add": -20, "applies_to": "opponents", "if_active": true}])")}) {
            db.add(card);
        }
        db.add(test::basic_energy("TST-5", "Psychic Energy", E::Psychic));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {4, 4}, {36, 5}, {4, 6}, {4, 7}});

    Action attack(std::uint8_t index) {
        return Action{ActionKind::Attack, pokebot::kNoCard, kActiveSlot, index};
    }

    // Turn 2. Both players have Shieldmon Active with 2 Energy; `them` has Benchwall and Shieldmon on the Bench, `me` nothing.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 16);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            for (const int p : {me, them}) {
                test::set_active(game, p, "TST-1");
                test::clear_bench(game, p);
                test::attach_from_deck(game, p, kActiveSlot, "TST-5");
                test::attach_from_deck(game, p, kActiveSlot, "TST-5");
            }
            test::add_to_bench(game, them, "TST-2");
            test::add_to_bench(game, them, "TST-1");
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        pokebot::PlayerState& mine() { return game.player(me); }
        pokebot::PlayerState& theirs() { return game.player(them); }

        void both_attack(std::uint8_t mine_first, std::uint8_t theirs_next) {
            pokebot::apply(game, attack(mine_first));
            REQUIRE(game.current_player == them);
            REQUIRE(game.decision().kind == DecisionKind::MainPhase);
            pokebot::apply(game, attack(theirs_next));
        }

        void check_cards() const {
            CHECK(test::cards_accounted_for(game.player(0)) == 60);
            CHECK(test::cards_accounted_for(game.player(1)) == 60);
            CHECK(pokebot::check_invariants(game).empty());
        }
    };

} // namespace

TEST_CASE("a game's cards say once whether any of them has a damage passive", "[protection]") {
    const CardDB db = sample_db();
    CHECK(CardTable(db, kDeck, kDeck).has_damage_passives());
    const pokebot::Decklist plain = test::deck({{4, 1}, {4, 3}, {52, 5}});
    CHECK_FALSE(CardTable(db, plain, plain).has_damage_passives());
    CHECK(CardTable(db, plain, kDeck).has_damage_passives());
    CHECK(CardTable(db, plain, kDeck).shuffled(7).has_damage_passives());
}

TEST_CASE("this Pokemon takes less damage during the opponent's next turn, then not", "[protection]") {
    Fixture f;
    f.both_attack(1, 0);
    CHECK(f.mine().active->damage_counters == 2);
    pokebot::apply(f.game, attack(8));
    pokebot::apply(f.game, attack(0));
    CHECK(f.mine().active->damage_counters == 7);
    f.check_cards();
}

TEST_CASE("preventing all damage, or damage up to an amount", "[protection]") {
    {
        Fixture f;
        f.both_attack(2, 0);
        CHECK(f.mine().active->damage_counters == 0);
    }
    {
        Fixture f;
        f.both_attack(3, 8);
        CHECK(f.mine().active->damage_counters == 0);
    }
    {
        Fixture f;
        f.both_attack(3, 0);
        CHECK(f.mine().active->damage_counters == 5);
        f.check_cards();
    }
}

TEST_CASE("preventing effects stops Special Conditions and damage counters", "[protection]") {
    Fixture f;
    f.both_attack(2, 9);
    CHECK(f.mine().active->damage_counters == 0);
    CHECK_FALSE(f.mine().active->special.any());
    f.check_cards();
}

TEST_CASE("the Defending Pokemon does less damage, or takes more", "[protection]") {
    {
        Fixture f;
        f.both_attack(4, 0);
        CHECK(f.mine().active->damage_counters == 3);
    }
    {
        Fixture f;
        f.both_attack(5, 8);
        pokebot::apply(f.game, attack(0));
        CHECK(f.theirs().active->damage_counters == 1 + 8);
        f.check_cards();
    }
}

TEST_CASE("this Pokemon's attacks do more damage during your next turn", "[protection]") {
    Fixture f;
    f.both_attack(6, 8);
    pokebot::apply(f.game, attack(0));
    CHECK(f.theirs().active->damage_counters == 1 + 8);
    f.check_cards();
}

TEST_CASE("the Defending Pokemon's attack happens only on heads", "[protection]") {
    int happened = 0;
    for (int seed = 0; seed < 12; ++seed) {
        Fixture f;
        for (int i = 0; i < seed; ++i) {
            f.game.rng.coin_flip();
        }
        f.both_attack(7, 0);
        happened += f.mine().active->damage_counters == 5 ? 1 : 0;
        CHECK((f.mine().active->damage_counters == 0 || f.mine().active->damage_counters == 5));
        f.check_cards();
    }
    CHECK(happened > 0);
    CHECK(happened < 12);
}

TEST_CASE("passives: a Tool that reduces damage, prevention from Pokemon ex, on the Bench, and less damage while Active", "[protection]") {
    {
        Fixture f;
        f.theirs().active->tool = test::take_from_deck(f.game, f.them, "TST-6");
        pokebot::apply(f.game, attack(0));
        CHECK(f.theirs().active->damage_counters == 2);
        f.check_cards();
    }
    {
        Fixture f;
        test::set_active(f.game, f.them, "TST-4");
        test::set_active(f.game, f.me, "TST-3");
        test::attach_from_deck(f.game, f.me, kActiveSlot, "TST-5");
        pokebot::apply(f.game, attack(0));
        CHECK(f.theirs().active->damage_counters == 0);
    }
    {
        Fixture f;
        test::set_active(f.game, f.them, "TST-4");
        pokebot::apply(f.game, attack(0));
        CHECK(f.theirs().active->damage_counters == 5);
    }
    {
        Fixture f;
        pokebot::apply(f.game, attack(10));
        CHECK(f.theirs().bench[0].damage_counters == 0);
        CHECK(f.theirs().bench[1].damage_counters == 2);
    }
    {
        Fixture f;
        test::set_active(f.game, f.them, "TST-7");
        pokebot::apply(f.game, attack(0));
        CHECK(f.theirs().active->damage_counters == 3);
        f.check_cards();
    }
}
