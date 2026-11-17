#include <filesystem>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "cards/deck_validator.hpp"

using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardKind;
using pokebot::DeckEntry;
using pokebot::Decklist;
using pokebot::DeckValidation;
using pokebot::EnergyKind;
using pokebot::EnergyType;
using pokebot::Stage;
using pokebot::TrainerKind;
using pokebot::validate_deck;

namespace {

    CardDef pokemon(const std::string& id, const std::string& name, Stage stage) {
        CardDef c;
        c.id = id;
        c.name = name;
        c.kind = CardKind::Pokemon;
        c.stage = stage;
        c.hp = 60;
        c.types = {EnergyType::Colorless};
        return c;
    }

    CardDef item(const std::string& id, const std::string& name, bool ace_spec = false) {
        CardDef c;
        c.id = id;
        c.name = name;
        c.kind = CardKind::Trainer;
        c.trainer_kind = TrainerKind::Item;
        c.rules.ace_spec = ace_spec;
        return c;
    }

    CardDef basic_energy(const std::string& id, const std::string& name) {
        CardDef c;
        c.id = id;
        c.name = name;
        c.kind = CardKind::Energy;
        c.energy_kind = EnergyKind::Basic;
        c.provides = {EnergyType::Darkness};
        return c;
    }

    // TST-1 and TST-7 are two printings of the same card name.
    CardDB sample_db() {
        CardDB db;
        db.add(pokemon("TST-1", "Testmon", Stage::Basic));
        db.add(pokemon("TST-2", "Testvolve", Stage::Stage1));
        db.add(item("TST-3", "Test Ball"));
        db.add(item("TST-4", "Test Box", true));
        db.add(item("TST-5", "Other Box", true));
        db.add(basic_energy("TST-6", "Darkness Energy"));
        db.add(pokemon("TST-7", "Testmon", Stage::Basic));
        return db;
    }

    DeckEntry entry(int count, int number) {
        return DeckEntry{count, "Card " + std::to_string(number), "TST", number};
    }

    // 4 + 4 + 4 + 1 + 47 = 60 cards; 47 Basic Energy is fine.
    Decklist legal_deck() {
        return Decklist{{entry(4, 1), entry(4, 2), entry(4, 3), entry(1, 4), entry(47, 6)}};
    }

    bool mentions(const DeckValidation& v, const std::string& text) {
        for (const auto& problem : v.problems) {
            if (problem.find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

} // namespace

TEST_CASE("a legal deck has no problems", "[deck_validator]") {
    const auto v = validate_deck(legal_deck(), sample_db());
    CHECK(v.legal());
    CHECK(v.problems.empty());
}

TEST_CASE("a deck must have exactly 60 cards", "[deck_validator]") {
    auto deck = legal_deck();
    deck.entries.back().count = 46; // 59 cards
    const auto v = validate_deck(deck, sample_db());
    CHECK_FALSE(v.legal());
    CHECK(mentions(v, "59 cards"));
}

TEST_CASE("the 4-copy limit counts every printing of a name together", "[deck_validator]") {
    auto deck = legal_deck();
    deck.entries.push_back(entry(1, 7)); // a 5th "Testmon", different printing
    deck.entries.back().count = 1;
    deck.entries[4].count = 46; // keep 60 cards
    const auto v = validate_deck(deck, sample_db());
    REQUIRE(v.problems.size() == 1);
    CHECK(mentions(v, "5 cards named \"Testmon\""));
}

TEST_CASE("Basic Energy has no copy limit", "[deck_validator]") {
    const auto v = validate_deck(legal_deck(), sample_db()); // 47 Darkness Energy
    CHECK_FALSE(mentions(v, "Darkness Energy"));
}

TEST_CASE("a deck needs at least one Basic Pokemon", "[deck_validator]") {
    const Decklist deck{{entry(4, 2), entry(4, 3), entry(52, 6)}}; // only a Stage 1
    const auto v = validate_deck(deck, sample_db());
    REQUIRE(v.problems.size() == 1);
    CHECK(mentions(v, "no Basic Pokemon"));
}

TEST_CASE("at most one ACE SPEC card", "[deck_validator]") {
    auto deck = legal_deck();
    deck.entries.push_back(entry(1, 5)); // a second ACE SPEC
    deck.entries[4].count = 46;
    const auto v = validate_deck(deck, sample_db());
    REQUIRE(v.problems.size() == 1);
    CHECK(mentions(v, "2 ACE SPEC cards (Test Box, Other Box)"));
}

TEST_CASE("unknown cards are reported with a hint", "[deck_validator]") {
    auto deck = legal_deck();
    deck.entries.push_back(DeckEntry{1, "Mystery Card", "TST", 999});
    deck.entries[4].count = 46;
    const auto v = validate_deck(deck, sample_db());
    REQUIRE(v.problems.size() == 1);
    CHECK(mentions(v, "unknown card TST-999"));
    CHECK(mentions(v, "pokebot-import"));
}

TEST_CASE("every problem is reported at once", "[deck_validator]") {
    // 61 cards, 5 copies of Testmon, and 2 ACE SPEC cards.
    const Decklist deck{{entry(4, 1), entry(1, 7), entry(1, 4), entry(1, 5), entry(54, 6)}};
    const auto v = validate_deck(deck, sample_db());
    CHECK(v.problems.size() == 3);
    CHECK(mentions(v, "61 cards"));
    CHECK(mentions(v, "5 cards named"));
    CHECK(mentions(v, "ACE SPEC"));
}

TEST_CASE("the project's decks are legal", "[deck_validator]") {
    const std::filesystem::path root = POKEBOT_SOURCE_DIR;
    if (!std::filesystem::exists(root / "data/cards/generated")) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const CardDB db = CardDB::load(root / "data/cards/generated", root / "data/cards/effects");
    for (const auto& file : std::filesystem::directory_iterator(root / "data/decks")) {
        if (file.path().extension() != ".txt") {
            continue;
        }
        INFO(file.path().filename().string());
        const auto v = validate_deck(pokebot::load_decklist(file.path()), db);
        CHECK(v.problems.empty());
    }
}