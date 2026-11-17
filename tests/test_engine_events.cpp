#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine/engine.hpp"
#include "engine/events/events.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using pokebot::Action;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::DecisionKind;
using pokebot::EnergyType;
using pokebot::EventKind;
using pokebot::GameState;
using pokebot::kActiveSlot;

namespace {

    // TST-1 Attackmon (100 HP): Tackle [C] 30. TST-2 Weakmon (30 HP). TST-3 Benchmon (100 HP).
    // TST-4 Evomon evolves from Attackmon. TST-5 Darkness Energy.
    CardDB sample_db() {
        using E = EnergyType;
        CardDef attacker = test::basic_pokemon("TST-1", "Attackmon", 100);
        attacker.attacks = {test::attack("Tackle", {E::Colorless}, 30)};

        CardDB db;
        db.add(attacker);
        db.add(test::basic_pokemon("TST-2", "Weakmon", 30));
        db.add(test::basic_pokemon("TST-3", "Benchmon", 100));
        db.add(test::evolution("TST-4", "Evomon", pokebot::Stage::Stage1, "Attackmon"));
        db.add(test::basic_energy("TST-5", "Darkness Energy", E::Darkness));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {4, 4}, {44, 5}});
    const Action kTackle{ActionKind::Attack, pokebot::kNoCard, kActiveSlot, 0};

    std::vector<pokebot::Event> seen;

    void record(const GameState&, const pokebot::Event& event) {
        seen.push_back(event);
    }

    std::vector<EventKind> kinds() {
        std::vector<EventKind> out;
        for (const auto& event : seen) {
            out.push_back(event.kind);
        }
        return out;
    }

    // Turn 2: `me` has Attackmon with one Energy; `them` has Weakmon Active and Benchmon on the Bench.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 5);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            test::set_active(game, me, "TST-1");
            test::attach_from_deck(game, me, kActiveSlot, "TST-5");
            test::set_active(game, them, "TST-2");
            test::clear_bench(game, them);
            test::add_to_bench(game, them, "TST-3");
            seen.clear();
            pokebot::set_event_hook(record);
        }
        ~Fixture() { pokebot::set_event_hook(nullptr); }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;
    };

} // namespace

TEST_CASE("an attack that Knocks Out raises its events in order", "[engine][events]") {
    Fixture f;
    const auto attacker = f.game.player(f.me).active->id();
    const auto defender = f.game.player(f.them).active->id();

    pokebot::apply(f.game, kTackle);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::Promote));

    CHECK(kinds() == std::vector<EventKind>{EventKind::DamagedByAttack, EventKind::AttackDone, EventKind::KnockedOut,
                                            EventKind::TurnEnding, EventKind::Checkup});
    CHECK(seen[0].pokemon == defender);
    CHECK(seen[0].card == attacker);
    CHECK(seen[0].player == f.them);
    CHECK(seen[2].pokemon == defender);
}

TEST_CASE("benching, evolving and attaching Energy raise events", "[engine][events]") {
    Fixture f;
    f.game.turn = 3; // past the first turns, so evolving is allowed
    test::put_in_hand(f.game, f.me, "TST-3");
    test::put_in_hand(f.game, f.me, "TST-4");
    test::put_in_hand(f.game, f.me, "TST-5");

    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayBasic, "TST-3"));
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::Evolve, "TST-4"));
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::AttachEnergy, "TST-5", kActiveSlot));

    CHECK(kinds() == std::vector<EventKind>{EventKind::PutOnBench, EventKind::Evolved, EventKind::EnergyAttached});
    CHECK(f.game.def(seen[1].card).id == "TST-4");
    CHECK(seen[1].pokemon == f.game.player(f.me).active->id());
}

TEST_CASE("retreating raises Retreated and MovedToBench for the Pokemon that left", "[engine][events]") {
    Fixture f;
    test::add_to_bench(f.game, f.me, "TST-3");
    const auto retreating = f.game.player(f.me).active->id();

    pokebot::apply(f.game, test::find_action(f.game, ActionKind::Retreat));

    CHECK(kinds() == std::vector<EventKind>{EventKind::Retreated, EventKind::MovedToBench});
    CHECK(seen[1].pokemon == retreating);
}

TEST_CASE("history remembers the last two turns' attacks and Knock Outs", "[engine][events]") {
    Fixture f;
    const auto attacker = f.game.player(f.me).active->id();

    pokebot::apply(f.game, kTackle);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::Promote));

    // Their turn: my attack was during their opponent's last turn.
    const auto& history = f.game.history;
    CHECK(history.this_turn().player == f.them);
    CHECK(history.opponents_last_turn().player == f.me);
    CHECK(history.opponents_last_turn().attacker == attacker);
    CHECK(history.opponents_last_turn().attack.index == 0);
    CHECK(history.opponents_last_turn().knocked_out[static_cast<std::size_t>(f.them)] == 1);
    CHECK(history.opponents_last_turn().knocked_out[static_cast<std::size_t>(f.me)] == 0);

    test::end_turns(f.game, 1); // back to me: that attack was during my last turn
    CHECK(f.game.history.your_last_turn().attacker == attacker);
    CHECK(f.game.history.opponents_last_turn().attacker == pokebot::kNoCard);
}

TEST_CASE("an extra attack offers that Pokemon's attacks or ending the turn", "[engine][events]") {
    Fixture f;
    auto& active = *f.game.player(f.me).active;
    f.game.pending.back() = pokebot::Decision{DecisionKind::ExtraAttack, static_cast<std::uint8_t>(f.me), 0, active.id()};
    f.game.flags.attacks_used = 1;

    auto actions = pokebot::legal_actions(f.game);
    CHECK(actions.size() == 2);
    CHECK(test::offers(actions, ActionKind::Attack));
    CHECK(test::offers(actions, ActionKind::EndTurn));

    SECTION("a second attack ends the turn once the attacks allowed are used") {
        test::set_active(f.game, f.them, "TST-3"); // 100 HP: no Knock Out
        pokebot::apply(f.game, kTackle);
        CHECK(f.game.current_player == f.them);
        CHECK(f.game.decision().kind == DecisionKind::MainPhase);
    }
    SECTION("only the Pokemon that attacked may attack again") {
        f.game.pending.back().target = f.game.player(f.them).active->id();
        actions = pokebot::legal_actions(f.game);
        CHECK(actions.size() == 1);
        CHECK(actions[0].kind == ActionKind::EndTurn);
    }
}
