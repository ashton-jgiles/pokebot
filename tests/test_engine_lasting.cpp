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
using pokebot::TrainerKind;

namespace {

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

    // TST-1 Lastmon (300 HP), all attacks [C]:
    //   0 Blow 30 (can't use Blow during your next turn)   1 Blade 30 (can't attack during your next turn)
    //   2 Clutch 10 (the Defending Pokemon can't retreat during your opponent's next turn)
    //   3 Pollen 10 (your opponent can't play Items during their next turn)
    // TST-2 Targetmon (300 HP). TST-3 Bigmon ex (300 HP). TST-4 Energy. TST-5 Belt (Supporter: +40 to an Active ex this turn).
    // TST-6 Draw Item.
    CardDB sample_db() {
        using E = EnergyType;
        const auto with = [](const char* name, int damage, const char* effects) {
            auto a = test::attack(name, {E::Colorless}, damage);
            a.effects = json::parse(effects);
            return a;
        };
        CardDef last = test::basic_pokemon("TST-1", "Lastmon", 300);
        last.attacks = {
            with("Blow", 30, R"([{"op": "lasting", "effect": "cant_use_attack", "until": "your_next_turn"}])"),
            with("Blade", 30, R"([{"op": "lasting", "effect": "cant_attack", "until": "your_next_turn"}])"),
            with("Clutch", 10, R"([{"op": "lasting", "effect": "cant_retreat", "on": "defending", "until": "opponents_next_turn"}])"),
            with("Pollen", 10, R"([{"op": "lasting", "effect": "no_items", "on": "player", "player": "opponent", "until": "opponents_next_turn"}])"),
        };
        CardDef big = test::basic_pokemon("TST-3", "Bigmon ex", 300);
        big.rules.ex = true;

        CardDB db;
        db.add(last);
        db.add(test::basic_pokemon("TST-2", "Targetmon", 300));
        db.add(big);
        db.add(test::basic_energy("TST-4", "Darkness Energy", E::Darkness));
        db.add(trainer("TST-5", "Belt", TrainerKind::Supporter,
                       R"([{"op": "lasting", "effect": "damage_bonus", "on": "player", "amount": 40, "only_ex": true, "until": "this_turn"}])"));
        db.add(trainer("TST-6", "Draw Item", TrainerKind::Item, R"([{"op": "draw"}])"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {40, 4}, {4, 5}, {4, 6}});

    Action attack(std::uint8_t index) {
        return Action{ActionKind::Attack, pokebot::kNoCard, kActiveSlot, index};
    }

    bool can_use(const GameState& game, std::uint8_t index) {
        for (const auto& action : pokebot::legal_actions(game)) {
            if (action.kind == ActionKind::Attack && action.amount == index) {
                return true;
            }
        }
        return false;
    }

    // Turn 2: both players have a Pokemon Active with 2 Energy and Targetmon on the Bench. `me` has Lastmon.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 16);
        int me = 0;
        int them = 0;

        explicit Fixture(const char* their_active = "TST-2") {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            for (const int p : {me, them}) {
                test::set_active(game, p, p == me ? "TST-1" : their_active);
                test::clear_bench(game, p);
                test::add_to_bench(game, p, "TST-2");
                test::attach_from_deck(game, p, kActiveSlot, "TST-4");
                test::attach_from_deck(game, p, kActiveSlot, "TST-4");
            }
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        void check_cards() const {
            CHECK(test::cards_accounted_for(game.player(0)) == 60);
            CHECK(test::cards_accounted_for(game.player(1)) == 60);
            CHECK(pokebot::check_invariants(game).empty());
        }
    };

} // namespace

TEST_CASE("can't use that attack during your next turn, but can use the others", "[lasting]") {
    Fixture f;
    pokebot::apply(f.game, attack(0));
    test::end_turns(f.game, 1); // their turn passes: my next turn
    REQUIRE(f.game.current_player == f.me);
    CHECK_FALSE(can_use(f.game, 0));
    CHECK(can_use(f.game, 1));
    f.check_cards();

    test::end_turns(f.game, 2); // the turn after that: free again
    CHECK(can_use(f.game, 0));
    CHECK(f.game.lasting.empty());
}

TEST_CASE("can't attack during your next turn", "[lasting]") {
    Fixture f;
    pokebot::apply(f.game, attack(1));
    test::end_turns(f.game, 1);
    CHECK_FALSE(test::offers(pokebot::legal_actions(f.game), ActionKind::Attack));
}

TEST_CASE("moving to the Bench ends an effect on that Pokemon", "[lasting]") {
    Fixture f;
    pokebot::apply(f.game, attack(1));
    test::end_turns(f.game, 1);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::Retreat));
    CHECK(f.game.lasting.empty());
    f.check_cards();
}

TEST_CASE("the Defending Pokemon can't retreat during the opponent's next turn only", "[lasting]") {
    Fixture f;
    pokebot::apply(f.game, attack(2));
    REQUIRE(f.game.current_player == f.them);
    CHECK_FALSE(test::offers(pokebot::legal_actions(f.game), ActionKind::Retreat));
    test::end_turns(f.game, 2); // back to them on their following turn
    CHECK(test::offers(pokebot::legal_actions(f.game), ActionKind::Retreat));
}

TEST_CASE("the opponent can't play Items during their next turn", "[lasting]") {
    Fixture f;
    test::put_in_hand(f.game, f.them, "TST-6");
    test::put_in_hand(f.game, f.me, "TST-6");
    const auto offers_item = [&f]() {
        for (const auto& action : pokebot::legal_actions(f.game)) {
            if (action.kind == ActionKind::PlayTrainer && f.game.def(action.card).id == "TST-6") {
                return true;
            }
        }
        return false;
    };
    pokebot::apply(f.game, attack(3));
    CHECK_FALSE(offers_item());
    test::end_turns(f.game, 1); // my turn: my Items are fine
    CHECK(offers_item());
    f.check_cards();
}

TEST_CASE("a damage bonus this turn applies only to an Active Pokemon ex", "[lasting]") {
    {
        Fixture f("TST-3");
        test::put_in_hand(f.game, f.me, "TST-5");
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-5"));
        REQUIRE(f.game.lasting.size() == 1);
        pokebot::apply(f.game, attack(1));
        CHECK(f.game.player(f.them).active->damage_counters == 7); // 30 + 40
        f.check_cards();
    }
    {
        Fixture f;
        test::put_in_hand(f.game, f.me, "TST-5");
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-5"));
        pokebot::apply(f.game, attack(1));
        CHECK(f.game.player(f.them).active->damage_counters == 3); // not ex: no bonus
    }
}

TEST_CASE("a lasting effect for this turn is gone next turn", "[lasting]") {
    Fixture f;
    test::put_in_hand(f.game, f.me, "TST-5");
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-5"));
    test::end_turns(f.game, 1);
    CHECK(f.game.lasting.empty());
    f.check_cards();
}

TEST_CASE("history tells a Knock Out by an attack from one at Pokemon Checkup", "[lasting]") {
    {
        Fixture f;
        f.game.player(f.them).active->damage_counters = 29;
        pokebot::apply(f.game, attack(0));
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::Promote));
        const auto& last = f.game.history.opponents_last_turn();
        CHECK(last.knocked_out_by_attack[static_cast<std::size_t>(f.them)] == 1);
    }
    {
        Fixture f;
        f.game.player(f.them).active->damage_counters = 29;
        pokebot::detail::apply_special_condition(f.game, *f.game.player(f.them).active, pokebot::SpecialCondition::Poisoned, 1);
        test::end_turns(f.game, 1);
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::Promote));
        const auto& last = f.game.history.opponents_last_turn();
        CHECK(last.knocked_out[static_cast<std::size_t>(f.them)] == 1);
        CHECK(last.knocked_out_by_attack[static_cast<std::size_t>(f.them)] == 0);
    }
}

TEST_CASE("lasting effects are checked when the cards load", "[lasting]") {
    using pokebot::CardDataError;
    using pokebot::compile_attack;
    using pokebot::compile_trainer;
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "lasting", "effect": "cant_sneeze", "until": "this_turn"}])")), CardDataError);
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "lasting", "effect": "cant_attack", "until": "forever"}])")), CardDataError);
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "lasting", "effect": "no_items", "until": "this_turn"}])")), CardDataError);
    CHECK_THROWS_AS(compile_trainer(json::parse(R"([{"op": "lasting", "effect": "cant_attack", "until": "this_turn"}])")), CardDataError);
    CHECK_NOTHROW(compile_trainer(json::parse(R"([{"op": "lasting", "effect": "no_items", "on": "player", "until": "this_turn"}])")));
}
