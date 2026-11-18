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

    CardDef trainer(const char* id, const char* name, const char* requirements, const char* effects) {
        CardDef c;
        c.id = id;
        c.name = name;
        c.kind = pokebot::CardKind::Trainer;
        c.trainer_kind = TrainerKind::Item;
        c.text = "test card";
        c.requirements = json::parse(requirements);
        c.effects = json::parse(effects);
        return c;
    }

    // TST-1 Formulamon (200 HP), all attacks [C]:
    //   0 Hand Hit 10x (10 per card in the opponent's hand)    1 Rondo 20+ (+10 per Benched Pokemon, both sides)
    //   2 Combo 10+ (+10 per heads, flip until tails)          3 Spinner 30+ (may put a Darkness Energy into hand: +50)
    //   4 Thunder 20x (discard any Basic Energy from your Pokemon: 20 each)    5 Rage 10x (10 per damage counter on itself)
    // TST-2 Targetmon (300 HP). TST-3 Darkness Energy. TST-4 Red Card. TST-5 Cipher.
    CardDB sample_db() {
        using E = EnergyType;
        CardDef formula = test::basic_pokemon("TST-1", "Formulamon", 200);
        const auto with = [](const char* name, int damage, const char* effects) {
            auto a = test::attack(name, {E::Colorless}, damage);
            a.effects = json::parse(effects);
            return a;
        };
        formula.attacks = {
            with("Hand Hit", 10, R"([{"op": "set_damage", "per": 10, "for_each": {"of": "hand", "player": "opponent"}}, {"op": "damage"}])"),
            with("Rondo", 20, R"([{"op": "add_damage", "per": 10, "for_each": {"of": "benched", "player": "each"}}, {"op": "damage"}])"),
            with("Combo", 10, R"([{"op": "flip_until_tails"}, {"op": "add_damage", "per": 10, "for_each": {"of": "heads"}}, {"op": "damage"}])"),
            with("Spinner", 30, R"([{"op": "may", "effects": [{"op": "move_attached_energy", "filter": {"type": "darkness"}}]},
                                    {"op": "if", "condition": {"moved_at_least": 1}, "then": [{"op": "add_damage", "amount": 50}]},
                                    {"op": "damage"}])"),
            with("Thunder", 20, R"([{"op": "move_attached_energy", "from": "your_pokemon", "count": 60, "up_to": true,
                                     "filter": {"kind": "energy", "basic": true}, "to": "discard"},
                                    {"op": "set_damage", "per": 20, "for_each": {"of": "moved"}}, {"op": "damage"}])"),
            with("Rage", 10, R"([{"op": "set_damage", "per": 10, "for_each": {"of": "damage_counters"}}, {"op": "damage"}])"),
        };

        CardDB db;
        db.add(formula);
        db.add(test::basic_pokemon("TST-2", "Targetmon", 300));
        db.add(test::basic_energy("TST-3", "Darkness Energy", E::Darkness));
        db.add(trainer("TST-4", "Red Card", R"([{"opponent_prizes_at_most": 3}])",
                       R"([{"op": "hand_to_deck_bottom", "player": "opponent"},
                           {"op": "if", "condition": {"moved_at_least": 1}, "then": [{"op": "draw", "count": 3, "player": "opponent"}]}])"));
        db.add(trainer("TST-5", "Cipher", "[]", R"([{"op": "search_to_top", "count": 2}])"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {44, 3}, {4, 4}, {4, 5}});

    Action attack(std::uint8_t index) {
        return Action{ActionKind::Attack, pokebot::kNoCard, kActiveSlot, index};
    }

    // Turn 2: `me` has Formulamon (one Energy) Active and Targetmon on the Bench; `them` has Targetmon Active and on the Bench.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 12);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            test::set_active(game, me, "TST-1");
            test::set_active(game, them, "TST-2");
            for (const int p : {me, them}) {
                test::clear_bench(game, p);
                test::add_to_bench(game, p, "TST-2");
            }
            test::attach_from_deck(game, me, kActiveSlot, "TST-3");
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        pokebot::PlayerState& mine() { return game.player(me); }
        int counters_on_them() { return game.player(them).active->damage_counters; }

        void check_cards() const {
            CHECK(test::cards_accounted_for(game.player(0)) == 60);
            CHECK(test::cards_accounted_for(game.player(1)) == 60);
            CHECK(pokebot::check_invariants(game).empty());
        }
    };

} // namespace

TEST_CASE("x damage: 10 for each card in the opponent's hand", "[damage]") {
    Fixture f;
    const auto hand = static_cast<int>(f.game.player(f.them).hand.size());
    REQUIRE(hand > 0);
    pokebot::apply(f.game, attack(0));
    CHECK(f.counters_on_them() == hand);
    f.check_cards();
}

TEST_CASE("+ damage: 10 more for each Benched Pokemon on both sides", "[damage]") {
    Fixture f;
    pokebot::apply(f.game, attack(1));
    CHECK(f.counters_on_them() == 4); // 20 + 10 x 2
}

TEST_CASE("flip until tails: 10 more for each heads", "[damage]") {
    bool saw_heads = false;
    for (int seed = 0; seed < 12; ++seed) {
        Fixture f;
        for (int i = 0; i < seed; ++i) {
            f.game.rng.coin_flip();
        }
        pokebot::apply(f.game, attack(2));
        CHECK(f.counters_on_them() >= 1);
        saw_heads = saw_heads || f.counters_on_them() > 1;
    }
    CHECK(saw_heads);
}

TEST_CASE("an \"if you do\" bonus only counts when the Energy was moved", "[damage]") {
    SECTION("yes") {
        Fixture f;
        const auto hand = f.mine().hand.size();
        pokebot::apply(f.game, attack(3));
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::AnswerYes));
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-3"));
        CHECK(f.counters_on_them() == 8); // 30 + 50
        CHECK(f.mine().hand.size() == hand + 1);
        f.check_cards();
    }
    SECTION("no") {
        Fixture f;
        pokebot::apply(f.game, attack(3));
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::AnswerNo));
        CHECK(f.counters_on_them() == 3);
    }
}

TEST_CASE("discarding any amount of Basic Energy from your Pokemon: 20 for each", "[damage]") {
    Fixture f;
    test::attach_from_deck(f.game, f.me, kActiveSlot, "TST-3");
    test::attach_from_deck(f.game, f.me, pokebot::bench_slot(0), "TST-3");
    const auto discard = f.mine().discard.size();
    pokebot::apply(f.game, attack(4));

    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    CHECK(f.game.decision().amount == 3); // two on the Active, one on the Bench
    CHECK(test::offers(pokebot::legal_actions(f.game), ActionKind::ChooseDone));
    for (int i = 0; i < 3; ++i) {
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-3"));
    }
    CHECK(f.counters_on_them() == 6);
    CHECK(f.mine().discard.size() == discard + 3);
    CHECK(f.mine().bench[0].energy.empty());
    f.check_cards();
}

TEST_CASE("discarding nothing does no damage", "[damage]") {
    Fixture f;
    pokebot::apply(f.game, attack(4));
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseDone));
    CHECK(f.counters_on_them() == 0);
}

TEST_CASE("x damage: 10 for each damage counter on the attacker", "[damage]") {
    Fixture f;
    f.mine().active->damage_counters = 5;
    pokebot::apply(f.game, attack(5));
    CHECK(f.counters_on_them() == 5);
}

TEST_CASE("Red Card: the opponent's hand goes to the bottom of their deck and they draw 3", "[damage]") {
    Fixture f;
    test::put_in_hand(f.game, f.me, "TST-4");
    CHECK_FALSE(test::offers(pokebot::legal_actions(f.game), ActionKind::PlayTrainer)); // they have 6 Prize cards

    auto& them = f.game.player(f.them);
    while (them.prizes.size() > 3) {
        them.discard.push_back(them.prizes.pop_back());
    }
    const auto old_hand = them.hand;
    REQUIRE(!old_hand.empty());
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-4"));

    CHECK(them.hand.size() == 3);
    for (std::size_t i = 0; i < old_hand.size(); ++i) {
        CHECK(old_hand.contains(them.deck[i])); // the bottom of the deck is the old hand
    }
    f.check_cards();
}

TEST_CASE("Cipher puts the 2 chosen cards on top of the deck, the last one chosen on top", "[damage]") {
    Fixture f;
    test::put_in_hand(f.game, f.me, "TST-5");
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-5"));
    const auto first = test::find_action(f.game, ActionKind::ChooseCard, "TST-2").card;
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-2"));
    const auto second = test::find_action(f.game, ActionKind::ChooseCard, "TST-1").card;
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-1"));

    const auto& deck = f.mine().deck;
    CHECK(deck[deck.size() - 1] == second);
    CHECK(deck[deck.size() - 2] == first);
    f.check_cards();
}

TEST_CASE("damage formulas are checked when the cards load", "[damage]") {
    using pokebot::CardDataError;
    using pokebot::compile_attack;
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "add_damage", "amount": 30}])")), CardDataError); // no damage step
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "add_damage", "per": 30}, {"op": "damage"}])")), CardDataError);
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "set_damage", "per": 30, "for_each": {"of": "stars"}}, {"op": "damage"}])")),
                    CardDataError);
    CHECK_THROWS_AS(pokebot::compile_trainer(json::parse(R"([{"op": "add_damage", "amount": 30}])")), CardDataError);
    CHECK_NOTHROW(compile_attack(json::parse(R"([{"op": "flip_coins", "count": 2}, {"op": "set_damage", "per": 30, "for_each": {"of": "heads"}}, {"op": "damage"}])")));
}
