#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cards/db/card_db.hpp"
#include "translate.hpp"

using nlohmann::json;
using pokebot::compile_attack;
using pokebot::DamageMod;
using pokebot::import::AttackContext;
using pokebot::import::normalize_text;
using pokebot::import::split_sentences;
using pokebot::import::translate_attack;

namespace {

    void reads_as(const std::string& text, DamageMod mod, const char* written) {
        INFO(text);
        const auto translation = translate_attack(text, AttackContext{"Test Attack", mod});
        INFO("unread: " << translation.unread);
        REQUIRE(translation.ok);
        INFO("translated: " << translation.effects.dump());
        CHECK(compile_attack(translation.effects) == compile_attack(json::parse(written)));
    }

    void not_read(const std::string& text, DamageMod mod = DamageMod::None) {
        INFO(text);
        CHECK_FALSE(translate_attack(text, AttackContext{"Test Attack", mod}).ok);
    }

} // namespace

TEST_CASE("card text is cleaned before it's read", "[translate]") {
    CHECK(normalize_text("Draw  a <em>card</em>.\n") == "Draw a card.");
    CHECK(normalize_text("Your opponent\xE2\x80\x99s Active") == "Your opponent's Active");
    CHECK(normalize_text("Hit 1 of them. (Don't apply Weakness and Resistance for Benched Pokémon.)") == "Hit 1 of them.");
    CHECK(split_sentences("Flip a coin. If heads, draw a card.") == std::vector<std::string>{"Flip a coin", "If heads, draw a card"});
    CHECK(split_sentences("").empty());
}

TEST_CASE("damage formulas, coin flips and conditions", "[translate]") {
    reads_as("Flip 3 coins. This attack does 40 damage for each heads.", DamageMod::Times,
             R"([{"op": "flip_coins", "count": 3}, {"op": "set_damage", "per": 40, "for_each": {"of": "heads"}}, {"op": "damage"}])");
    reads_as("Flip a coin until you get tails. This attack does 30 more damage for each heads.", DamageMod::Plus,
             R"([{"op": "flip_until_tails"}, {"op": "add_damage", "per": 30, "for_each": {"of": "heads"}}, {"op": "damage"}])");
    reads_as("This attack does 10 damage for each damage counter on this Pokémon.", DamageMod::Times,
             R"([{"op": "set_damage", "per": 10, "for_each": {"of": "damage_counters"}}, {"op": "damage"}])");
    reads_as("This attack does 20 more damage for each Benched Pokémon (both yours and your opponent's).", DamageMod::Plus,
             R"([{"op": "add_damage", "per": 20, "for_each": {"of": "benched", "player": "each"}}, {"op": "damage"}])");
    reads_as("This attack does 50 damage for each Basic Energy card in your opponent's discard pile.", DamageMod::Times,
             R"([{"op": "set_damage", "per": 50, "for_each": {"of": "discard", "player": "opponent", "filter": {"kind": "energy", "basic": true}}}, {"op": "damage"}])");
    reads_as("If a Stadium is in play, this attack does 70 more damage.", DamageMod::Plus,
             R"([{"op": "if", "condition": {"stadium_in_play": true}, "then": [{"op": "add_damage", "amount": 70}]}, {"op": "damage"}])");
    reads_as("Flip a coin. If tails, this Pokémon is now Confused.", DamageMod::None,
             R"([{"op": "flip_coins"}, {"op": "if", "condition": {"heads_at_least": 1}, "then": [], "else": [{"op": "special_condition", "condition": "confused", "player": "you"}]}])");
    reads_as("Flip 2 coins. If both of them are heads, your opponent's Active Pokémon is now Burned, Confused, and Poisoned.", DamageMod::None,
             R"([{"op": "flip_coins", "count": 2}, {"op": "if", "condition": {"heads_at_least": 2}, "then": [{"op": "special_condition", "condition": "burned"},
                 {"op": "special_condition", "condition": "confused"}, {"op": "special_condition", "condition": "poisoned"}]}])");
}

TEST_CASE("damage to chosen Pokemon and damage counters", "[translate]") {
    reads_as("This attack does 60 damage to 2 of your opponent's Pokémon.", DamageMod::None, R"([{"op": "damage_to", "targets": 2, "amount": 60}])");
    reads_as("Discard all Energy from this Pokémon, and this attack also does 80 damage to 1 of your opponent's Benched Pokémon. (Don't apply Weakness and Resistance for Benched Pokémon.)",
             DamageMod::None, R"([{"op": "discard_all_attached_energy"}, {"op": "damage_to", "on": "bench", "amount": 80}])");
    reads_as("This attack does 20 damage to 1 of your opponent's Pokémon for each damage counter on that Pokémon.", DamageMod::None,
             R"([{"op": "damage_to", "per": 20, "for_each": {"of": "target_damage_counters"}}])");
    reads_as("Put 4 damage counters on your opponent's Benched Pokémon in any way you like.", DamageMod::None, R"([{"op": "place_counters", "count": 4, "on": "bench"}])");
    reads_as("Put 3 damage counters on 1 of your opponent's Pokémon.", DamageMod::None, R"([{"op": "counters_to", "count": 3}])");
}

TEST_CASE("cards from the deck, the discard pile and Energy", "[translate]") {
    reads_as("Search your deck for up to 2 {L} Pokémon, reveal them, and put them into your hand. Then, shuffle your deck.", DamageMod::None,
             R"([{"op": "search_deck", "reveal": true, "count": 2, "filter": {"kind": "pokemon", "type": "lightning"}}])");
    reads_as("Search your deck for up to 3 Basic Pokémon with 60 HP or less and put them onto your Bench. Then, shuffle your deck.", DamageMod::None,
             R"([{"op": "search_deck", "count": 3, "to": "bench", "filter": {"kind": "pokemon", "stage": "basic", "max_hp": 60}}])");
    reads_as("You may search your deck for a Supporter card and put it into your hand. Then, shuffle your deck.", DamageMod::None,
             R"([{"op": "may", "effects": [{"op": "search_deck", "filter": {"trainer": "supporter"}}]}])");
    reads_as("Search your deck for a Basic {G} Energy card and attach it to 1 of your Benched Pokémon. Then, shuffle your deck.", DamageMod::None,
             R"([{"op": "attach_to", "from": "deck", "on": "bench", "filter": {"kind": "energy", "basic": true, "type": "grass"}}])");
    reads_as("Attach a Basic {R} Energy card from your hand to this Pokémon.", DamageMod::None,
             R"([{"op": "attach_from_hand", "filter": {"kind": "energy", "basic": true, "type": "fire"}}])");
    reads_as("Discard 2 {W} Energy from this Pokémon.", DamageMod::None, R"([{"op": "move_attached_energy", "count": 2, "to": "discard", "filter": {"type": "water"}}])");
    reads_as("You may discard any amount of {L} Energy from your Pokémon. This attack does 40 damage for each card you discarded in this way.", DamageMod::Times,
             R"([{"op": "move_attached_energy", "from": "your_pokemon", "count": 60, "up_to": true, "to": "discard", "filter": {"type": "lightning"}},
                 {"op": "set_damage", "per": 40, "for_each": {"of": "moved"}}, {"op": "damage"}])");
    reads_as("You may put an Energy attached to this Pokémon into your hand and have this attack do 90 more damage.", DamageMod::Plus,
             R"([{"op": "may", "effects": [{"op": "move_attached_energy"}]}, {"op": "if", "condition": {"moved_at_least": 1}, "then": [{"op": "add_damage", "amount": 90}]}, {"op": "damage"}])");
    reads_as("You may shuffle 2 Energy attached to this Pokémon into your deck. If you do, this attack also does 50 damage to 1 of your opponent's Benched Pokémon.",
             DamageMod::None,
             R"([{"op": "if", "condition": {"self_energy_at_least": 2}, "then": [{"op": "may", "effects": [{"op": "move_attached_energy", "count": 2, "to": "deck"},
                 {"op": "shuffle_deck"}, {"op": "damage_to", "on": "bench", "amount": 50}]}]}])");
    reads_as("Discard your hand and draw 5 cards.", DamageMod::None, R"([{"op": "discard_hand"}, {"op": "draw", "count": 5}])");
}

TEST_CASE("lasting effects, switching, and attacks of other Pokemon", "[translate]") {
    reads_as("During your next turn, this Pokémon can't use Test Attack.", DamageMod::None, R"([{"op": "lasting", "effect": "cant_use_attack", "until": "your_next_turn"}])");
    reads_as("During your next turn, this Pokémon can't attack.", DamageMod::None, R"([{"op": "lasting", "effect": "cant_attack", "until": "your_next_turn"}])");
    reads_as("During your opponent's next turn, they can't play any Item cards from their hand.", DamageMod::None,
             R"([{"op": "lasting", "effect": "no_items", "on": "player", "player": "opponent", "until": "opponents_next_turn"}])");
    reads_as("Switch this Pokémon with 1 of your Benched Pokémon.", DamageMod::None, R"([{"op": "switch_self"}])");
    reads_as("Choose 1 of your Benched Ethan's Pokémon's attacks and use it as this attack.", DamageMod::None, R"([{"op": "use_attack", "filter": {"name_prefix": "Ethan's"}}])");
    reads_as("This attack's damage isn't affected by any effects on your opponent's Active Pokémon.", DamageMod::None, R"([{"op": "ignore_defender_effects"}, {"op": "damage"}])");
}

TEST_CASE("the translator reads all of a text or none of it", "[translate]") {
    not_read("Draw a card quickly.");
    not_read("Heal 30 damage from this Pokémon.");
    not_read("During your next turn, this Pokémon can't use Another Attack.");
    not_read("This attack does 30 more damage for each heads.", DamageMod::Times);
    not_read("This attack does 30 damage for each heads.", DamageMod::Plus);
    not_read("Draw a card.", DamageMod::Plus);
    not_read("If you do, draw a card.");
    const auto translation = translate_attack("Draw a card. Heal 30 damage from this Pokémon.", AttackContext{"Test Attack", DamageMod::None});
    CHECK(translation.unread == "Heal 30 damage from this Pokémon");
}

TEST_CASE("every hand-written attack the translator reads compiles to the same program", "[translate]") {
    const std::filesystem::path root = POKEBOT_SOURCE_DIR;
    if (!std::filesystem::exists(root / "data/cards/generated")) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const pokebot::CardDB db = pokebot::CardDB::load(root / "data/cards/generated", root / "data/cards/effects");
    std::string different;
    int read = 0;
    for (const auto& file : std::filesystem::directory_iterator(root / "data/cards/effects")) {
        std::ifstream in(root / "data/cards/generated" / file.path().filename(), std::ios::binary);
        const pokebot::CardDef card = json::parse(in).get<pokebot::CardDef>();
        const pokebot::CardDef& written = db.at(card.id);
        for (std::size_t i = 0; i < card.attacks.size(); ++i) {
            if (card.attacks[i].text.empty()) {
                continue;
            }
            const auto translation = translate_attack(card.attacks[i].text, AttackContext{card.attacks[i].name, card.attacks[i].damage_mod});
            if (!translation.ok) {
                continue;
            }
            ++read;
            if (compile_attack(translation.effects) != written.attacks[i].program) {
                different += "\n  " + card.id + " " + card.attacks[i].name + ": " + translation.effects.dump();
            }
        }
    }
    INFO("different:" << different);
    CHECK(different.empty());
    CHECK(read > 0);
}
