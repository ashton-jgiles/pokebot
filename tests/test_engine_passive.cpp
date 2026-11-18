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
using pokebot::EnergyType;
using pokebot::GameState;
using pokebot::kActiveSlot;
using pokebot::TrainerKind;
namespace rules = pokebot::rules;

namespace {

    CardDef trainer(const char* id, const char* name, TrainerKind kind, const char* passive) {
        CardDef c;
        c.id = id;
        c.name = name;
        c.kind = pokebot::CardKind::Trainer;
        c.trainer_kind = kind;
        c.text = "test card";
        c.passive = json::parse(passive);
        return c;
    }

    pokebot::Ability passive_ability(const char* name, const char* passive) {
        pokebot::Ability a;
        a.name = name;
        a.text = "test ability";
        a.passive = json::parse(passive);
        return a;
    }

    // TST-1 Psymon (Psychic, 200 HP): Hit [C] 30, Shred [C] 30 (not affected by effects on the Defending Pokemon).
    // TST-2 Dragonmon (Dragon, 100 HP, Retreat 2, no Weakness). TST-3 N's Mon (100 HP, Retreat 3).
    // TST-4 Fairymon: opponent's Dragon Pokemon have Weakness Psychic. TST-5 Skymon: your Basic Pokemon have no Retreat Cost.
    // TST-6 Evomon (Stage 1 from Dragonmon, Retreat 2). TST-7 Energy.
    // Trainers: TST-8 Balloon (-2 Retreat), TST-9 Cape (+100 HP), TST-10 Mochi (+40 while Poisoned), TST-11 Castle (N's: no Retreat).
    CardDB sample_db() {
        using E = EnergyType;
        CardDef psy = test::basic_pokemon("TST-1", "Psymon", 200);
        psy.types = {E::Psychic};
        psy.attacks = {test::attack("Hit", {E::Colorless}, 30), test::attack("Shred", {E::Colorless}, 30)};
        psy.attacks[1].effects = json::parse(R"([{"op": "ignore_defender_effects"}, {"op": "damage"}])");

        CardDef dragon = test::basic_pokemon("TST-2", "Dragonmon", 100, 2);
        dragon.types = {E::Dragon};
        CardDef fairy = test::basic_pokemon("TST-4", "Fairymon", 100);
        fairy.abilities = {passive_ability("Fairy Zone", R"([{"modify": "weakness_type", "type": "psychic", "applies_to": "opponents",
                                                              "filter": {"kind": "pokemon", "type": "dragon"}}])")};
        CardDef sky = test::basic_pokemon("TST-5", "Skymon", 100);
        sky.abilities = {passive_ability("Skyliner", R"([{"modify": "retreat_cost", "set": 0, "applies_to": "yours", "filter": {"stage": "basic"}}])")};
        CardDef evo = test::evolution("TST-6", "Evomon", pokebot::Stage::Stage1, "Dragonmon", 120, 2);
        evo.types = {E::Dragon};

        CardDB db;
        for (const auto& card : {psy, dragon, fairy, sky, evo}) {
            db.add(card);
        }
        db.add(test::basic_pokemon("TST-3", "N's Mon", 100, 3));
        db.add(test::basic_energy("TST-7", "Psychic Energy", E::Psychic));
        db.add(trainer("TST-8", "Balloon", TrainerKind::Tool, R"([{"modify": "retreat_cost", "add": -2}])"));
        db.add(trainer("TST-9", "Cape", TrainerKind::Tool, R"([{"modify": "max_hp", "add": 100}])"));
        db.add(trainer("TST-10", "Mochi", TrainerKind::Tool, R"([{"modify": "damage_dealt", "add": 40, "if_poisoned": true}])"));
        db.add(trainer("TST-11", "Castle", TrainerKind::Stadium,
                       R"([{"modify": "retreat_cost", "set": 0, "applies_to": "all", "filter": {"name_prefix": "N's"}}])"));
        return db;
    }

    const pokebot::Decklist kDeck =
        test::deck({{4, 1}, {4, 2}, {4, 3}, {4, 4}, {4, 5}, {4, 6}, {20, 7}, {4, 8}, {4, 9}, {4, 10}, {4, 11}});

    Action attack(std::uint8_t index) {
        return Action{ActionKind::Attack, pokebot::kNoCard, kActiveSlot, index};
    }

    // Turn 2: `me` has Psymon Active with one Energy; `them` has Dragonmon Active and N's Mon on the Bench.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 18);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            test::set_active(game, me, "TST-1");
            test::clear_bench(game, me);
            test::attach_from_deck(game, me, kActiveSlot, "TST-7");
            test::set_active(game, them, "TST-2");
            test::clear_bench(game, them);
            test::add_to_bench(game, them, "TST-3");
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        pokebot::PokemonInPlay& active(int player) { return *game.player(player).active; }

        void give_tool(int player, const char* id) { active(player).tool = test::take_from_deck(game, player, id); }

        void check_cards() const {
            CHECK(test::cards_accounted_for(game.player(0)) == 60);
            CHECK(test::cards_accounted_for(game.player(1)) == 60);
            CHECK(pokebot::check_invariants(game).empty());
        }
    };

} // namespace

TEST_CASE("a Tool changes only the Pokemon it's attached to", "[passive]") {
    Fixture f;
    f.give_tool(f.them, "TST-9");
    CHECK(rules::max_hp(f.game, f.active(f.them)) == 200);
    CHECK(rules::max_hp(f.game, f.game.player(f.them).bench[0]) == 100);

    Fixture g;
    g.give_tool(g.them, "TST-8");
    CHECK(rules::retreat_cost(g.game, g.active(g.them)) == 0); // 2 - 2
    g.check_cards();
}

TEST_CASE("a Stadium can change both players' Pokemon that match", "[passive]") {
    Fixture f;
    auto& n_mon = f.game.player(f.them).bench[0];
    CHECK(rules::retreat_cost(f.game, n_mon) == 3);
    f.game.player(f.me).stadium = test::take_from_deck(f.game, f.me, "TST-11");
    CHECK(rules::retreat_cost(f.game, n_mon) == 0);            // the opponent's N's Pokemon too
    CHECK(rules::retreat_cost(f.game, f.active(f.them)) == 2); // not an N's Pokemon
    f.check_cards();
}

TEST_CASE("a passive Ability changes your Basic Pokemon, not Stage 1 or your opponent's", "[passive]") {
    Fixture f;
    test::add_to_bench(f.game, f.me, "TST-5"); // Skymon
    test::add_to_bench(f.game, f.me, "TST-2"); // a Basic Dragonmon of mine
    auto& mine = f.game.player(f.me).bench[1];
    CHECK(rules::retreat_cost(f.game, mine) == 0);
    CHECK(rules::retreat_cost(f.game, f.active(f.them)) == 2); // the opponent's Basic

    mine.stack.push_back(test::take_from_deck(f.game, f.me, "TST-6")); // evolve it: Stage 1
    CHECK(rules::retreat_cost(f.game, mine) == 2);
}

TEST_CASE("Fairy Zone gives the opponent's Dragon Pokemon Weakness to Psychic, and Shred ignores it", "[passive]") {
    {
        Fixture f;
        CHECK_FALSE(rules::weakness(f.game, f.active(f.them)).has_value());
        test::add_to_bench(f.game, f.me, "TST-4");
        const auto w = rules::weakness(f.game, f.active(f.them));
        REQUIRE(w.has_value());
        CHECK(w->type == EnergyType::Psychic);
        CHECK(w->multiplier == 2);

        pokebot::apply(f.game, attack(0));
        CHECK(f.active(f.them).damage_counters == 6); // 30 x 2
        f.check_cards();
    }
    {
        Fixture f;
        test::add_to_bench(f.game, f.me, "TST-4");
        pokebot::apply(f.game, attack(1));
        CHECK(f.active(f.them).damage_counters == 3); // Shred: no Weakness from Fairy Zone
    }
}

TEST_CASE("a Tool's damage bonus needs its condition", "[passive]") {
    Fixture f;
    f.give_tool(f.me, "TST-10");
    pokebot::apply(f.game, attack(0));
    CHECK(f.active(f.them).damage_counters == 3); // not Poisoned: no bonus

    Fixture g;
    g.give_tool(g.me, "TST-10");
    pokebot::detail::apply_special_condition(g.game, g.active(g.me), pokebot::SpecialCondition::Poisoned, 1);
    pokebot::apply(g.game, attack(0));
    CHECK(g.active(g.them).damage_counters == 7); // 30 + 40
}

TEST_CASE("passive effects count as written effects", "[passive]") {
    const CardDB db = sample_db();
    for (const auto& missing : db.missing_effects()) {
        CHECK(missing.find("Balloon") == std::string::npos);
        CHECK(missing.find("Fairy Zone") == std::string::npos);
    }
}

TEST_CASE("passive effects are checked when the cards load", "[passive]") {
    using pokebot::CardDataError;
    using pokebot::compile_passive;
    CHECK_THROWS_AS(compile_passive(json::parse(R"([{"modify": "luck", "add": 1}])")), CardDataError);
    CHECK_THROWS_AS(compile_passive(json::parse(R"([{"modify": "max_hp"}])")), CardDataError);
    CHECK_THROWS_AS(compile_passive(json::parse(R"([{"modify": "weakness_type"}])")), CardDataError);
    CHECK_THROWS_AS(compile_passive(json::parse(R"([{"modify": "max_hp", "add": 10, "applies_to": "everyone"}])")), CardDataError);
    CHECK_THROWS_AS(pokebot::compile_attack(json::parse(R"([{"op": "ignore_defender_effects"}])")), CardDataError); // must place damage
}
