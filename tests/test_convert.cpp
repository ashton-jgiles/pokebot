#include <catch2/catch_test_macros.hpp>

#include "convert.hpp"

using nlohmann::json;
using pokebot::CardDataError;
using pokebot::CardKind;
using pokebot::DamageMod;
using pokebot::EnergyType;
using pokebot::import::convert_card;

TEST_CASE("converts a Pokemon ex with modifier damage and a free attack", "[convert]") {
    const json t = json::parse(R"({
        "id": "xx1-001", "name": "Testmon ex", "category": "Pokemon", "stage": "Stage1",
        "evolveFrom": "Testling", "hp": 230, "types": ["Darkness"], "suffix": "ex", "retreat": 2,
        "weaknesses": [{"type": "Grass", "value": "×2"}],
        "resistances": [{"type": "Fighting", "value": "-30"}],
        "abilities": [{"type": "Ability", "name": "Peek", "effect": "Look at a card."}],
        "attacks": [
            {"name": "Free Hit", "damage": 10},
            {"name": "Multi", "cost": ["Darkness", "Colorless"], "damage": "20×",
             "effect": "This attack does 20 damage for each ..."}
        ]})");

    const auto result = convert_card(t, "TST-1");
    const auto& c = result.card;
    CHECK(c.id == "TST-1");
    CHECK(c.tcgdex_id == "xx1-001");
    CHECK(c.kind == CardKind::Pokemon);
    CHECK(c.hp == 230);
    CHECK(c.evolves_from == "Testling");
    CHECK(c.types == std::vector{EnergyType::Darkness});
    REQUIRE(c.weakness);
    CHECK(c.weakness->type == EnergyType::Grass);
    CHECK(c.weakness->multiplier == 2);
    REQUIRE(c.resistance);
    CHECK(c.resistance->amount == 30);
    REQUIRE(c.abilities.size() == 1);
    CHECK(c.abilities[0].text == "Look at a card.");
    REQUIRE(c.attacks.size() == 2);
    CHECK(c.attacks[0].cost.empty());
    CHECK(c.attacks[1].damage == 20);
    CHECK(c.attacks[1].damage_mod == DamageMod::Times);
    CHECK(c.rules.ex);
    CHECK(c.prize_value() == 2);
    CHECK(result.notes.size() == 1); // the Tera reminder
}

TEST_CASE("Times damage survives a save and load", "[convert]") {
    // Guards against the enum and its name table drifting out of order.
    const json t = json::parse(R"({"id": "x", "name": "Mon", "category": "Pokemon", "stage": "Basic",
        "hp": 60, "types": ["Fire"],
        "attacks": [{"name": "A", "damage": "30+"}, {"name": "B", "damage": "20×"}]})");
    const auto reloaded = json(convert_card(t, "TST-9").card).get<pokebot::CardDef>();
    CHECK(reloaded.attacks[0].damage_mod == DamageMod::Plus);
    CHECK(reloaded.attacks[1].damage_mod == DamageMod::Times);
}

TEST_CASE("detects ACE SPEC Trainers", "[convert]") {
    const json t = json::parse(R"({"id": "xx1-002", "name": "Test Box", "category": "Trainer",
        "trainerType": "Item", "rarity": "ACE SPEC Rare", "effect": "Search your deck..."})");
    const auto c = convert_card(t, "TST-2").card;
    CHECK(c.kind == CardKind::Trainer);
    CHECK(c.rules.ace_spec);
    CHECK(c.text == "Search your deck...");
}

TEST_CASE("takes basic Energy's type from its name", "[convert]") {
    const json t = json::parse(R"({"id": "mee-007", "name": "Darkness Energy",
        "category": "Energy", "energyType": "Normal"})");
    const auto c = convert_card(t, "MEE-7").card;
    CHECK(c.kind == CardKind::Energy);
    CHECK(c.provides == std::vector{EnergyType::Darkness});
}

TEST_CASE("treats Energy with rules text as Special even if TCGdex says Normal", "[convert]") {
    // TCGdex lists Ignition Energy (WHT 86) as "Normal".
    const json t = json::parse(R"({"id": "x", "name": "Ignition Energy", "category": "Energy",
        "energyType": "Normal", "effect": "If this card is attached to 1 of your Pokemon, ..."})");
    const auto result = convert_card(t, "WHT-86");
    CHECK(result.card.energy_kind == pokebot::EnergyKind::Special);
    CHECK(result.card.provides.empty());
    CHECK(result.card.text.starts_with("If this card"));
    CHECK(result.notes.size() == 2); // mislabeled + fill in what it provides
}

TEST_CASE("rejects data it doesn't understand", "[convert]") {
    const json vmax = json::parse(R"({"id": "x", "name": "Big", "category": "Pokemon",
        "stage": "VMAX", "hp": 320, "types": ["Fire"]})");
    CHECK_THROWS_AS(convert_card(vmax, "TST-3"), CardDataError);

    const json bad_damage = json::parse(R"({"id": "x", "name": "Odd", "category": "Pokemon",
        "stage": "Basic", "hp": 60, "types": ["Fire"], "attacks": [{"name": "A", "damage": "lots"}]})");
    CHECK_THROWS_AS(convert_card(bad_damage, "TST-4"), CardDataError);
}
