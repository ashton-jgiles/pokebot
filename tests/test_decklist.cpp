#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "core/decklist.hpp"

using pokebot::DecklistError;
using pokebot::parse_decklist;

namespace {

pokebot::Decklist parse(const std::string& text) {
  std::istringstream in(text);
  return parse_decklist(in);
}

}  // namespace

TEST_CASE("parses names with spaces, apostrophes and energy symbols", "[decklist]") {
  const auto deck = parse(
      "Pokémon: 1\n"
      "4 N's Zoroark ex JTG 98\n"
      "\n"
      "Trainer: 1\n"
      "2 Buddy-Buddy Poffin ASC 184\n"
      "\n"
      "Energy: 1\n"
      "7 Basic {D} Energy MEE 7\n"
      "\n"
      "Total Cards: 13\n");

  REQUIRE(deck.entries.size() == 3);
  CHECK(deck.entries[0].count == 4);
  CHECK(deck.entries[0].name == "N's Zoroark ex");
  CHECK(deck.entries[0].set_code == "JTG");
  CHECK(deck.entries[0].number == 98);
  CHECK(deck.entries[1].name == "Buddy-Buddy Poffin");
  CHECK(deck.entries[2].name == "Basic {D} Energy");
  CHECK(deck.entries[2].number == 7);
  CHECK(deck.total_cards() == 13);
}

TEST_CASE("handles Windows line endings and missing total", "[decklist]") {
  const auto deck = parse("Pokémon: 1\r\n3 Dragapult ex TWM 130\r\n");
  REQUIRE(deck.entries.size() == 1);
  CHECK(deck.entries[0].name == "Dragapult ex");
  CHECK(deck.entries[0].number == 130);
}

TEST_CASE("rejects a total that doesn't match the cards", "[decklist]") {
  CHECK_THROWS_AS(parse("4 Dreepy TWM 128\nTotal Cards: 60\n"), DecklistError);
}

TEST_CASE("reports the line number of a bad line", "[decklist]") {
  try {
    parse("Pokémon: 2\n4 Dreepy TWM 128\n4 Drakloak twm 129\n");
    FAIL("expected a DecklistError");
  } catch (const DecklistError& e) {
    CHECK(e.line() == 3);  // lowercase set code
  }
  CHECK_THROWS_AS(parse("4 Dreepy 128\n"), DecklistError);       // no set code
  CHECK_THROWS_AS(parse("0 Dreepy TWM 128\n"), DecklistError);   // zero count
  CHECK_THROWS_AS(parse("4 Dreepy TWM 12a\n"), DecklistError);   // bad number
}

TEST_CASE("both project decklists load with 60 cards", "[decklist]") {
  for (const char* name : {"dragapult.txt", "ns-zoroark.txt"}) {
    const auto deck =
        pokebot::load_decklist(std::string(POKEBOT_SOURCE_DIR) + "/data/decks/" + name);
    INFO(name);
    CHECK(deck.total_cards() == 60);
  }
}
