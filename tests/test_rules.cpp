#include <catch2/catch_test_macros.hpp>

#include "engine/engine.hpp"
#include "engine/rules/rules.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::EnergyType;
using pokebot::GameState;
using pokebot::kActiveSlot;
namespace rules = pokebot::rules;

namespace {

    // TST-1 Basicmon (Water, 70 HP, Retreat 2): Splash [W C] 20. TST-2 Evomon evolves from Basicmon.
    // TST-3 Bigmon ex. TST-4 Water Energy.
    CardDB sample_db() {
        using E = EnergyType;
        CardDef basic = test::basic_pokemon("TST-1", "Basicmon", 70, 2);
        basic.types = {E::Water};
        basic.attacks = {test::attack("Splash", {E::Water, E::Colorless}, 20)};

        CardDef big = test::basic_pokemon("TST-3", "Bigmon ex", 200);
        big.rules.ex = true;

        CardDB db;
        db.add(basic);
        db.add(test::evolution("TST-2", "Evomon", pokebot::Stage::Stage1, "Basicmon"));
        db.add(big);
        db.add(test::basic_energy("TST-4", "Water Energy", E::Water));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {48, 4}});

    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 3);
        int me = 0;

        Fixture() {
            test::play_setup(game);
            me = game.current_player;
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        const pokebot::PokemonInPlay& active() const { return *game.player(me).active; }
    };

} // namespace

TEST_CASE("rules answer HP and Knock Outs from the printed HP and damage counters", "[rules]") {
    Fixture f;
    test::set_active(f.game, f.me, "TST-1");
    auto& active = *f.game.player(f.me).active;

    CHECK(rules::max_hp(f.game, active) == 70);
    active.damage_counters = 6;
    CHECK(rules::remaining_hp(f.game, active) == 10);
    CHECK_FALSE(rules::is_knocked_out(f.game, active));
    active.damage_counters = 7;
    CHECK(rules::is_knocked_out(f.game, active));
}

TEST_CASE("rules answer the printed type, costs, Energy and Prize cards", "[rules]") {
    using E = EnergyType;
    Fixture f;
    test::set_active(f.game, f.me, "TST-1");

    CHECK(rules::types(f.game, f.active()) == rules::EnergyList{E::Water});
    CHECK(rules::retreat_cost(f.game, f.active()) == 2);
    CHECK(rules::attack_cost(f.game, f.active(), 0) == rules::EnergyList{E::Water, E::Colorless});
    CHECK(rules::prize_value(f.game, f.active()) == 1);

    const auto energy = test::take_from_deck(f.game, f.me, "TST-4");
    CHECK(rules::energy_provided(f.game, f.active(), energy) == rules::EnergyList{E::Water});

    test::set_active(f.game, f.me, "TST-3");
    CHECK(rules::prize_value(f.game, f.active()) == 2);
}

TEST_CASE("rules apply the rulebook's turn limits", "[rules]") {
    Fixture f;
    CHECK(rules::bench_size(f.game, f.me) == 5);
    CHECK(rules::is_first_turn(f.game));
    CHECK_FALSE(rules::can_attack(f.game, f.me));
    CHECK(rules::can_attach_energy(f.game, f.me));

    test::end_turns(f.game, 1);
    CHECK(rules::can_attack(f.game, f.game.current_player));
}

TEST_CASE("rules allow evolving only after the first turns and not on the turn a Pokemon was played", "[rules]") {
    Fixture f;
    test::set_active(f.game, f.me, "TST-1");
    const auto evomon = test::take_from_deck(f.game, f.me, "TST-2");
    const auto bigmon = test::take_from_deck(f.game, f.me, "TST-3");

    CHECK_FALSE(rules::can_evolve(f.game, f.active()));
    CHECK(rules::evolves_into(f.game, f.active(), evomon));
    CHECK_FALSE(rules::evolves_into(f.game, f.active(), bigmon));

    f.game.turn = 3;
    CHECK(rules::can_evolve(f.game, f.active()));
    f.game.player(f.me).active->turn_played = 3;
    CHECK_FALSE(rules::can_evolve(f.game, f.active()));
}
