#include <catch2/catch_test_macros.hpp>

#include "cards/card_db.hpp"

using nlohmann::json;
using pokebot::CardDataError;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::merge_effects;

namespace {

    json sample_pokemon() {
        return json::parse(R"({
            "id": "TST-1", "name": "Testmon ex", "kind": "pokemon", "rules": ["ex"],
            "stage": "basic", "hp": 200, "types": ["psychic"], "weakness": null, "resistance": null,
            "retreat": 1, "abilities": [],
            "attacks": [{"name": "Zap", "cost": ["psychic"], "damage": 30, "damage_mod": "none",
                         "text": "Put 2 damage counters on 1 of your opponent's Pokemon.",
                         "effects": []}]})");
    }

} // namespace

TEST_CASE("card JSON round-trips through CardDef", "[card_db]") {
    const auto card = sample_pokemon().get<CardDef>();
    CHECK(json(card).get<CardDef>().attacks[0].damage == 30);
    CHECK(card.prize_value() == 2);
}

TEST_CASE("effects files add rules and attack effects", "[card_db]") {
    const json overrides = json::parse(R"({
        "rules": ["ex", "tera"],
        "attacks": {"Zap": {"effects": [{"op": "place_damage_counters", "count": 2}]}}})");
    const auto card = merge_effects(sample_pokemon(), overrides).get<CardDef>();
    CHECK(card.rules.tera);
    CHECK(card.attacks[0].effects.size() == 1);
    CHECK(card.attacks[0].text.starts_with("Put 2")); // generated fields are kept
}

TEST_CASE("effects files can't name attacks that don't exist", "[card_db]") {
    const json typo = json::parse(R"({"attacks": {"Zapp": {"effects": []}}})");
    CHECK_THROWS_AS(merge_effects(sample_pokemon(), typo), CardDataError);
}

TEST_CASE("lists everything that still needs effects", "[card_db]") {
    CardDB db;
    db.add(sample_pokemon().get<CardDef>());
    const auto missing = db.missing_effects();
    REQUIRE(missing.size() == 1);
    CHECK(missing[0] == "TST-1 Testmon ex: attack Zap");
}

TEST_CASE("bad card data is reported, not silently accepted", "[card_db]") {
    auto bad = sample_pokemon();
    bad["kind"] = "pokemonn";
    CHECK_THROWS_AS(bad.get<CardDef>(), CardDataError);

    auto wrong_type = sample_pokemon();
    wrong_type["hp"] = "200";
    CHECK_THROWS_AS(wrong_type.get<CardDef>(), CardDataError);
}
