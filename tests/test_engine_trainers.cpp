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

    CardDef trainer(const char* id, const char* name, TrainerKind kind, const char* requirements, const char* effects) {
        CardDef c;
        c.id = id;
        c.name = name;
        c.kind = pokebot::CardKind::Trainer;
        c.trainer_kind = kind;
        c.text = "test card";
        c.requirements = json::parse(requirements);
        c.effects = json::parse(effects);
        return c;
    }

    // TST-1 Basicmon (60 HP), TST-2 Bigmon (100 HP), TST-3 Energy.
    // Trainers: TST-4 Draw Item, TST-5 Draw Supporter, TST-6 Stadium A, TST-7 Stadium B, TST-8 Tool,
    // TST-9 Ball (discard 2, search a Pokemon), TST-10 Boss (gust), TST-11 Poffin (Basic with 70 HP or less to Bench),
    // TST-12 Choice (draw 3 with exactly 6 Prizes, else 1), TST-13 Judge (each shuffles and draws 4),
    // TST-14 Stretcher (a Pokemon or Basic Energy from discard), TST-15 Unwritten (text but no effects).
    CardDB sample_db() {
        CardDB db;
        db.add(test::basic_pokemon("TST-1", "Basicmon", 60));
        db.add(test::basic_pokemon("TST-2", "Bigmon", 100));
        db.add(test::basic_energy("TST-3", "Darkness Energy", EnergyType::Darkness));
        db.add(trainer("TST-4", "Draw Item", TrainerKind::Item, "[]", R"([{"op": "draw"}])"));
        db.add(trainer("TST-5", "Draw Supporter", TrainerKind::Supporter, "[]", R"([{"op": "draw", "count": 2}])"));
        db.add(trainer("TST-6", "Stadium A", TrainerKind::Stadium, "[]", "[]"));
        db.add(trainer("TST-7", "Stadium B", TrainerKind::Stadium, "[]", "[]"));
        db.add(trainer("TST-8", "Tool", TrainerKind::Tool, "[]", "[]"));
        db.add(trainer("TST-9", "Ball", TrainerKind::Item, R"([{"hand_besides": 2}])",
                       R"([{"op": "discard_from_hand", "count": 2}, {"op": "search_deck", "filter": {"kind": "pokemon"}}])"));
        db.add(trainer("TST-10", "Boss", TrainerKind::Supporter, R"([{"opponent_has_benched": true}])",
                       R"([{"op": "switch_active", "player": "opponent"}])"));
        db.add(trainer("TST-11", "Poffin", TrainerKind::Item, R"([{"bench_not_full": true}])",
                       R"([{"op": "search_deck", "count": 2, "to": "bench", "filter": {"stage": "basic", "max_hp": 70}}])"));
        db.add(trainer("TST-12", "Choice", TrainerKind::Item, "[]",
                       R"([{"op": "if", "condition": {"prizes_exactly": 6}, "then": [{"op": "draw", "count": 3}], "else": [{"op": "draw"}]}])"));
        db.add(trainer("TST-13", "Judge", TrainerKind::Supporter, "[]",
                       R"([{"op": "shuffle_hand_into_deck", "player": "each"}, {"op": "draw", "count": 4, "player": "each"}])"));
        db.add(trainer("TST-14", "Stretcher", TrainerKind::Item,
                       R"([{"discard_has": {"any_of": [{"kind": "pokemon"}, {"kind": "energy", "basic": true}]}}])",
                       R"([{"op": "from_discard", "filter": {"any_of": [{"kind": "pokemon"}, {"kind": "energy", "basic": true}]}}])"));
        db.add(trainer("TST-15", "Unwritten", TrainerKind::Item, "[]", "[]"));
        return db;
    }

    const pokebot::Decklist kDeck =
        test::deck({{4, 1}, {4, 2}, {16, 3}, {3, 4}, {3, 5}, {3, 6}, {3, 7}, {3, 8}, {3, 9}, {3, 10}, {3, 11}, {3, 12}, {3, 13}, {3, 14}, {3, 15}});

    // Turn 2: `me` has Basicmon Active and an empty Bench; `them` has Basicmon Active and Bigmon on the Bench.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 8);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            test::set_active(game, me, "TST-1");
            test::clear_bench(game, me);
            test::set_active(game, them, "TST-1");
            test::clear_bench(game, them);
            test::add_to_bench(game, them, "TST-2");
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        pokebot::PlayerState& mine() { return game.player(me); }

        void play(const char* id) { pokebot::apply(game, test::find_action(game, ActionKind::PlayTrainer, id)); }

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

TEST_CASE("an Item does its effect, goes to the discard pile, and the turn goes on", "[trainers]") {
    Fixture f;
    const auto card = test::put_in_hand(f.game, f.me, "TST-4");
    const auto hand = f.mine().hand.size();
    f.play("TST-4");
    CHECK(f.mine().hand.size() == hand); // played one, drew one
    CHECK(f.mine().discard.contains(card));
    CHECK(f.game.decision().kind == DecisionKind::MainPhase);
    f.check_cards();
}

TEST_CASE("one Supporter per turn, and none on the first player's first turn", "[trainers]") {
    Fixture f;
    test::put_in_hand(f.game, f.me, "TST-5");
    test::put_in_hand(f.game, f.me, "TST-5");
    REQUIRE(f.can_play("TST-5"));
    f.play("TST-5");
    CHECK_FALSE(f.can_play("TST-5"));

    f.game.turn = 1;
    f.game.flags.supporter_played = false;
    CHECK_FALSE(f.can_play("TST-5"));
}

TEST_CASE("a new Stadium replaces the old one, but not one with the same name", "[trainers]") {
    Fixture f;
    const auto a = test::put_in_hand(f.game, f.me, "TST-6");
    test::put_in_hand(f.game, f.me, "TST-7");
    f.play("TST-6");
    CHECK(f.mine().stadium == a);
    CHECK_FALSE(f.can_play("TST-7")); // one Stadium per turn

    test::end_turns(f.game, 1);
    test::put_in_hand(f.game, f.them, "TST-6");
    const auto b = test::put_in_hand(f.game, f.them, "TST-7");
    CHECK_FALSE(f.can_play("TST-6")); // same name as the one in play
    f.play("TST-7");
    CHECK(f.game.player(f.them).stadium == b);
    CHECK(f.mine().stadium == pokebot::kNoCard);
    CHECK(f.mine().discard.contains(a)); // back to its owner's discard pile
    CHECK(pokebot::rules::stadium_in_play(f.game) == b);
    f.check_cards();
}

TEST_CASE("a Tool attaches to a Pokemon that doesn't have one", "[trainers]") {
    Fixture f;
    const auto tool = test::put_in_hand(f.game, f.me, "TST-8");
    test::put_in_hand(f.game, f.me, "TST-8");
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::AttachTool, "TST-8", kActiveSlot));
    CHECK(f.mine().active->tool == tool);
    CHECK_FALSE(test::offers(pokebot::legal_actions(f.game), ActionKind::AttachTool));
    f.check_cards();
}

TEST_CASE("a card's requirements decide whether it can be played", "[trainers]") {
    Fixture f;
    while (!f.mine().hand.empty()) {
        f.mine().deck.push_back(f.mine().hand.pop_back());
    }
    test::put_in_hand(f.game, f.me, "TST-9");
    test::put_in_hand(f.game, f.me, "TST-3");
    CHECK_FALSE(f.can_play("TST-9")); // only 1 other card
    test::put_in_hand(f.game, f.me, "TST-3");
    CHECK(f.can_play("TST-9"));

    test::put_in_hand(f.game, f.me, "TST-15");
    CHECK_FALSE(f.can_play("TST-15")); // its effects aren't written yet
}

TEST_CASE("a Ball discards 2 cards, then searches, while the card waits to be discarded", "[trainers]") {
    Fixture f;
    const auto ball = test::put_in_hand(f.game, f.me, "TST-9");
    test::put_in_hand(f.game, f.me, "TST-3");
    test::put_in_hand(f.game, f.me, "TST-3");
    const auto discard = f.mine().discard.size();
    f.play("TST-9");

    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    CHECK(f.mine().resolving.contains(ball));
    CHECK_FALSE(f.mine().hand.contains(ball)); // it can't discard itself
    CHECK(pokebot::check_invariants(f.game).empty());
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-3"));
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-3"));

    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards); // now the search
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-2"));
    CHECK(f.mine().discard.size() == discard + 3); // 2 Energy and the Ball
    CHECK(f.mine().resolving.empty());
    CHECK(f.game.decision().kind == DecisionKind::MainPhase);
    f.check_cards();
}

TEST_CASE("Boss brings up one of the opponent's Benched Pokemon", "[trainers]") {
    Fixture f;
    const auto bigmon = f.game.player(f.them).bench[0].id();
    test::put_in_hand(f.game, f.me, "TST-10");
    f.play("TST-10");
    REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
    CHECK(f.game.decision().player == f.me);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChoosePokemon));
    CHECK(f.game.player(f.them).active->id() == bigmon);
    f.check_cards();
}

TEST_CASE("Boss can't be played when the opponent has no Benched Pokemon", "[trainers]") {
    Fixture f;
    test::clear_bench(f.game, f.them);
    test::put_in_hand(f.game, f.me, "TST-10");
    CHECK_FALSE(f.can_play("TST-10"));
}

TEST_CASE("Poffin puts Basic Pokemon with 70 HP or less onto the Bench, as many as fit", "[trainers]") {
    Fixture f;
    for (int i = 0; i < 4; ++i) {
        test::add_to_bench(f.game, f.me, "TST-2");
    }
    test::put_in_hand(f.game, f.me, "TST-11");
    f.play("TST-11");

    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    CHECK(f.game.decision().amount == 1); // only one Bench space
    for (const auto& action : pokebot::legal_actions(f.game)) {
        if (action.kind == ActionKind::ChooseCard) {
            CHECK(f.game.def(action.card).id == "TST-1"); // Bigmon has 100 HP
        }
    }
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-1"));
    CHECK(f.mine().bench.size() == 5);
    CHECK(f.game.def(f.mine().bench[4].top()).id == "TST-1");
    CHECK_FALSE(f.can_play("TST-11")); // Bench full
    f.check_cards();
}

TEST_CASE("an if step picks its branch when the card resolves", "[trainers]") {
    Fixture f;
    test::put_in_hand(f.game, f.me, "TST-12");
    test::put_in_hand(f.game, f.me, "TST-12");
    auto hand = f.mine().hand.size();
    f.play("TST-12"); // 6 Prize cards: draw 3
    CHECK(f.mine().hand.size() == hand - 1 + 3);

    f.mine().discard.push_back(f.mine().prizes.pop_back());
    hand = f.mine().hand.size();
    f.play("TST-12"); // 5 Prize cards: draw 1
    CHECK(f.mine().hand.size() == hand - 1 + 1);
    f.check_cards();
}

TEST_CASE("Judge: each player shuffles their hand away and draws 4", "[trainers]") {
    Fixture f;
    test::put_in_hand(f.game, f.me, "TST-13");
    f.play("TST-13");
    CHECK(f.mine().hand.size() == 4);
    CHECK(f.game.player(f.them).hand.size() == 4);
    f.check_cards();
}

TEST_CASE("Stretcher takes a Pokemon or a Basic Energy from the discard pile", "[trainers]") {
    Fixture f;
    test::put_in_hand(f.game, f.me, "TST-14");
    CHECK_FALSE(f.can_play("TST-14")); // nothing in the discard pile

    f.mine().discard.push_back(test::take_from_deck(f.game, f.me, "TST-4"));
    CHECK_FALSE(f.can_play("TST-14")); // an Item doesn't count
    f.mine().discard.push_back(test::take_from_deck(f.game, f.me, "TST-3"));
    REQUIRE(f.can_play("TST-14"));
    f.play("TST-14");

    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    CHECK_FALSE(test::offers(pokebot::legal_actions(f.game), ActionKind::ChooseDone)); // exactly 1
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard, "TST-3"));
    CHECK(f.game.decision().kind == DecisionKind::MainPhase);
    f.check_cards();
}
