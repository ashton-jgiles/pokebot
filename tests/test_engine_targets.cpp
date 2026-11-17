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

    // TST-1 Snipermon (Fire, 200 HP), all attacks [C]:
    //   0 Arrow: 50 to 1 of the opponent's Pokemon        1 Barrage: 30 to 2 of them
    //   2 Dive 10: 3 damage counters on their Bench          3 Slash: 10 x the damage counters on 1 of their Pokemon
    //   4 Cannon 10: discard all Energy, 40 to 1 Benched     5 Tuck 10: return to hand
    //   6 Joker: use an attack of a Benched "N's" Pokemon
    // TST-2 N's Helper (100 HP): Bite [C] 20, Copycat [C] (also a copy attack, so it can't be copied).
    // TST-3 Grassmon (100 HP, Weakness Fire). TST-4 Teramon ex (Tera, 200 HP). TST-5 Energy. TST-6 Hammer (Item).
    CardDB sample_db() {
        using E = EnergyType;
        const auto with = [](const char* name, int damage, const char* effects) {
            auto a = test::attack(name, {E::Colorless}, damage);
            a.effects = json::parse(effects);
            return a;
        };
        CardDef sniper = test::basic_pokemon("TST-1", "Snipermon", 200);
        sniper.types = {E::Fire};
        sniper.attacks = {
            with("Arrow", 0, R"([{"op": "damage_to", "amount": 50}])"),
            with("Barrage", 0, R"([{"op": "damage_to", "targets": 2, "amount": 30}])"),
            with("Dive", 10, R"([{"op": "place_counters", "count": 3, "on": "bench"}])"),
            with("Slash", 0, R"([{"op": "damage_to", "per": 10, "for_each": {"of": "target_damage_counters"}}])"),
            with("Cannon", 10, R"([{"op": "discard_all_attached_energy"}, {"op": "damage_to", "on": "bench", "amount": 40}])"),
            with("Tuck", 10, R"([{"op": "return_self_to_hand"}])"),
            with("Joker", 0, R"([{"op": "use_attack", "filter": {"name_prefix": "N's"}}])"),
        };
        CardDef helper = test::basic_pokemon("TST-2", "N's Helper", 100);
        helper.attacks = {with("Bite", 20, "[]"), with("Copycat", 0, R"([{"op": "use_attack"}])")};
        CardDef grass = test::basic_pokemon("TST-3", "Grassmon", 100);
        grass.types = {E::Grass};
        grass.weakness = pokebot::Weakness{E::Fire, 2};
        CardDef tera = test::basic_pokemon("TST-4", "Teramon ex", 200);
        tera.rules.ex = true;
        tera.rules.tera = true;
        CardDef hammer;
        hammer.id = "TST-6";
        hammer.name = "Hammer";
        hammer.kind = pokebot::CardKind::Trainer;
        hammer.trainer_kind = pokebot::TrainerKind::Item;
        hammer.text = "test card";
        hammer.effects = json::parse(R"([{"op": "flip_coins"}, {"op": "if", "condition": {"heads_at_least": 1}, "then": [{"op": "discard_target_energy"}]}])");

        CardDB db;
        for (const auto& card : {sniper, helper, grass, tera, hammer}) {
            db.add(card);
        }
        db.add(test::basic_energy("TST-5", "Darkness Energy", E::Darkness));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {4, 4}, {40, 5}, {4, 6}});

    Action attack(std::uint8_t index) {
        return Action{ActionKind::Attack, pokebot::kNoCard, kActiveSlot, index};
    }

    Action choose(pokebot::Slot slot) {
        return Action{ActionKind::ChoosePokemon, pokebot::kNoCard, slot, 0};
    }

    // Turn 2. `me`: Snipermon Active with 2 Energy, N's Helper on the Bench.
    // `them`: Grassmon Active; Bench 1 is Teramon ex, Bench 2 is Grassmon.
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
            test::add_to_bench(game, me, "TST-2");
            test::attach_from_deck(game, me, kActiveSlot, "TST-5");
            test::attach_from_deck(game, me, kActiveSlot, "TST-5");
            test::set_active(game, them, "TST-3");
            test::clear_bench(game, them);
            test::add_to_bench(game, them, "TST-4");
            test::add_to_bench(game, them, "TST-3");
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

TEST_CASE("damage to a Benched Pokemon ignores Weakness; to the Active it doesn't", "[targets]") {
    {
        Fixture f;
        pokebot::apply(f.game, attack(0));
        REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
        CHECK(pokebot::legal_actions(f.game).size() == 3); // the Active and both Benched Pokemon
        pokebot::apply(f.game, choose(pokebot::bench_slot(1)));
        CHECK(f.theirs().bench[1].damage_counters == 5); // 50, no Weakness on the Bench
        f.check_cards();
    }
    {
        Fixture f;
        pokebot::apply(f.game, attack(0));
        pokebot::apply(f.game, choose(kActiveSlot));
        CHECK(f.mine().prizes.size() == 5); // 50 x 2 Knocks Out the 100 HP Active
        CHECK(f.game.decision().kind == DecisionKind::PromoteActive);
    }
}

TEST_CASE("a Benched Tera Pokemon takes no damage from attacks", "[targets]") {
    Fixture f;
    pokebot::apply(f.game, attack(0));
    pokebot::apply(f.game, choose(pokebot::bench_slot(0)));
    CHECK(f.theirs().bench[0].damage_counters == 0);
}

TEST_CASE("two targets must be different Pokemon", "[targets]") {
    Fixture f;
    pokebot::apply(f.game, attack(1));
    pokebot::apply(f.game, choose(pokebot::bench_slot(1)));
    REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
    for (const auto& action : pokebot::legal_actions(f.game)) {
        CHECK(action.target != pokebot::bench_slot(1));
    }
    pokebot::apply(f.game, choose(kActiveSlot));
    CHECK(f.theirs().bench[1].damage_counters == 3);
    CHECK(f.theirs().active->damage_counters == 6); // 30 x 2
    f.check_cards();
}

TEST_CASE("damage counters go anywhere on the Bench, Tera too, each spread offered once", "[targets]") {
    Fixture f;
    pokebot::apply(f.game, attack(2));
    REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
    pokebot::apply(f.game, choose(pokebot::bench_slot(0)));
    pokebot::apply(f.game, choose(pokebot::bench_slot(1)));
    // after Bench 2, Bench 1 isn't offered again: putting it there first is the same spread
    for (const auto& action : pokebot::legal_actions(f.game)) {
        CHECK(action.target == pokebot::bench_slot(1));
    }
    pokebot::apply(f.game, choose(pokebot::bench_slot(1)));
    CHECK(f.theirs().bench[0].damage_counters == 1); // counters aren't damage: Tera doesn't stop them
    CHECK(f.theirs().bench[1].damage_counters == 2);
    CHECK(f.theirs().active->damage_counters == 2); // the attack's own 10, doubled by Weakness
    f.check_cards();
}

TEST_CASE("damage based on the target's damage counters", "[targets]") {
    Fixture f;
    f.theirs().bench[1].damage_counters = 4;
    pokebot::apply(f.game, attack(3));
    pokebot::apply(f.game, choose(pokebot::bench_slot(1)));
    CHECK(f.theirs().bench[1].damage_counters == 8);
}

TEST_CASE("discarding all Energy, then damage to a Benched Pokemon", "[targets]") {
    Fixture f;
    const auto discard = f.mine().discard.size();
    pokebot::apply(f.game, attack(4));
    REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
    CHECK_FALSE(test::offers(pokebot::legal_actions(f.game), ActionKind::ChooseDone));
    for (const auto& action : pokebot::legal_actions(f.game)) {
        CHECK(action.target != kActiveSlot);
    }
    pokebot::apply(f.game, choose(pokebot::bench_slot(1)));
    CHECK(f.mine().active->energy.empty());
    CHECK(f.mine().discard.size() == discard + 2);
    CHECK(f.theirs().bench[1].damage_counters == 4);
    f.check_cards();
}

TEST_CASE("returning the attacker to hand asks for a new Active", "[targets]") {
    Fixture f;
    const auto hand = f.mine().hand.size();
    pokebot::apply(f.game, attack(5));
    CHECK_FALSE(f.mine().active.has_value());
    CHECK(f.mine().hand.size() == hand + 3); // Snipermon and 2 Energy
    REQUIRE(f.game.decision().kind == DecisionKind::PromoteActive);
    CHECK(f.game.decision().player == f.me);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::Promote));
    CHECK(f.game.current_player == f.them);
    f.check_cards();
}

TEST_CASE("a copy attack uses a Benched N's Pokemon's attack as the attacker", "[targets]") {
    Fixture f;
    pokebot::apply(f.game, attack(6));
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseAttack);
    const auto options = pokebot::legal_actions(f.game);
    REQUIRE(options.size() == 1); // Bite; Copycat is a copy attack itself
    CHECK(f.game.def(options[0].card).attacks[options[0].amount].name == "Bite");

    pokebot::apply(f.game, options[0]);
    CHECK(f.theirs().active->damage_counters == 4); // 20 x 2: Snipermon is Fire, so Weakness applies
    CHECK(f.game.current_player == f.them);
    CHECK(f.game.effects.empty());
    f.check_cards();
}

TEST_CASE("a copy attack with nothing to copy does nothing", "[targets]") {
    Fixture f;
    test::clear_bench(f.game, f.me);
    pokebot::apply(f.game, attack(6));
    CHECK(f.theirs().active->damage_counters == 0);
    CHECK(f.game.current_player == f.them);
}

TEST_CASE("Hammer discards an opponent's Energy into their own discard pile on heads", "[targets]") {
    int discarded = 0;
    for (int seed = 0; seed < 12; ++seed) {
        Fixture f;
        for (int i = 0; i < seed; ++i) {
            f.game.rng.coin_flip();
        }
        test::attach_from_deck(f.game, f.them, pokebot::bench_slot(1), "TST-5");
        const auto their_discard = f.theirs().discard.size();
        test::put_in_hand(f.game, f.me, "TST-6");
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-6"));
        if (f.game.decision().kind == DecisionKind::ChoosePokemon) {
            const auto actions = pokebot::legal_actions(f.game);
            REQUIRE(actions.size() == 1); // only the Pokemon with Energy
            pokebot::apply(f.game, actions[0]);
            pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-5"));
            CHECK(f.theirs().discard.size() == their_discard + 1);
            CHECK(f.theirs().bench[1].energy.empty());
            ++discarded;
        }
        CHECK(f.game.decision().kind == DecisionKind::MainPhase);
        f.check_cards();
    }
    CHECK(discarded > 0);
    CHECK(discarded < 12);
}
