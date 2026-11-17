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
