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
namespace rules = pokebot::rules;

namespace {

    CardDef special_energy(const char* id, const char* name, const char* fields) {
        const json j = json::parse(fields);
        CardDef c;
        c.id = id;
        c.name = name;
        c.kind = pokebot::CardKind::Energy;
        c.energy_kind = pokebot::EnergyKind::Special;
        c.text = "test card";
        c.provides = {EnergyType::Colorless};
        c.provides_when = j.value("provides_when", json::array());
        c.discard_at_end_of_turn = j.value("discard_at_end_of_turn", false);
        return c;
    }

    // TST-1 Basicmon (Retreat 2). TST-2 Midmon (Stage 1 from Basicmon, Retreat 2). TST-3 Topmon (Stage 2 from Midmon).
    // TST-4 Fire Energy. TST-5 Water Energy.
    // TST-6 Upper: {C}; on a Stage 2, two units that can each be any type. TST-7 Ignite: {C}; on an Evolution, {C}{C}{C}; discarded at the end of your turn.
    CardDB sample_db() {
        using E = EnergyType;
        CardDB db;
        db.add(test::basic_pokemon("TST-1", "Basicmon", 100, 2));
        db.add(test::evolution("TST-2", "Midmon", pokebot::Stage::Stage1, "Basicmon", 120, 2));
        db.add(test::evolution("TST-3", "Topmon", pokebot::Stage::Stage2, "Midmon", 150, 2));
        db.add(test::basic_energy("TST-4", "Fire Energy", E::Fire));
        db.add(test::basic_energy("TST-5", "Water Energy", E::Water));
        db.add(special_energy("TST-6", "Upper", R"({"provides_when": [{"holder": {"stage": "stage2"}, "provides": ["any", "any"]}]})"));
        db.add(special_energy("TST-7", "Ignite", R"({"discard_at_end_of_turn": true,
                                                     "provides_when": [{"holder": {"stage": "evolution"}, "provides": ["colorless", "colorless", "colorless"]}]})"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {20, 4}, {20, 5}, {4, 6}, {4, 7}});

    // Turn 2: `me` has Basicmon Active with nothing attached and Basicmon on the Bench.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 24);
        int me = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            test::set_active(game, me, "TST-1");
            test::clear_bench(game, me);
            test::add_to_bench(game, me, "TST-1");
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        pokebot::PokemonInPlay& active() { return *game.player(me).active; }

        void evolve_active_to(const char* id) { active().stack.push_back(test::take_from_deck(game, me, id)); }

        void attach(const char* id) { test::attach_from_deck(game, me, kActiveSlot, id); }

        bool pays(std::initializer_list<EnergyType> cost) {
            rules::EnergyList list;
            for (const EnergyType symbol : cost) {
                list.push_back(symbol);
            }
            return pokebot::detail::can_pay_cost(game, active(), list);
        }

        void check_cards() const {
            CHECK(test::cards_accounted_for(game.player(0)) == 60);
            CHECK(test::cards_accounted_for(game.player(1)) == 60);
            CHECK(pokebot::check_invariants(game).empty());
        }
    };

} // namespace

TEST_CASE("a Special Energy provides its usual Energy unless its holder matches", "[energy]") {
    using E = EnergyType;
    Fixture f;
    f.attach("TST-6");
    CHECK(rules::energy_units(f.game, f.active()) == 1);
    CHECK(f.pays({E::Colorless}));
    CHECK_FALSE(f.pays({E::Water}));

    f.evolve_active_to("TST-2");
    f.evolve_active_to("TST-3"); // Stage 2: two units of any type
    CHECK(rules::energy_units(f.game, f.active()) == 2);
    CHECK(f.pays({E::Water, E::Fire}));
    CHECK(f.pays({E::Water, E::Colorless}));
    CHECK_FALSE(f.pays({E::Water, E::Water, E::Colorless})); // only 2 Energy at a time
}

TEST_CASE("one card can provide several Energy", "[energy]") {
    using E = EnergyType;
    Fixture f;
    f.attach("TST-7");
    CHECK(rules::energy_units(f.game, f.active()) == 1);
    f.evolve_active_to("TST-2");
    CHECK(rules::energy_units(f.game, f.active()) == 3);
    CHECK(f.pays({E::Colorless, E::Colorless, E::Colorless}));
    CHECK_FALSE(f.pays({E::Fire}));
}

TEST_CASE("cost matching finds an assignment a greedy one would miss", "[energy]") {
    using E = EnergyType;
    Fixture f;
    f.evolve_active_to("TST-2");
    f.evolve_active_to("TST-3");
    f.attach("TST-6");                                   // two units of any type, listed first
    f.attach("TST-4");                                   // Fire
    CHECK(f.pays({E::Fire, E::Water, E::Psychic}));      // Fire from Fire, the rest from Upper
    CHECK_FALSE(f.pays({E::Water, E::Water, E::Water})); // Fire can't be Water
    CHECK_FALSE(f.pays({E::Fire, E::Water, E::Psychic, E::Colorless}));
}

TEST_CASE("Retreat Cost is paid in Energy provided, not cards", "[energy]") {
    Fixture f;
    f.evolve_active_to("TST-2");
    f.attach("TST-7"); // one card, three Energy
    REQUIRE(test::offers(pokebot::legal_actions(f.game), ActionKind::Retreat));
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::Retreat));
    CHECK(f.game.player(f.me).bench[0].energy.empty()); // that one card paid all 2
    f.check_cards();
}

TEST_CASE("choosing which Energy to discard stops once the cost is paid", "[energy]") {
    Fixture f;
    f.evolve_active_to("TST-2");
    f.attach("TST-4");
    f.attach("TST-7");
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::Retreat));
    REQUIRE(f.game.decision().kind == DecisionKind::RetreatDiscard);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::DiscardEnergy, "TST-7"));
    CHECK(f.game.decision().kind == DecisionKind::MainPhase); // Ignite alone paid the 2
    CHECK(f.game.player(f.me).bench[0].energy.size() == 1);
    f.check_cards();
}

TEST_CASE("an Energy that's discarded at the end of your turn", "[energy]") {
    Fixture f;
    f.attach("TST-7");
    const auto ignite = f.active().energy.back();
    test::end_turns(f.game, 1);
    CHECK(f.active().energy.empty());
    CHECK(f.game.player(f.me).discard.contains(ignite));
    f.check_cards();
}

TEST_CASE("conditional Energy is checked when the cards load", "[energy]") {
    CHECK_THROWS_AS(pokebot::compile_provides_when(json::parse(R"([{"provides": ["rainbow"]}])")), pokebot::CardDataError);
    CHECK_THROWS_AS(pokebot::compile_provides_when(json::parse(R"({"provides": ["any"]})")), pokebot::CardDataError);
    const auto when = pokebot::compile_provides_when(json::parse(R"([{"holder": {"stage": "stage2"}, "provides": ["any", "water"]}])"));
    REQUIRE(when.size() == 1);
    CHECK(when[0].provides[0] == pokebot::kAnyEnergy);
    CHECK(when[0].provides[1] == pokebot::energy_mask(EnergyType::Water));
}
