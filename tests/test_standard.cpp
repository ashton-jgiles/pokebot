#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cards/db/card_db.hpp"
#include "standard.hpp"

using nlohmann::json;
using pokebot::import::card_signature;
using pokebot::import::Coverage;
using pokebot::import::expand_energy_symbols;
using pokebot::import::link_reprints;
using pokebot::import::printing_id;
using pokebot::import::standard_coverage;
using pokebot::import::StandardCard;
using pokebot::import::StoredTranslations;
using pokebot::import::translate_generated;

namespace {

    struct TempDir {
        std::filesystem::path path;
        TempDir() {
            std::random_device rd;
            path = std::filesystem::temp_directory_path() / ("pokebot-standard-" + std::to_string(rd()));
            std::filesystem::create_directories(path);
        }
        ~TempDir() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
        TempDir(const TempDir&) = delete;
        TempDir& operator=(const TempDir&) = delete;
    };

    json pokemon(const std::string& local_id, const std::string& name, const std::string& stage, int hp, const std::string& text, bool legal = true) {
        json card = {{"id", "xx-" + local_id},
                     {"localId", local_id},
                     {"name", name},
                     {"category", "Pokemon"},
                     {"stage", stage},
                     {"hp", hp},
                     {"types", {"Fire"}},
                     {"legal", {{"standard", legal}, {"expanded", true}}},
                     {"attacks", {{{"name", "Hit"}, {"cost", {"Fire"}}, {"damage", 30}, {"effect", text}}}}};
        return card;
    }

    const json kCorrections = json::parse(R"({"cards": {}, "evolves_from": {}})");

    const StandardCard* named(const Coverage& coverage, const std::string& name) {
        for (const StandardCard& entry : coverage.cards) {
            if (entry.card.name == name) {
                return &entry;
            }
        }
        return nullptr;
    }

} // namespace

TEST_CASE("printing ids are the set code and the number, without padding", "[standard]") {
    CHECK(printing_id("TWM", "130") == "TWM-130");
    CHECK(printing_id("MEE", "007") == "MEE-7");
    CHECK(printing_id("30C", "B").empty());
    CHECK(printing_id("TWM", "0").empty());
}

TEST_CASE("reprints with the same rules are one card, whatever their tags and spacing", "[standard]") {
    TempDir dir;
    json set = json::array();
    set.push_back(pokemon("001", "Testmon", "Basic", 70, "Draw a card."));
    set.push_back(pokemon("101", "Testmon", "Basic", 70, "Draw  a <em>card</em>."));
    set.push_back(pokemon("002", "Testmon", "Basic", 80, "Draw a card."));
    set.push_back(pokemon("003", "Oldmon", "Basic", 60, "", false));
    set.push_back(pokemon("C", "Promomon", "Basic", 60, ""));
    const Coverage coverage = standard_coverage({{"TST", set}}, kCorrections, dir.path);
    CHECK(coverage.printings == 5);
    CHECK(coverage.not_legal.at("TST") == 1);
    CHECK(coverage.unnumbered == 1);
    REQUIRE(coverage.cards.size() == 2);
    CHECK(coverage.cards[0].printings == std::vector<std::string>{"TST-1", "TST-101"});
    CHECK(coverage.cards[1].printings == std::vector<std::string>{"TST-2"});
    CHECK(card_signature(coverage.cards[0].card) != card_signature(coverage.cards[1].card));
}

TEST_CASE("a card is complete when any printing has its effects or its text is translated, and missing ones are listed", "[standard]") {
    TempDir dir;
    json set = json::array();
    set.push_back(pokemon("001", "Testmon", "Basic", 70, "Draw a card."));
    set.push_back(pokemon("101", "Testmon", "Basic", 70, "Draw a card."));
    set.push_back(pokemon("002", "Plainmon", "Basic", 90, ""));
    set.push_back(pokemon("003", "Drawmon", "Basic", 90, "Draw 2 cards."));
    set.push_back(pokemon("005", "Oddmon", "Basic", 90, "Draw a card and win the game."));
    json ex = pokemon("004", "Bigmon ex", "Basic", 200, "");
    ex["suffix"] = "ex";
    set.push_back(ex);
    std::ofstream(dir.path / "TST-101.json") << R"({"attacks": {"Hit": {"effects": [{"op": "draw"}]}}})";
    const Coverage coverage = standard_coverage({{"TST", set}}, kCorrections, dir.path);

    const StandardCard* testmon = named(coverage, "Testmon");
    REQUIRE(testmon);
    CHECK(testmon->missing.empty());
    CHECK(testmon->effects_from == "TST-101");
    CHECK(testmon->card.attacks[0].effects.size() == 1);
    REQUIRE(named(coverage, "Plainmon"));
    CHECK(named(coverage, "Plainmon")->missing.empty());
    REQUIRE(named(coverage, "Drawmon"));
    CHECK(named(coverage, "Drawmon")->missing.empty());
    CHECK(named(coverage, "Drawmon")->translated);
    CHECK(named(coverage, "Drawmon")->effects_from.empty());
    CHECK_FALSE(testmon->translated);
    REQUIRE(named(coverage, "Oddmon"));
    CHECK(named(coverage, "Oddmon")->missing == std::vector<std::string>{"TST-5 Oddmon: attack Hit"});
    REQUIRE(named(coverage, "Bigmon ex"));
    CHECK(named(coverage, "Bigmon ex")->tera_unchecked);
    CHECK_FALSE(testmon->tera_unchecked);
}

TEST_CASE("missing and contradictory data is a problem, and corrections fix it", "[standard]") {
    TempDir dir;
    json set = json::array();
    set.push_back(pokemon("001", "Midmon", "Stage1", 90, ""));
    set.push_back(pokemon("002", "Bigmon", "Basic", 300, ""));
    json evolved = pokemon("003", "Bigmon", "Stage2", 310, "");
    evolved["evolveFrom"] = "Midmon";
    set.push_back(evolved);
    set.push_back(json{{"id", "xx-004"}, {"localId", "004"}, {"name", "Blank"}, {"category", "Trainer"}, {"trainerType", "Item"}, {"legal", {{"standard", true}}}});
    set.push_back(json{{"id", "xx-005"}, {"localId", "005"}, {"name", "Odd"}, {"category", "Pokemon"}, {"stage", "VMAX"}, {"hp", 300}, {"types", {"Fire"}}, {"legal", {{"standard", true}}}});
    const Coverage before = standard_coverage({{"TST", set}}, kCorrections, dir.path);
    for (const char* name : {"Midmon", "Bigmon", "Blank", "Odd"}) {
        INFO(name);
        REQUIRE(named(before, name));
        CHECK(named(before, name)->problems.size() == 1);
    }

    const json corrections = json::parse(R"({"cards": {"xx-002": {"stage": "Stage2", "evolveFrom": "Midmon"}},
        "evolves_from": {"Midmon": "Babymon"}})");
    const Coverage after = standard_coverage({{"TST", set}}, corrections, dir.path);
    for (const StandardCard& entry : after.cards) {
        INFO(entry.card.name);
        if (entry.card.name == "Midmon" || entry.card.name == "Bigmon") {
            CHECK(entry.problems.empty());
        }
    }
}

TEST_CASE("decklists' Energy symbols read as the names TCGdex gives", "[standard]") {
    CHECK(expand_energy_symbols("Telepathic {P} Energy") == "Telepathic Psychic Energy");
    CHECK(expand_energy_symbols("Rocky {F} Energy") == "Rocky Fighting Energy");
    CHECK(expand_energy_symbols("Ultra Ball") == "Ultra Ball");
    CHECK(expand_energy_symbols("{X} and {") == "{X} and {");
}

TEST_CASE("a printing without its own effects uses those of a reprint with the same rules", "[standard]") {
    TempDir generated;
    TempDir effects;
    const auto card = [](const std::string& id, int hp) {
        return json{{"id", id}, {"name", "Testmon"}, {"kind", "pokemon"}, {"rules", json::array()}, {"stage", "basic"}, {"hp", hp}, {"types", {"fire"}}, {"weakness", nullptr}, {"resistance", nullptr}, {"retreat", 1}, {"abilities", json::array()}, {"attacks", {{{"name", "Hit"}, {"cost", {"fire"}}, {"damage", 10}, {"damage_mod", "none"}, {"text", "Draw a card."}, {"effects", json::array()}}}}};
    };
    std::ofstream(generated.path / "TST-1.json") << card("TST-1", 70).dump();
    std::ofstream(generated.path / "OLD-5.json") << card("OLD-5", 70).dump();
    std::ofstream(generated.path / "TST-2.json") << card("TST-2", 80).dump();
    std::ofstream(effects.path / "TST-1.json") << R"({"attacks": {"Hit": {"effects": [{"op": "draw"}]}}})";

    const auto links = link_reprints(generated.path, effects.path);
    CHECK(links == std::map<std::string, std::string>{{"OLD-5", "TST-1"}});
    CHECK(link_reprints(generated.path, effects.path) == links);
    const pokebot::CardDB db = pokebot::CardDB::load(generated.path, effects.path);
    CHECK(db.at("OLD-5").attacks[0].effects.size() == 1);
    CHECK(db.at("TST-2").attacks[0].effects.empty());

    std::filesystem::remove(effects.path / "TST-1.json");
    CHECK(link_reprints(generated.path, effects.path).empty());
    std::ifstream in(generated.path / "OLD-5.json");
    CHECK_FALSE(json::parse(in).contains("same_as"));
}

TEST_CASE("cards without effects get a stored translation, and lose it once they have their own", "[standard]") {
    TempDir generated;
    TempDir effects;
    const auto card = [](const std::string& id, const std::string& text) {
        return json{{"id", id}, {"name", "Testmon"}, {"kind", "pokemon"}, {"rules", json::array()}, {"stage", "basic"}, {"hp", 70}, {"types", {"fire"}}, {"weakness", nullptr}, {"resistance", nullptr}, {"retreat", 1}, {"abilities", json::array()}, {"attacks", {{{"name", "Hit"}, {"cost", {"fire"}}, {"damage", 10}, {"damage_mod", "none"}, {"text", text}, {"effects", json::array()}}}}};
    };
    std::ofstream(generated.path / "TST-1.json") << card("TST-1", "Draw a card.").dump();
    std::ofstream(generated.path / "TST-2.json") << card("TST-2", "Draw 2 cards.").dump();
    std::ofstream(generated.path / "TST-3.json") << card("TST-3", "Draw a card and win the game.").dump();
    std::ofstream(effects.path / "TST-1.json") << R"({"attacks": {"Hit": {"effects": [{"op": "draw"}, {"op": "draw"}]}}})";

    const StoredTranslations stored = translate_generated(generated.path, effects.path);
    CHECK(stored.cards == std::set<std::string>{"TST-2"});
    CHECK(stored.complete == 1);
    CHECK(stored.problems.empty());
    const pokebot::CardDB db = pokebot::CardDB::load(generated.path, effects.path);
    CHECK(db.at("TST-1").attacks[0].effects.size() == 2);
    CHECK(pokebot::missing_effects(db.at("TST-2")).empty());
    CHECK_FALSE(pokebot::missing_effects(db.at("TST-3")).empty());

    std::ofstream(effects.path / "TST-2.json") << R"({"attacks": {"Hit": {"effects": [{"op": "draw"}]}}})";
    CHECK(translate_generated(generated.path, effects.path).cards.empty());
    std::ifstream in(generated.path / "TST-2.json");
    CHECK_FALSE(json::parse(in).contains("translation"));
}
