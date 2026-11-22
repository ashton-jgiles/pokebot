#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "agents/flat_mc/deck_model.hpp"
#include "agents/random/random_agent.hpp"
#include "engine/observation/observation.hpp"
#include "harness/match.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using pokebot::ArchetypeError;
using pokebot::ArchetypeLibrary;
using pokebot::ArchetypeList;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardKind;
using pokebot::DeckModel;
using pokebot::EnergyType;
using pokebot::SeenCards;
using pokebot::TrainerKind;

namespace {

    CardDB test_db() {
        CardDB db;
        db.add(test::basic_pokemon("TST-1", "Card 1"));
        CardDef item;
        item.id = "TST-2";
        item.name = "Card 2";
        item.kind = CardKind::Trainer;
        item.trainer_kind = TrainerKind::Item;
        db.add(item);
        CardDef unwritten = test::basic_pokemon("TST-3", "Card 3");
        pokebot::Attack attack = test::attack("Mystery", {EnergyType::Colorless}, 10);
        attack.text = "Flip a coin. If heads, something happens.";
        unwritten.attacks.push_back(attack);
        db.add(unwritten);
        db.add(test::basic_pokemon("TST-4", "Card 4"));
        db.add(test::basic_energy("TST-6", "Card 6", EnergyType::Grass));
        return db;
    }

    ArchetypeLibrary belief_library(const CardDB& db) {
        ArchetypeLibrary library;
        library.add("ace", "main", test::deck({{4, 1}, {56, 6}}), db);
        library.add("ace", "tech", test::deck({{4, 1}, {4, 2}, {52, 6}}), db);
        library.add("box", "main", test::deck({{4, 4}, {56, 6}}), db);
        return library;
    }

    double archetype_weight(const DeckModel& model, const std::vector<double>& belief, const std::string& archetype) {
        for (const auto& [name, weight] : model.by_archetype(belief)) {
            if (name == archetype) {
                return weight;
            }
        }
        return 0.0;
    }

    struct TempDir {
        std::filesystem::path path;
        TempDir() {
            std::random_device rd;
            path = std::filesystem::temp_directory_path() / ("pokebot-test-archetypes-" + std::to_string(rd()));
            std::filesystem::create_directories(path);
        }
        ~TempDir() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
        void write(const std::string& file, const std::string& text) const {
            std::filesystem::create_directories((path / file).parent_path());
            std::ofstream(path / file, std::ios::binary) << text;
        }
    };

    const char* const kLegal = "4 Card 1 TST 1\n4 Card 2 TST 2\n52 Card 6 TST 6\n";

} // namespace

TEST_CASE("a list's cards are counted by card", "[deck_model]") {
    const CardDB db = test_db();
    ArchetypeLibrary library;
    library.add("box", "main", test::deck({{2, 1}, {4, 2}, {2, 1}, {52, 6}}), db);
    REQUIRE(library.size() == 1);
    const ArchetypeList& list = library.lists().front();
    CHECK(list.count("TST-1") == 4);
    CHECK(list.count("TST-2") == 4);
    CHECK(list.count("TST-6") == 52);
    CHECK(list.count("TST-3") == 0);
    CHECK(list.count("nothing") == 0);
    REQUIRE(list.cards.size() == 3);
    CHECK(list.cards[0].def == db.find("TST-1"));
    CHECK(list.cards[1].def == db.find("TST-2"));
    CHECK(list.cards[2].def == db.find("TST-6"));
}

TEST_CASE("lists are kept in order by archetype, then variant", "[deck_model]") {
    const CardDB db = test_db();
    ArchetypeLibrary library;
    library.add("box", "main", test::deck({{4, 1}, {56, 6}}), db);
    library.add("ace", "tech", test::deck({{4, 1}, {4, 2}, {52, 6}}), db);
    library.add("ace", "main", test::deck({{4, 1}, {56, 6}}), db);
    REQUIRE(library.size() == 3);
    CHECK(library.lists()[0].archetype == "ace");
    CHECK(library.lists()[0].variant == "main");
    CHECK(library.lists()[1].variant == "tech");
    CHECK(library.lists()[2].archetype == "box");
    CHECK(library.archetypes() == std::vector<std::string>{"ace", "box"});
    REQUIRE(library.find("ace", "tech") != nullptr);
    CHECK(library.find("ace", "tech")->count("TST-2") == 4);
    CHECK(library.find("box", "tech") == nullptr);
}

TEST_CASE("only legal lists with every card's effects written are accepted", "[deck_model]") {
    const CardDB db = test_db();
    ArchetypeLibrary library;
    CHECK_THROWS_AS(library.add("box", "short", test::deck({{4, 1}, {55, 6}}), db), ArchetypeError);
    CHECK_THROWS_WITH(library.add("box", "short", test::deck({{4, 1}, {55, 6}}), db), ContainsSubstring("box/short") && ContainsSubstring("59 cards"));
    CHECK_THROWS_WITH(library.add("box", "five", test::deck({{5, 1}, {55, 6}}), db), ContainsSubstring("5 cards named"));
    CHECK_THROWS_WITH(library.add("box", "unknown", test::deck({{4, 9}, {56, 6}}), db), ContainsSubstring("unknown card TST-9"));
    CHECK_THROWS_WITH(library.add("box", "unwritten", test::deck({{4, 3}, {56, 6}}), db), ContainsSubstring("without effects") && ContainsSubstring("TST-3 Card 3: attack Mystery"));
    CHECK(library.empty());

    library.add("box", "main", test::deck({{4, 1}, {56, 6}}), db);
    CHECK_THROWS_WITH(library.add("box", "main", test::deck({{4, 1}, {4, 2}, {52, 6}}), db), ContainsSubstring("listed twice"));
    CHECK(library.size() == 1);
}

TEST_CASE("a library loads from one folder per archetype, a file per variant", "[deck_model]") {
    const CardDB db = test_db();
    const TempDir dir;
    dir.write("box/main.txt", kLegal);
    dir.write("box/no-items.txt", "Total Cards: 60\n4 Card 1 TST 1\n56 Card 6 TST 6\n");
    dir.write("box/notes.md", "not a list");
    dir.write("ace/main.txt", kLegal);
    dir.write("README.md", "not an archetype");
    const ArchetypeLibrary library = ArchetypeLibrary::load(dir.path, db);
    REQUIRE(library.size() == 3);
    CHECK(library.archetypes() == std::vector<std::string>{"ace", "box"});
    REQUIRE(library.find("box", "no-items") != nullptr);
    CHECK(library.find("box", "no-items")->count("TST-2") == 0);
    CHECK(library.find("box", "main")->count("TST-2") == 4);
}

TEST_CASE("a library that can't be loaded says why", "[deck_model]") {
    const CardDB db = test_db();
    SECTION("no folder") {
        const TempDir dir;
        CHECK_THROWS_WITH(ArchetypeLibrary::load(dir.path / "missing", db), ContainsSubstring("no archetype library"));
    }
    SECTION("no archetypes") {
        const TempDir dir;
        CHECK_THROWS_WITH(ArchetypeLibrary::load(dir.path, db), ContainsSubstring("no archetypes"));
    }
    SECTION("an archetype with no lists") {
        const TempDir dir;
        dir.write("box/notes.md", "not a list");
        CHECK_THROWS_WITH(ArchetypeLibrary::load(dir.path, db), ContainsSubstring("archetype box has no lists"));
    }
    SECTION("a list that doesn't parse") {
        const TempDir dir;
        dir.write("box/main.txt", "4 Card\n");
        CHECK_THROWS_WITH(ArchetypeLibrary::load(dir.path, db), ContainsSubstring("main.txt") && ContainsSubstring("line 1"));
    }
    SECTION("an illegal list") {
        const TempDir dir;
        dir.write("box/main.txt", "4 Card 1 TST 1\n");
        CHECK_THROWS_WITH(ArchetypeLibrary::load(dir.path, db), ContainsSubstring("box/main is not a legal deck"));
    }
}

TEST_CASE("the project's archetype library", "[deck_model]") {
    const std::filesystem::path root = POKEBOT_SOURCE_DIR;
    if (!std::filesystem::exists(root / "data/cards/generated")) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const CardDB db = CardDB::load(root / "data/cards/generated", root / "data/cards/effects");
    const ArchetypeLibrary library = ArchetypeLibrary::load(root / "data/archetypes", db);

    std::vector<std::string> decks;
    for (const auto& file : std::filesystem::directory_iterator(root / "data/decks")) {
        if (file.path().extension() == ".txt") {
            decks.push_back(file.path().stem().string());
        }
    }
    std::sort(decks.begin(), decks.end());
    CHECK(library.archetypes() == decks);

    for (const std::string& deck : decks) {
        INFO(deck);
        const ArchetypeList* main = library.find(deck, "main");
        REQUIRE(main != nullptr);
        CHECK(main->list == pokebot::load_decklist(root / "data/decks" / (deck + ".txt")));
        int variants = 0;
        for (const ArchetypeList& list : library.lists()) {
            variants += list.archetype == deck ? 1 : 0;
        }
        CHECK(variants >= 3);
    }

    std::set<std::vector<std::pair<std::string, int>>> distinct;
    for (const ArchetypeList& list : library.lists()) {
        std::vector<std::pair<std::string, int>> cards;
        for (const pokebot::ArchetypeCard& card : list.cards) {
            cards.emplace_back(card.def->id, card.count);
        }
        distinct.insert(cards);
    }
    CHECK(distinct.size() == library.size());
}

TEST_CASE("with nothing seen, the belief is the prior: archetypes alike, then their variants", "[deck_model]") {
    const CardDB db = test_db();
    const ArchetypeLibrary library = belief_library(db);
    const DeckModel model(library);
    const std::vector<double> belief = model.belief(SeenCards{});
    REQUIRE(belief.size() == 3);
    CHECK_THAT(belief[0], WithinAbs(0.25, 1e-12));
    CHECK_THAT(belief[1], WithinAbs(0.25, 1e-12));
    CHECK_THAT(belief[2], WithinAbs(0.5, 1e-12));
}

TEST_CASE("a card only one archetype plays settles the archetype", "[deck_model]") {
    const CardDB db = test_db();
    const ArchetypeLibrary library = belief_library(db);
    const DeckModel model(library);
    const std::vector<double> belief = model.belief(SeenCards{{"TST-4", 1}});
    CHECK(belief[2] > 0.99);
    CHECK_THAT(belief[0], WithinRel(belief[1], 1e-12));
    CHECK_THAT(archetype_weight(model, belief, "box"), WithinAbs(belief[2], 1e-12));
}

TEST_CASE("a tech card picks the variant", "[deck_model]") {
    const CardDB db = test_db();
    const ArchetypeLibrary library = belief_library(db);
    const DeckModel model(library, 0.01);
    const std::vector<double> belief = model.belief(SeenCards{{"TST-1", 1}, {"TST-2", 1}});
    CHECK_THAT(belief[1] / belief[0], WithinRel(400.0, 1e-9));
    CHECK(belief[1] > 0.99);
}

TEST_CASE("how many copies are seen matters, by the hypergeometric count", "[deck_model]") {
    const CardDB db = test_db();
    ArchetypeLibrary library;
    library.add("x", "four", test::deck({{4, 1}, {56, 6}}), db);
    library.add("x", "two", test::deck({{2, 1}, {58, 6}}), db);
    const DeckModel model(library, 0.01);
    const std::vector<double> one = model.belief(SeenCards{{"TST-1", 1}});
    CHECK_THAT(one[0], WithinAbs(2.0 / 3.0, 1e-12));
    const std::vector<double> three = model.belief(SeenCards{{"TST-1", 3}});
    CHECK_THAT(three[1] / three[0], WithinRel(0.01 / 4.0, 1e-9));
}

TEST_CASE("cards no list explains keep the belief defined", "[deck_model]") {
    const CardDB db = test_db();
    const ArchetypeLibrary library = belief_library(db);
    const DeckModel model(library);
    const std::vector<double> belief = model.belief(SeenCards{{"TST-9", 2}});
    for (std::size_t i = 0; i < belief.size(); ++i) {
        CHECK_THAT(belief[i], WithinAbs(model.prior(i), 1e-12));
    }
    double total = 0.0;
    for (const double weight : model.belief(SeenCards{{"TST-9", 60}, {"TST-4", 5}})) {
        CHECK(weight >= 0.0);
        total += weight;
    }
    CHECK_THAT(total, WithinAbs(1.0, 1e-12));
}

TEST_CASE("the belief summed by archetype", "[deck_model]") {
    const CardDB db = test_db();
    const ArchetypeLibrary library = belief_library(db);
    const DeckModel model(library);
    const auto sums = model.by_archetype(model.belief(SeenCards{{"TST-1", 1}}));
    REQUIRE(sums.size() == 2);
    CHECK(sums[0].first == "ace");
    CHECK(sums[1].first == "box");
    CHECK_THAT(sums[0].second + sums[1].second, WithinAbs(1.0, 1e-12));
    CHECK(sums[0].second > 0.99);
}

TEST_CASE("a deck model needs lists and a chance between 0 and 1", "[deck_model]") {
    const CardDB db = test_db();
    const ArchetypeLibrary empty;
    CHECK_THROWS_AS(DeckModel(empty), std::invalid_argument);
    const ArchetypeLibrary library = belief_library(db);
    CHECK_THROWS_AS(DeckModel(library, 0.0), std::invalid_argument);
    CHECK_THROWS_AS(DeckModel(library, 1.5), std::invalid_argument);
    CHECK_NOTHROW(DeckModel(library, 1.0));
}

TEST_CASE("on the real decks, the belief depends only on the cards seen and finds the archetype", "[deck_model]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::filesystem::path root = POKEBOT_SOURCE_DIR;
    const ArchetypeLibrary library = ArchetypeLibrary::load(root / "data/archetypes", real->db);
    const DeckModel model(library);
    std::map<SeenCards, std::vector<double>> by_seen;
    std::uint64_t seed = 1;
    for (const ArchetypeList& opponent : library.lists()) {
        ArchetypeLibrary without;
        for (const ArchetypeList& list : library.lists()) {
            if (&list != &opponent) {
                without.add(list.archetype, list.variant, list.list, real->db);
            }
        }
        const DeckModel missing(without);
        const pokebot::CardTable table(real->db, real->decks.front(), opponent.list);
        for (int game = 0; game < 2; ++game, ++seed) {
            INFO(opponent.archetype << "/" << opponent.variant << " game " << game);
            pokebot::BiasedRandomAgent a(seed), b(seed + 1000);
            pokebot::MatchOptions options;
            options.check_invariants = false;
            pokebot::Observation last;
            options.on_action = [&](const pokebot::GameState&, const pokebot::Action&, const pokebot::GameState& after) {
                if (after.turn == 0) {
                    return;
                }
                last = pokebot::observe(after, 0);
                const std::vector<double> belief = model.belief(last);
                const auto [it, added] = by_seen.emplace(pokebot::opponent_cards_seen(last), belief);
                if (!added) {
                    CHECK(it->second == belief);
                }
            };
            const pokebot::GameRecord record = pokebot::play_game(table, seed, a, b, options);
            if (record.turns < 10) {
                continue;
            }
            CHECK(archetype_weight(model, model.belief(last), opponent.archetype) > 0.9);
            const auto sums = missing.by_archetype(missing.belief(last));
            const auto best = std::max_element(sums.begin(), sums.end(), [](const auto& x, const auto& y) { return x.second < y.second; });
            CHECK(best->first == opponent.archetype);
        }
    }
}
