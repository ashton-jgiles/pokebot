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

    Action choose(pokebot::Slot slot) {
        return Action{ActionKind::ChoosePokemon, pokebot::kNoCard, slot, 0};
    }

    // TST-1 Plainmon. TST-2 N's Mon. TST-3 Darkmon (Darkness). TST-4 Darkness Energy. TST-5 Fire Energy.
    // TST-6 PP Up: a Basic Energy from the discard pile to 1 of your Benched N's Pokemon.
    // TST-7 Secret Art: up to 2 of your Darkness Pokemon each get a Darkness Energy from the deck; if the Active got one, it's Poisoned.
    // TST-8 Encourage: only with more Prize cards left than the opponent; up to 2 Basic Energy from the discard pile to 1 of your Pokemon.
    // TST-9 Switcher: move a Basic Energy from 1 of your Pokemon to another.
    CardDB sample_db() {
        using E = EnergyType;
        CardDB db;
        db.add(test::basic_pokemon("TST-1", "Plainmon", 100));
        db.add(test::basic_pokemon("TST-2", "N's Mon", 100));
        CardDef dark = test::basic_pokemon("TST-3", "Darkmon", 100);
        dark.types = {E::Darkness};
        db.add(dark);
        db.add(test::basic_energy("TST-4", "Darkness Energy", E::Darkness));
        db.add(test::basic_energy("TST-5", "Fire Energy", E::Fire));
        db.add(trainer("TST-6", "PP Up", R"([{"discard_has": {"kind": "energy", "basic": true}}, {"bench_has": {"name_prefix": "N's"}}])",
                       R"([{"op": "attach_to", "on": "bench", "target": {"name_prefix": "N's"}, "filter": {"kind": "energy", "basic": true}}])"));
        db.add(trainer("TST-7", "Secret Art", "[]",
                       R"([{"op": "attach_to", "from": "deck", "targets": 2, "up_to_targets": true, "target": {"type": "darkness"},
                            "filter": {"kind": "energy", "type": "darkness"}},
                           {"op": "if", "condition": {"attached_to_active": true}, "then": [{"op": "special_condition", "condition": "poisoned", "player": "you"}]}])"));
        db.add(trainer("TST-8", "Encourage", R"([{"more_prizes_than_opponent": true}])",
                       R"([{"op": "attach_to", "count": 2, "up_to": true, "filter": {"kind": "energy", "basic": true}}])"));
        db.add(trainer("TST-9", "Switcher", R"([{"your_energy_has": {"kind": "energy", "basic": true}}, {"has_benched": true}])",
                       R"([{"op": "move_energy", "filter": {"kind": "energy", "basic": true}}])"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {16, 4}, {16, 5}, {4, 6}, {4, 7}, {4, 8}, {4, 9}});

    // Turn 2: `me` has Darkmon Active, Plainmon and N's Mon on the Bench.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 26);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            test::set_active(game, me, "TST-3");
            test::clear_bench(game, me);
            test::add_to_bench(game, me, "TST-1");
            test::add_to_bench(game, me, "TST-2");
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        pokebot::PlayerState& mine() { return game.player(me); }

        void to_discard(const char* id) { mine().discard.push_back(test::take_from_deck(game, me, id)); }

        bool can_play(const char* id) {
            for (const auto& action : pokebot::legal_actions(game)) {
                if (action.kind == ActionKind::PlayTrainer && game.def(action.card).id == id) {
                    return true;
                }
            }
            return false;
        }

        void play(const char* id) {
            test::put_in_hand(game, me, id);
            pokebot::apply(game, test::find_action(game, ActionKind::PlayTrainer, id));
        }

        void check_cards() const {
            CHECK(test::cards_accounted_for(game.player(0)) == 60);
            CHECK(test::cards_accounted_for(game.player(1)) == 60);
            CHECK(pokebot::check_invariants(game).empty());
        }
    };

} // namespace

TEST_CASE("attaching a Basic Energy from the discard pile to a matching Benched Pokemon", "[attach]") {
    Fixture f;
    test::put_in_hand(f.game, f.me, "TST-6");
    CHECK_FALSE(f.can_play("TST-6")); // nothing in the discard pile
    f.to_discard("TST-5");
    REQUIRE(f.can_play("TST-6"));
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-6"));
    REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
    const auto targets = pokebot::legal_actions(f.game);
    REQUIRE(targets.size() == 1); // only N's Mon
    CHECK(targets[0].target == pokebot::bench_slot(1));
    pokebot::apply(f.game, targets[0]);
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-5"));
    CHECK(f.mine().bench[1].energy.size() == 1);
    CHECK(f.game.decision().kind == DecisionKind::MainPhase);
    f.check_cards();
}

TEST_CASE("each chosen Pokemon gets its own Energy from the deck", "[attach]") {
    Fixture f;
    test::add_to_bench(f.game, f.me, "TST-3"); // a second Darkness Pokemon
    f.play("TST-7");
    REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
    CHECK(test::offers(pokebot::legal_actions(f.game), ActionKind::ChooseDone)); // "up to 2"
    pokebot::apply(f.game, choose(pokebot::bench_slot(2)));
    pokebot::apply(f.game, choose(kActiveSlot));
    for (int i = 0; i < 2; ++i) {
        REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-4"));
    }
    CHECK(f.mine().bench[2].energy.size() == 1);
    CHECK(f.mine().active->energy.size() == 1);
    CHECK(f.mine().active->special.poison == 1); // the Active got one
    f.check_cards();
}

TEST_CASE("the Active isn't Poisoned when it got no Energy", "[attach]") {
    Fixture f;
    test::add_to_bench(f.game, f.me, "TST-3");
    f.play("TST-7");
    pokebot::apply(f.game, choose(pokebot::bench_slot(2)));
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseDone));
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-4"));
    CHECK(f.mine().bench[2].energy.size() == 1);
    CHECK(f.mine().active->special.poison == 0);
    f.check_cards();
}

TEST_CASE("up to 2 Energy to one Pokemon, only with more Prize cards left", "[attach]") {
    Fixture f;
    f.to_discard("TST-4");
    f.to_discard("TST-5");
    test::put_in_hand(f.game, f.me, "TST-8");
    CHECK_FALSE(f.can_play("TST-8")); // 6 Prize cards each
    auto& them = f.game.player(f.them);
    them.discard.push_back(them.prizes.pop_back());
    REQUIRE(f.can_play("TST-8"));
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-8"));
    pokebot::apply(f.game, choose(pokebot::bench_slot(0)));
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-4"));
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards); // a second one is allowed
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-5"));
    CHECK(f.mine().bench[0].energy.size() == 2);
    f.check_cards();
}

TEST_CASE("moving a Basic Energy from one of your Pokemon to another", "[attach]") {
    Fixture f;
    test::attach_from_deck(f.game, f.me, kActiveSlot, "TST-4");
    f.play("TST-9");
    REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
    const auto sources = pokebot::legal_actions(f.game);
    REQUIRE(sources.size() == 1); // only the Active has Energy
    pokebot::apply(f.game, sources[0]);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-4"));
    REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
    for (const auto& action : pokebot::legal_actions(f.game)) {
        CHECK(action.target != kActiveSlot); // not back onto the same Pokemon
    }
    pokebot::apply(f.game, choose(pokebot::bench_slot(1)));
    CHECK(f.mine().active->energy.empty());
    CHECK(f.mine().bench[1].energy.size() == 1);
    f.check_cards();
}

TEST_CASE("attach_to is checked when the cards load", "[attach]") {
    using pokebot::CardDataError;
    using pokebot::compile_trainer;
    CHECK_THROWS_AS(compile_trainer(json::parse(R"([{"op": "attach_to", "from": "space"}])")), CardDataError);
    CHECK_THROWS_AS(compile_trainer(json::parse(R"([{"op": "attach_to", "targets": 9}])")), CardDataError);
    const auto program = compile_trainer(json::parse(R"([{"op": "attach_to", "from": "deck"}])"));
    REQUIRE(program.size() == 6);
    CHECK(program[1].op == pokebot::Op::EachTarget);
    CHECK(program[1].jump == 5);
    CHECK(program[4].op == pokebot::Op::NextTarget);
    CHECK(program[4].jump == 1);
    CHECK(program[5].op == pokebot::Op::ShuffleDeck);
}
