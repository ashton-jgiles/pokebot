#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cards/db/card_db.hpp"
#include "cards/effects/program.hpp"
#include "support/test_cards.hpp"

using nlohmann::json;
using pokebot::CardDataError;
using pokebot::compile_attack;
using pokebot::Op;
using pokebot::Zone;

namespace {

    std::vector<Op> ops(const pokebot::Program& program) {
        std::vector<Op> out;
        for (const auto& step : program) {
            out.push_back(step.op);
        }
        return out;
    }

} // namespace

TEST_CASE("an attack with no effects is just its damage", "[effects]") {
    CHECK(ops(compile_attack(json::array())) == std::vector<Op>{Op::Damage});
}

TEST_CASE("search_deck becomes choose, move and shuffle", "[effects]") {
    const auto program = compile_attack(json::parse(R"([{"op": "search_deck", "count": 3, "filter": {"kind": "pokemon"}}])"));
    REQUIRE(ops(program) == std::vector<Op>{Op::Damage, Op::ChooseCards, Op::MoveChosen, Op::ShuffleDeck});
    CHECK(program[1].from == Zone::Deck);
    CHECK(program[1].min == 0);
    CHECK(program[1].max == 3);
    CHECK(program[1].filter.kind == pokebot::CardKind::Pokemon);
    CHECK(program[2].from == Zone::Deck);
    CHECK(program[2].to == Zone::Hand);
}

TEST_CASE("a search can reveal what it finds", "[effects]") {
    const auto revealed = pokebot::compile_trainer(json::parse(R"([{"op": "search_deck", "reveal": true}])"));
    REQUIRE(ops(revealed) == std::vector<Op>{Op::ChooseCards, Op::MoveChosen, Op::ShuffleDeck});
    CHECK(revealed[1].reveal);
    CHECK_FALSE(pokebot::compile_trainer(json::parse(R"([{"op": "search_deck"}])"))[1].reveal);
    CHECK(pokebot::compile_trainer(json::parse(R"([{"op": "search_to_top", "reveal": true}])"))[2].reveal);
    CHECK_THROWS_AS(pokebot::compile_trainer(json::parse(R"([{"op": "search_deck", "reveal": "yes"}])")), CardDataError);
}

TEST_CASE("may asks first and jumps past its effects on no", "[effects]") {
    const auto program = compile_attack(json::parse(R"([{"op": "may", "effects": [{"op": "search_deck"}]}, {"op": "draw"}])"));
    REQUIRE(ops(program) == std::vector<Op>{Op::Damage, Op::AskYesNo, Op::ChooseCards, Op::MoveChosen, Op::ShuffleDeck, Op::Draw});
    CHECK(program[1].jump == 5);
}

TEST_CASE("a damage step in the list keeps its place", "[effects]") {
    const auto program = compile_attack(json::parse(R"([{"op": "draw", "count": 2}, {"op": "damage"}])"));
    REQUIRE(ops(program) == std::vector<Op>{Op::Draw, Op::Damage});
    CHECK(program[0].max == 2);
}

TEST_CASE("mistakes in an effects list are rejected", "[effects]") {
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "teleport"}])")), CardDataError);
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"count": 1}])")), CardDataError);
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "search_deck", "to": "moon"}])")), CardDataError);
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "search_deck", "filter": {"colour": "red"}}])")), CardDataError);
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "draw", "count": 99}])")), CardDataError);
    CHECK_THROWS_AS(compile_attack(json::parse(R"({"op": "draw"})")), CardDataError);
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "draw", "count": "three"}])")), CardDataError);
}

TEST_CASE("filters match kind, type, Basic and name", "[effects]") {
    using pokebot::CardFilter;
    using E = pokebot::EnergyType;
    auto dark = test::basic_pokemon("TST-1", "Darkmon");
    dark.types = {E::Darkness};
    const auto stage1 = test::evolution("TST-2", "Evomon", pokebot::Stage::Stage1, "Darkmon");
    const auto energy = test::basic_energy("TST-3", "Darkness Energy", E::Darkness);

    CardFilter pokemon;
    pokemon.kind = pokebot::CardKind::Pokemon;
    CHECK(matches(pokemon, dark));
    CHECK_FALSE(matches(pokemon, energy));

    CardFilter darkness;
    darkness.type = E::Darkness;
    CHECK(matches(darkness, dark));
    CHECK(matches(darkness, energy));
    CHECK_FALSE(matches(darkness, stage1));

    CardFilter basic;
    basic.basic = true;
    CHECK(matches(basic, dark));
    CHECK(matches(basic, energy));
    CHECK_FALSE(matches(basic, stage1));

    CardFilter named;
    named.name = "Evomon";
    CHECK(matches(named, stage1));
    CHECK_FALSE(matches(named, dark));
    CHECK(matches(CardFilter{}, energy));
}

TEST_CASE("adding a card compiles its attacks and names the attack on a mistake", "[effects]") {
    auto card = test::basic_pokemon("TST-1", "Effectmon");
    card.attacks = {test::attack("Collect", {}, 0)};
    card.attacks[0].effects = json::parse(R"([{"op": "draw"}])");
    pokebot::CardDB db;
    db.add(card);
    CHECK(ops(db.at("TST-1").attacks[0].program) == std::vector<Op>{Op::Damage, Op::Draw});

    card.attacks[0].effects = json::parse(R"([{"op": "teleport"}])");
    try {
        db.add(card);
        FAIL("a bad effect was accepted");
    } catch (const CardDataError& e) {
        CHECK(std::string(e.what()).find("Collect") != std::string::npos);
    }
}

TEST_CASE("if jumps to else on false, and past it after then", "[effects]") {
    const auto program = pokebot::compile_trainer(json::parse(
        R"([{"op": "if", "condition": {"prizes_exactly": 6}, "then": [{"op": "draw", "count": 8}], "else": [{"op": "draw", "count": 6}]}])"));
    REQUIRE(ops(program) == std::vector<Op>{Op::JumpUnless, Op::Draw, Op::Jump, Op::Draw});
    CHECK(program[0].jump == 3);
    CHECK(program[0].condition.kind == pokebot::ConditionKind::PrizesExactly);
    CHECK(program[0].condition.value == 6);
    CHECK(program[2].jump == 4);

    const auto no_else = pokebot::compile_trainer(json::parse(R"([{"op": "if", "condition": {"has_benched": true}, "then": [{"op": "draw"}]}])"));
    REQUIRE(ops(no_else) == std::vector<Op>{Op::JumpUnless, Op::Draw});
    CHECK(no_else[0].jump == 2);
}

TEST_CASE("Trainer effects have no damage step and can name a player", "[effects]") {
    const auto program = pokebot::compile_trainer(json::parse(R"([{"op": "draw", "count": 2, "player": "opponent"}])"));
    REQUIRE(ops(program) == std::vector<Op>{Op::Draw});
    CHECK(program[0].side == pokebot::Side::Opponent);
    CHECK_THROWS_AS(pokebot::compile_trainer(json::parse(R"([{"op": "damage"}])")), CardDataError);
    CHECK_THROWS_AS(pokebot::compile_trainer(json::parse(R"([{"op": "draw", "player": "everyone"}])")), CardDataError);
    CHECK_THROWS_AS(pokebot::compile_trainer(json::parse(R"([{"op": "switch_active", "player": "each"}])")), CardDataError);
}

TEST_CASE("requirements compile to conditions and reject mistakes", "[effects]") {
    const auto conditions = pokebot::compile_conditions(json::parse(R"([{"hand_besides": 2}, {"discard_has": {"kind": "energy"}}])"));
    REQUIRE(conditions.size() == 2);
    CHECK(conditions[0].kind == pokebot::ConditionKind::HandBesides);
    CHECK(conditions[0].value == 2);
    CHECK(conditions[1].kind == pokebot::ConditionKind::DiscardHas);
    CHECK(conditions[1].filter.kind == pokebot::CardKind::Energy);
    CHECK_THROWS_AS(pokebot::compile_conditions(json::parse(R"([{"moon_is_full": true}])")), CardDataError);
    CHECK_THROWS_AS(pokebot::compile_conditions(json::parse(R"([{"hand_besides": 2, "has_benched": true}])")), CardDataError);
    CHECK_THROWS_AS(pokebot::compile_conditions(json::parse(R"({"hand_besides": 2})")), CardDataError);
}

TEST_CASE("filters match Trainer kind, stage, Rule Box, HP and any of several", "[effects]") {
    using pokebot::CardFilter;
    auto small = test::basic_pokemon("TST-1", "Smallmon", 70);
    auto big = test::basic_pokemon("TST-2", "Bigmon ex", 200);
    big.rules.ex = true;
    const auto stage2 = test::evolution("TST-3", "Topmon", pokebot::Stage::Stage2, "Midmon");
    pokebot::CardDef item;
    item.kind = pokebot::CardKind::Trainer;
    item.trainer_kind = pokebot::TrainerKind::Item;
    const auto energy = test::basic_energy("TST-4", "Fire Energy", pokebot::EnergyType::Fire);

    CardFilter items;
    items.trainer = pokebot::TrainerKind::Item;
    CHECK(matches(items, item));
    CHECK_FALSE(matches(items, small));

    CardFilter evolution;
    evolution.stage = pokebot::StageFilter::Evolution;
    CHECK(matches(evolution, stage2));
    CHECK_FALSE(matches(evolution, small));

    CardFilter no_rule_box;
    no_rule_box.rule_box = false;
    CHECK(matches(no_rule_box, small));
    CHECK_FALSE(matches(no_rule_box, big));

    CardFilter up_to_70;
    up_to_70.max_hp = 70;
    CHECK(matches(up_to_70, small));
    CHECK_FALSE(matches(up_to_70, big));
    CHECK_FALSE(matches(up_to_70, energy));

    CardFilter pokemon_or_energy;
    pokemon_or_energy.any_of.resize(2);
    pokemon_or_energy.any_of[0].kind = pokebot::CardKind::Pokemon;
    pokemon_or_energy.any_of[1].kind = pokebot::CardKind::Energy;
    CHECK(matches(pokemon_or_energy, small));
    CHECK(matches(pokemon_or_energy, energy));
    CHECK_FALSE(matches(pokemon_or_energy, item));
}

TEST_CASE("healing, damage to itself, every target, the Active, and Weakness or Resistance ignored", "[effects]") {
    const auto heal = pokebot::compile_trainer(json::parse(R"([{"op": "heal_to", "amount": 30}, {"op": "heal_self", "all": true}])"));
    REQUIRE(ops(heal) == std::vector<Op>{Op::ChooseTargets, Op::HealTargets, Op::HealSelf});
    CHECK(heal[0].side == pokebot::Side::You);
    CHECK(heal[0].needs_damage);
    CHECK(heal[1].amount == 30);
    CHECK(heal[2].amount > 990);
    const auto each = compile_attack(json::parse(R"([{"op": "damage_to", "each": true, "on": "bench", "amount": 20}, {"op": "damage_self", "amount": 30}])"));
    REQUIRE(ops(each) == std::vector<Op>{Op::Damage, Op::ChooseTargets, Op::DamageTargets, Op::CountersOnSelf});
    CHECK(each[1].each);
    CHECK(each[1].from == Zone::Bench);
    CHECK(each[3].max == 3);
    const auto active = compile_attack(json::parse(R"([{"op": "discard_target_energy", "on": "active"}])"));
    CHECK(active[1].from == Zone::Active);
    CHECK(active[1].each);
    CHECK(ops(compile_attack(json::parse(R"([{"op": "ignore_weakness"}, {"op": "ignore_resistance"}, {"op": "damage"}])"))) ==
          std::vector<Op>{Op::IgnoreWeakness, Op::IgnoreResistance, Op::Damage});
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "ignore_resistance"}])")), CardDataError);
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "heal_self", "amount": 25}])")), CardDataError);
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "heal_self", "amount": 30, "all": true}])")), CardDataError);
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "damage_to", "each": true, "targets": 2, "amount": 20}])")), CardDataError);
    CHECK_THROWS_AS(pokebot::compile_trainer(json::parse(R"([{"op": "damage_self", "amount": 30}])")), CardDataError);
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "damage_to", "on": "moon", "amount": 20}])")), CardDataError);
}

TEST_CASE("conditions and counts about the opponent's Active Pokemon and this Pokemon", "[effects]") {
    const auto conditions = pokebot::compile_conditions(json::parse(R"([{"opponent_active_is": {"rule_box": true}}, {"opponent_active_damaged": true},
                                                                        {"opponent_active_has": "poisoned"}, {"self_damaged": true}, {"self_has_tool": true}])"));
    REQUIRE(conditions.size() == 5);
    CHECK(conditions[0].kind == pokebot::ConditionKind::OpponentActiveIs);
    CHECK(conditions[0].filter.rule_box == true);
    CHECK(conditions[1].kind == pokebot::ConditionKind::OpponentActiveDamaged);
    CHECK(conditions[2].kind == pokebot::ConditionKind::OpponentActiveHas);
    CHECK(conditions[2].special == pokebot::SpecialCondition::Poisoned);
    CHECK(conditions[3].kind == pokebot::ConditionKind::SelfDamaged);
    CHECK(conditions[4].kind == pokebot::ConditionKind::SelfHasTool);
    CHECK_THROWS_AS(pokebot::compile_conditions(json::parse(R"([{"opponent_active_has": "sleepy"}])")), CardDataError);
    for (const char* of : {"active_damage_counters", "in_play", "tools_in_play", "energy_in_play", "self_energy"}) {
        const json effects = {{{"op", "set_damage"}, {"per", 10}, {"for_each", {{"of", of}}}}, {{"op", "damage"}}};
        CHECK(compile_attack(effects)[0].count.kind != pokebot::CountKind::None);
    }
}

TEST_CASE("special_condition targets the opponent unless told otherwise", "[effects]") {
    const auto program = compile_attack(json::parse(
        R"([{"op": "special_condition", "condition": "asleep"}, {"op": "special_condition", "condition": "poisoned", "count": 4, "player": "you"}])"));
    REQUIRE(ops(program) == std::vector<Op>{Op::Damage, Op::ApplySpecialCondition, Op::ApplySpecialCondition});
    CHECK(program[1].special == pokebot::SpecialCondition::Asleep);
    CHECK(program[1].side == pokebot::Side::Opponent);
    CHECK(program[2].special == pokebot::SpecialCondition::Poisoned);
    CHECK(program[2].max == 4);
    CHECK(program[2].side == pokebot::Side::You);
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "special_condition", "condition": "sleepy"}])")), CardDataError);
    CHECK_THROWS_AS(compile_attack(json::parse(R"([{"op": "special_condition"}])")), CardDataError);
}
