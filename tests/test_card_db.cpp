#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <system_error>

#include <catch2/catch_test_macros.hpp>

#include "cards/db/card_db.hpp"

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

TEST_CASE("an Ability marked no_effect isn't missing, and the reason is kept", "[card_db]") {
    auto pokemon = sample_pokemon();
    pokemon["abilities"] = json::parse(R"([{"name": "Damp", "text": "Does nothing here.", "no_effect": "nothing it stops is in our pool"}])");
    pokemon["attacks"][0]["effects"] = json::parse(R"([{"op": "draw", "count": 1}])");
    const auto card = pokemon.get<CardDef>();
    CHECK(card.abilities[0].no_effect == "nothing it stops is in our pool");
    const json saved = card;
    CHECK(saved.get<CardDef>().abilities[0].no_effect == card.abilities[0].no_effect);

    CardDB db;
    db.add(card);
    CHECK(db.missing_effects().empty());
}

TEST_CASE("bad card data is reported, not silently accepted", "[card_db]") {
    auto bad = sample_pokemon();
    bad["kind"] = "pokemonn";
    CHECK_THROWS_AS(bad.get<CardDef>(), CardDataError);

    auto wrong_type = sample_pokemon();
    wrong_type["hp"] = "200";
    CHECK_THROWS_AS(wrong_type.get<CardDef>(), CardDataError);
}

TEST_CASE("a card's same_as uses another card's effects file, unless it has its own", "[card_db]") {
    std::random_device rd;
    const auto root = std::filesystem::temp_directory_path() / ("pokebot-same-as-" + std::to_string(rd()));
    const auto generated = root / "generated";
    const auto effects = root / "effects";
    std::filesystem::create_directories(generated);
    std::filesystem::create_directories(effects);
    json reprint = sample_pokemon();
    reprint["id"] = "OLD-1";
    reprint["same_as"] = "TST-1";
    json own = sample_pokemon();
    own["id"] = "OWN-1";
    own["same_as"] = "TST-1";
    std::ofstream(generated / "TST-1.json") << sample_pokemon().dump();
    std::ofstream(generated / "OLD-1.json") << reprint.dump();
    std::ofstream(generated / "OWN-1.json") << own.dump();
    std::ofstream(effects / "TST-1.json") << R"({"attacks": {"Zap": {"effects": [{"op": "draw"}]}}})";
    std::ofstream(effects / "OWN-1.json") << R"({"attacks": {"Zap": {"effects": [{"op": "draw"}, {"op": "draw"}]}}})";
    {
        const CardDB db = CardDB::load(generated, effects);
        CHECK(db.at("OLD-1").attacks[0].effects.size() == 1);
        CHECK(db.at("OWN-1").attacks[0].effects.size() == 2);
    }
    std::filesystem::remove(effects / "TST-1.json");
    CHECK_THROWS_AS(CardDB::load(generated, effects), CardDataError);
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}

TEST_CASE("a card's stored translation is used only when it has no effects file", "[card_db]") {
    std::random_device rd;
    const auto root = std::filesystem::temp_directory_path() / ("pokebot-translation-" + std::to_string(rd()));
    const auto generated = root / "generated";
    const auto effects = root / "effects";
    std::filesystem::create_directories(generated);
    std::filesystem::create_directories(effects);
    json translated = sample_pokemon();
    translated["id"] = "TRN-1";
    translated["translation"] = json::parse(R"({"attacks": {"Zap": {"effects": [{"op": "draw"}]}}})");
    json written = translated;
    written["id"] = "OWN-1";
    std::ofstream(generated / "TRN-1.json") << translated.dump();
    std::ofstream(generated / "OWN-1.json") << written.dump();
    std::ofstream(effects / "OWN-1.json") << R"({"attacks": {"Zap": {"effects": [{"op": "draw"}, {"op": "draw"}]}}})";
    {
        const CardDB db = CardDB::load(generated, effects);
        CHECK(db.at("TRN-1").attacks[0].effects.size() == 1);
        CHECK(db.at("OWN-1").attacks[0].effects.size() == 2);
    }
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}
