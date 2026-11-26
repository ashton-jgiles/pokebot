#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
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
using pokebot::import::translate_card;
using pokebot::import::translation_differences;

namespace {

    void reads_as(const std::string& text, DamageMod mod, const char* written) {
        INFO(text);
        const auto translation = translate_attack(text, AttackContext{"Test Attack", mod});
        INFO("unread: " << translation.unread);
        REQUIRE(translation.ok);
        INFO("translated: " << translation.effects.dump());
        CHECK(compile_attack(translation.effects) == compile_attack(json::parse(written)));
    }

    void card_reads_as(const std::string& generated, const char* written) {
        const pokebot::CardDef card = json::parse(generated).get<pokebot::CardDef>();
        const auto translation = translate_card(card);
        std::string unread;
        for (const auto& [part, why] : translation.unread) {
            unread += "\n  " + part + ": " + why;
        }
        INFO("unread:" << unread);
        REQUIRE(translation.unread.empty());
        INFO("translated: " << translation.overlay.dump());
        pokebot::CardDB expected;
        expected.add(pokebot::merge_effects(json(card), json::parse(written)).get<pokebot::CardDef>());
        pokebot::CardDB translated;
        translated.add(pokebot::merge_effects(json(card), translation.overlay).get<pokebot::CardDef>());
        CHECK(translation_differences(expected.at(card.id), translated.at(card.id), translation.overlay).empty());
        CHECK_FALSE(translation.overlay.empty());
    }

    std::string trainer(const char* kind, const char* text) {
        return json{{"id", "TST-1"}, {"name", "Test Card"}, {"kind", "trainer"}, {"trainer_kind", kind}, {"text", text}}.dump();
    }

    std::string pokemon_with(const char* ability, const char* text) {
        return json{{"id", "TST-2"}, {"name", "Testmon"}, {"kind", "pokemon"}, {"stage", "basic"}, {"hp", 100}, {"types", {"fire"}}, {"retreat", 1}, {"abilities", {{{"name", ability}, {"text", text}}}}}
            .dump();
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

TEST_CASE("healing, damage to itself, \"does nothing\", Weakness and Resistance, each target, conditions and counts", "[translate]") {
    reads_as("Heal 30 damage from this Pokémon.", DamageMod::None, R"([{"op": "heal_self", "amount": 30}])");
    reads_as("Heal 20 damage from each of your Benched Pokémon.", DamageMod::None, R"([{"op": "heal_to", "amount": 20, "each": true, "on": "bench"}])");
    reads_as("This Pokémon also does 30 damage to itself.", DamageMod::None, R"([{"op": "damage_self", "amount": 30}])");
    reads_as("Flip a coin. If tails, this attack does nothing.", DamageMod::None, R"([{"op": "flip_coins"}, {"op": "if", "condition": {"heads_at_least": 1}, "then": [{"op": "damage"}]}])");
    reads_as("Flip 2 coins. If either of them is tails, this attack does nothing. Your opponent's Active Pokémon is now Paralyzed.", DamageMod::None,
             R"([{"op": "flip_coins", "count": 2}, {"op": "if", "condition": {"heads_at_least": 2}, "then": [{"op": "damage"}, {"op": "special_condition", "condition": "paralyzed"}]}])");
    not_read("If tails, this attack does nothing.");
    not_read("Flip 2 coins. If tails, this attack does nothing.");
    reads_as("This attack's damage isn't affected by Weakness or Resistance.", DamageMod::None, R"([{"op": "ignore_weakness"}, {"op": "ignore_resistance"}, {"op": "damage"}])");
    reads_as("This attack also does 10 damage to each of your Benched Pokémon.", DamageMod::None,
             R"([{"op": "damage_to", "player": "you", "on": "bench", "each": true, "amount": 10}])");
    reads_as("This attack does 20 damage to each of your opponent's Pokémon.", DamageMod::None, R"([{"op": "damage_to", "each": true, "amount": 20}])");
    reads_as("If your opponent's Active Pokémon is a Pokémon ex, this attack does 50 more damage.", DamageMod::Plus,
             R"([{"op": "if", "condition": {"opponent_active_is": {"rule_box": true}}, "then": [{"op": "add_damage", "amount": 50}]}, {"op": "damage"}])");
    reads_as("If your opponent's Active Pokémon is Poisoned, this attack does 50 more damage.", DamageMod::Plus,
             R"([{"op": "if", "condition": {"opponent_active_has": "poisoned"}, "then": [{"op": "add_damage", "amount": 50}]}, {"op": "damage"}])");
    reads_as("This attack does 20 damage for each of your Pokémon in play.", DamageMod::Times,
             R"([{"op": "set_damage", "per": 20, "for_each": {"of": "in_play"}}, {"op": "damage"}])");
    reads_as("This attack does 30 more damage for each damage counter on your opponent's Active Pokémon.", DamageMod::Plus,
             R"([{"op": "add_damage", "per": 30, "for_each": {"of": "active_damage_counters", "player": "opponent"}}, {"op": "damage"}])");
    reads_as("Discard an Energy from your opponent's Active Pokémon.", DamageMod::None, R"([{"op": "discard_target_energy", "on": "active"}])");
    card_reads_as(trainer("item", "Heal 30 damage from 1 of your Pokémon."), R"({"requires": [{"has_damaged": true}], "effects": [{"op": "heal_to", "amount": 30}]})");
}

TEST_CASE("lasting damage changes, prevention, the attack flip, and damage passives", "[translate]") {
    reads_as("During your opponent's next turn, this Pokémon takes 30 less damage from attacks (after applying Weakness and Resistance).", DamageMod::None,
             R"([{"op": "lasting", "effect": "takes_less", "amount": 30, "until": "opponents_next_turn"}])");
    reads_as("Flip a coin. If heads, during your opponent's next turn, prevent all damage from and effects of attacks done to this Pokémon.", DamageMod::None,
             R"([{"op": "flip_coins"}, {"op": "if", "condition": {"heads_at_least": 1}, "then": [{"op": "lasting", "effect": "prevent_damage", "prevent_effects": true, "until": "opponents_next_turn"}]}])");
    reads_as("During your opponent's next turn, prevent all damage done to this Pokémon by attacks from Basic Pokémon.", DamageMod::None,
             R"([{"op": "lasting", "effect": "prevent_damage", "from_basic": true, "until": "opponents_next_turn"}])");
    reads_as("During your opponent's next turn, attacks used by the Defending Pokémon do 30 less damage (before applying Weakness and Resistance).", DamageMod::None,
             R"([{"op": "lasting", "effect": "deals_less", "amount": 30, "on": "defending", "until": "opponents_next_turn"}])");
    reads_as("During your next turn, attacks used by this Pokémon do 60 more damage to your opponent's Active Pokémon (before applying Weakness and Resistance).", DamageMod::None,
             R"([{"op": "lasting", "effect": "damage_bonus", "amount": 60, "until": "your_next_turn"}])");
    reads_as("During your opponent's next turn, if the Defending Pokémon tries to use an attack, your opponent flips a coin. If tails, that attack doesn't happen.", DamageMod::None,
             R"([{"op": "lasting", "effect": "attack_flip", "on": "defending", "until": "opponents_next_turn"}])");
    not_read("If tails, that attack doesn't happen.");
    card_reads_as(pokemon_with("Hard Shell", "This Pokémon takes 30 less damage from attacks (after applying Weakness and Resistance)."),
                  R"({"abilities": {"Hard Shell": {"passive": [{"modify": "damage_taken", "add": -30}]}}})");
    card_reads_as(pokemon_with("Bench Barrier", "As long as this Pokémon is on your Bench, prevent all damage from and effects of attacks from your opponent's Pokémon done to this Pokémon."),
                  R"({"abilities": {"Bench Barrier": {"passive": [{"modify": "prevent_damage", "if_benched": true}, {"modify": "prevent_effects", "if_benched": true}]}}})");
    card_reads_as(trainer("tool", "The Pokémon this card is attached to takes 20 less damage from attacks (after applying Weakness and Resistance)."),
                  R"({"passive": [{"modify": "damage_taken", "add": -20}]})");
}

TEST_CASE("gusting by the opponent's choice, drawing up to a hand size, discards, and Abilities that answer evolving or damage", "[translate]") {
    reads_as("Switch out your opponent's Active Pokémon to the Bench. (Your opponent chooses the new Active Pokémon.) If you do, this attack does 160 damage to the new Active Pokémon.",
             DamageMod::None,
             R"([{"op": "switch_active", "player": "opponent", "chooser": "opponent"}, {"op": "damage_to", "on": "active", "amount": 160}])");
    reads_as("Draw cards until you have 6 cards in your hand.", DamageMod::None, R"([{"op": "draw", "until": 6}])");
    reads_as("Before doing damage, discard all Pokémon Tools from your opponent's Active Pokémon. If you discarded a Pokémon Tool in this way, your opponent's Active Pokémon is now Paralyzed.",
             DamageMod::None,
             R"([{"op": "discard_tools"}, {"op": "damage"}, {"op": "if", "condition": {"moved_at_least": 1}, "then": [{"op": "special_condition", "condition": "paralyzed"}]}])");
    reads_as("Flip 3 coins. For each heads, discard a random card from your opponent's hand.", DamageMod::None,
             R"([{"op": "flip_coins", "count": 3}, {"op": "discard_random", "for_each": {"of": "heads"}}])");
    reads_as("Discard the top 2 cards of your opponent's deck.", DamageMod::None, R"([{"op": "discard_deck_top", "player": "opponent", "count": 2}])");
    reads_as("If your Benched Pokémon have any damage counters on them, this attack does 60 more damage.", DamageMod::Plus,
             R"([{"op": "if", "condition": {"bench_damaged": true}, "then": [{"op": "add_damage", "amount": 60}]}, {"op": "damage"}])");
    reads_as("During your next turn, this Pokémon can't retreat.", DamageMod::None, R"([{"op": "lasting", "effect": "cant_retreat", "until": "your_next_turn"}])");
    card_reads_as(trainer("item", "Heal 30 damage from your Active Pokémon."), R"({"effects": [{"op": "heal_to", "amount": 30, "on": "active"}]})");
    card_reads_as(pokemon_with("Psychic Draw", "Once during your turn, when you play this Pokémon from your hand to evolve 1 of your Pokémon, you may use this Ability. Draw 2 cards."),
                  R"({"abilities": {"Psychic Draw": {"trigger": "evolved", "effects": [{"op": "may", "effects": [{"op": "draw", "count": 2}]}]}}})");
    card_reads_as(pokemon_with("Poison Point", "If this Pokémon is in the Active Spot and is damaged by an attack from your opponent's Pokémon (even if this Pokémon is Knocked Out), the Attacking Pokémon is now Poisoned."),
                  R"({"abilities": {"Poison Point": {"trigger": "damaged_by_attack", "once_per_turn": false, "effects": [{"op": "special_condition", "condition": "poisoned"}]}}})");
    card_reads_as(pokemon_with("Whirlpool", "Once during your turn, you may switch your Active Pokémon with 1 of your Benched Pokémon. If you do, switch out your opponent's Active Pokémon to the Bench. (Your opponent chooses the new Active Pokémon.)"),
                  R"({"abilities": {"Whirlpool": {"requires": [{"has_benched": true}], "effects": [{"op": "switch_active"}, {"op": "switch_active", "player": "opponent", "chooser": "opponent"}]}}})");
}

TEST_CASE("typed Energy counts, minus damage, optional bonuses, Bench conditions, Energy in any way you like, and the first turn", "[translate]") {
    reads_as("This attack does 30 more damage for each {W} Energy attached to this Pokémon.", DamageMod::Plus,
             R"([{"op": "add_damage", "per": 30, "for_each": {"of": "self_energy", "filter": {"type": "water"}}}, {"op": "damage"}])");
    reads_as("Flip a coin for each Energy attached to this Pokémon. This attack does 70 damage for each heads.", DamageMod::Times,
             R"([{"op": "flip_coins", "for_each": {"of": "active_energy"}}, {"op": "set_damage", "per": 70, "for_each": {"of": "heads"}}, {"op": "damage"}])");
    reads_as("This attack does 40 damage for each of your Pokémon in play that has the Round attack.", DamageMod::Times,
             R"([{"op": "set_damage", "per": 40, "for_each": {"of": "in_play", "filter": {"has_attack": "Round"}}}, {"op": "damage"}])");
    reads_as("This attack does 10 less damage for each damage counter on this Pokémon.", DamageMod::Minus,
             R"([{"op": "add_damage", "per": -10, "for_each": {"of": "damage_counters"}}, {"op": "damage"}])");
    reads_as("You may do 30 more damage. If you do, this Pokémon also does 30 damage to itself.", DamageMod::Plus,
             R"([{"op": "may", "effects": [{"op": "add_damage", "amount": 30}, {"op": "damage_self", "amount": 30}]}, {"op": "damage"}])");
    reads_as("Discard a card from your hand. If you do, draw 2 cards.", DamageMod::None,
             R"([{"op": "discard_from_hand"}, {"op": "if", "condition": {"moved_at_least": 1}, "then": [{"op": "draw", "count": 2}]}])");
    reads_as("If this Pokémon has no damage counters on it, this attack does 150 more damage.", DamageMod::Plus,
             R"([{"op": "if", "condition": {"self_damaged": false}, "then": [{"op": "add_damage", "amount": 150}]}, {"op": "damage"}])");
    reads_as("If you have any {M} Pokémon on your Bench, this attack does 80 more damage.", DamageMod::Plus,
             R"([{"op": "if", "condition": {"bench_has": {"type": "metal"}}, "then": [{"op": "add_damage", "amount": 80}]}, {"op": "damage"}])");
    reads_as("If Durant is on your Bench, this attack does 20 more damage.", DamageMod::Plus,
             R"([{"op": "if", "condition": {"bench_has": {"name": "Durant"}}, "then": [{"op": "add_damage", "amount": 20}]}, {"op": "damage"}])");
    reads_as("If this Pokémon has at least 2 extra Energy attached (in addition to this attack's cost), this attack does 140 more damage.", DamageMod::Plus,
             R"([{"op": "if", "condition": {"extra_energy_at_least": 2}, "then": [{"op": "add_damage", "amount": 140}]}, {"op": "damage"}])");
    reads_as("Flip 3 coins. For each heads, discard an Energy from your opponent's Active Pokémon.", DamageMod::None,
             R"([{"op": "flip_coins", "count": 3}, {"op": "discard_target_energy", "on": "active", "for_each": {"of": "heads"}}])");
    reads_as("Discard a Special Energy from your opponent's Active Pokémon.", DamageMod::None,
             R"([{"op": "discard_target_energy", "on": "active", "filter": {"special": true}}])");
    reads_as("Search your deck for up to 2 Basic Energy cards and attach them to your Benched Pokémon in any way you like. Then, shuffle your deck.", DamageMod::None,
             R"([{"op": "attach_spread", "from": "deck", "shuffle": false, "count": 2, "up_to": true, "on": "bench", "filter": {"kind": "energy", "basic": true}}, {"op": "shuffle_deck"}])");
    const auto first = translate_attack("If you go first, you can use this attack during your first turn. Draw 2 cards.", AttackContext{"Test Attack", DamageMod::None});
    REQUIRE(first.ok);
    CHECK(first.first_turn);
    const auto carmine = translate_card(json::parse(trainer("supporter", "If you go first, you may use this card during your first turn. Discard your hand and draw 5 cards.")).get<pokebot::CardDef>());
    REQUIRE(carmine.unread.empty());
    CHECK(carmine.overlay.value("first_turn", false));
    card_reads_as(trainer("item", "Attach up to 2 Basic {F} Energy cards from your discard pile to your Benched Pokémon in any way you like."),
                  R"({"requires": [{"discard_has": {"kind": "energy", "basic": true, "type": "fighting"}}, {"has_benched": true}],
                      "effects": [{"op": "attach_spread", "from": "discard", "count": 2, "up_to": true, "on": "bench", "filter": {"kind": "energy", "basic": true, "type": "fighting"}}]})");
}

TEST_CASE("the translator reads all of a text or none of it", "[translate]") {
    not_read("Draw a card quickly.");
    not_read("Heal from this Pokémon the same amount of damage you did to your opponent's Active Pokémon.");
    not_read("During your next turn, this Pokémon can't use Another Attack.");
    not_read("This attack does 30 more damage for each heads.", DamageMod::Times);
    not_read("This attack does 30 damage for each heads.", DamageMod::Plus);
    not_read("Draw a card.", DamageMod::Plus);
    not_read("If you do, draw a card.");
    const auto translation = translate_attack("Draw a card. Your opponent reveals their hand.", AttackContext{"Test Attack", DamageMod::None});
    CHECK(translation.unread == "Your opponent reveals their hand");
}

TEST_CASE("Items and Supporters, with what they need to be played", "[translate]") {
    card_reads_as(trainer("item", "You can use this card only if you discard 2 other cards from your hand. Search your deck for a Pokémon, reveal it, and put it into your hand. Then, shuffle your deck."),
                  R"({"requires": [{"hand_besides": 2}], "effects": [{"op": "discard_from_hand", "count": 2}, {"op": "search_deck", "reveal": true, "filter": {"kind": "pokemon"}}]})");
    card_reads_as(trainer("supporter", "Shuffle your hand into your deck. Then, draw 5 cards. If you have exactly 6 Prize cards remaining, draw 7 cards instead."),
                  R"({"effects": [{"op": "shuffle_hand_into_deck"}, {"op": "if", "condition": {"prizes_exactly": 6}, "then": [{"op": "draw", "count": 7}], "else": [{"op": "draw", "count": 5}]}]})");
    card_reads_as(trainer("item", "Put up to 3 in any combination of {G} Pokémon and Basic {G} Energy cards from your discard pile into your hand."),
                  R"({"requires": [{"discard_has": {"any_of": [{"kind": "pokemon", "type": "grass"}, {"kind": "energy", "basic": true, "type": "grass"}]}}],
                      "effects": [{"op": "from_discard", "count": 3, "up_to": true, "filter": {"any_of": [{"kind": "pokemon", "type": "grass"}, {"kind": "energy", "basic": true, "type": "grass"}]}}]})");
    card_reads_as(trainer("item", "Search your deck for a Basic Pokémon and a Stage 1 Pokémon, reveal them, and put them into your hand. Then, shuffle your deck."),
                  R"({"effects": [{"op": "search_deck", "reveal": true, "filter": {"kind": "pokemon", "stage": "basic"}, "shuffle": false},
                                  {"op": "search_deck", "reveal": true, "filter": {"kind": "pokemon", "stage": "stage1"}}]})");
    card_reads_as(trainer("supporter", "Each player shuffles their hand into their deck. Then, you draw 4 cards, and your opponent draws 3 cards."),
                  R"({"effects": [{"op": "shuffle_hand_into_deck", "player": "each"}, {"op": "draw", "count": 4}, {"op": "draw", "count": 3, "player": "opponent"}]})");
    card_reads_as(trainer("supporter", "Switch in 1 of your opponent's Benched Pokémon to the Active Spot."),
                  R"({"requires": [{"opponent_has_benched": true}], "effects": [{"op": "switch_active", "player": "opponent"}]})");
    card_reads_as(trainer("item", "Choose up to 2 of your Benched {F} Pokémon and attach a Basic Energy card from your discard pile to each of them."),
                  R"({"requires": [{"discard_has": {"kind": "energy", "basic": true}}, {"bench_has": {"type": "fighting"}}],
                      "effects": [{"op": "attach_to", "on": "bench", "targets": 2, "up_to_targets": true, "target": {"type": "fighting"}, "filter": {"kind": "energy", "basic": true}}]})");
    card_reads_as(trainer("item", "Flip a coin. If heads, discard an Energy from 1 of your opponent's Pokémon."),
                  R"({"effects": [{"op": "flip_coins"}, {"op": "if", "condition": {"heads_at_least": 1}, "then": [{"op": "discard_target_energy"}]}]})");
}

TEST_CASE("Abilities: once per turn, costs, triggers and passives", "[translate]") {
    card_reads_as(pokemon_with("Quick Draw", "Once during your turn, if this Pokémon is in the Active Spot, you may use this Ability. Draw a card. You can't use more than 1 Quick Draw Ability each turn."),
                  R"({"abilities": {"Quick Draw": {"active_only": true, "one_per_name": true, "effects": [{"op": "draw"}]}}})");
    card_reads_as(pokemon_with("Trade In", "You must discard a card from your hand in order to use this Ability. Once during your turn, you may draw 3 cards."),
                  R"({"abilities": {"Trade In": {"requires": [{"hand_besides": 1}], "effects": [{"op": "discard_from_hand"}, {"op": "draw", "count": 3}]}}})");
    card_reads_as(pokemon_with("Jump In", "When you play this Pokémon from your hand onto your Bench during your turn, you may switch it with your Active Pokémon."),
                  R"({"abilities": {"Jump In": {"trigger": "played_to_bench", "effects": [{"op": "may", "effects": [{"op": "switch_to_active"}]}]}}})");
    card_reads_as(pokemon_with("Light Feet", "Your Basic Pokémon in play have no Retreat Cost."),
                  R"({"abilities": {"Light Feet": {"passive": [{"modify": "retreat_cost", "set": 0, "applies_to": "yours", "filter": {"stage": "basic"}}]}}})");
}

TEST_CASE("Tools, Stadiums and Special Energy", "[translate]") {
    card_reads_as(trainer("tool", "The Pokémon this card is attached to gets +50 HP."), R"({"passive": [{"modify": "max_hp", "add": 50}]})");
    card_reads_as(trainer("tool", "The Retreat Cost of the Pokémon this card is attached to is {C} less."), R"({"passive": [{"modify": "retreat_cost", "add": -1}]})");
    card_reads_as(trainer("stadium", "Once during each player's turn, that player may switch their Active {L} Pokémon with 1 of their Benched {L} Pokémon."),
                  R"({"requires": [{"active_is": {"kind": "pokemon", "type": "lightning"}}, {"bench_has": {"kind": "pokemon", "type": "lightning"}}],
                      "effects": [{"op": "switch_active", "filter": {"kind": "pokemon", "type": "lightning"}}]})");
    card_reads_as(trainer("stadium", "Whenever any player puts a Basic non-{M} Pokémon onto their Bench during their turn, place 3 damage counters on that Pokémon."),
                  R"({"trigger": "put_on_bench", "effects": [{"op": "if", "condition": {"self_is": {"kind": "pokemon", "stage": "basic", "not_type": "metal"}}, "then": [{"op": "counters_on_self", "count": 3}]}]})");
    card_reads_as(json{{"id", "TST-3"}, {"name", "Test Energy"}, {"kind", "energy"}, {"energy_kind", "special"}, {"text", "As long as this card is attached to a Pokémon, it provides {C} Energy. If this card is attached to an Evolution Pokémon, it provides {C}{C} Energy instead."}}
                      .dump(),
                  R"({"provides": ["colorless"], "provides_when": [{"holder": {"stage": "evolution"}, "provides": ["colorless", "colorless"]}]})");
    CHECK_FALSE(translate_card(json::parse(trainer("tool", "Draw a card.")).get<pokebot::CardDef>()).unread.empty());
    CHECK_FALSE(translate_card(json::parse(trainer("item", "Draw a card and win the game.")).get<pokebot::CardDef>()).unread.empty());
}

TEST_CASE("every hand-written card text the translator reads loads the same, and it reads all but the known ones", "[translate]") {
    const std::filesystem::path root = POKEBOT_SOURCE_DIR;
    if (!std::filesystem::exists(root / "data/cards/generated")) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const pokebot::CardDB db = pokebot::CardDB::load(root / "data/cards/generated", root / "data/cards/effects");
    std::string different;
    std::set<std::string> unread;
    int read = 0;
    for (const auto& file : std::filesystem::directory_iterator(root / "data/cards/effects")) {
        std::ifstream in(root / "data/cards/generated" / file.path().filename(), std::ios::binary);
        const pokebot::CardDef card = json::parse(in).get<pokebot::CardDef>();
        const auto translation = translate_card(card);
        read += translation.parts - static_cast<int>(translation.unread.size());
        for (const auto& [part, why] : translation.unread) {
            unread.insert(part);
        }
        pokebot::CardDB translated;
        translated.add(pokebot::merge_effects(json(card), translation.overlay).get<pokebot::CardDef>());
        for (const std::string& part : translation_differences(db.at(card.id), translated.at(card.id), translation.overlay)) {
            different += "\n  " + part + ": " + translation.overlay.dump();
        }
    }
    INFO("different:" << different);
    CHECK(different.empty());
    CHECK(read > 0);
    CHECK(unread == std::set<std::string>{"ASC-39 Psyduck: ability Damp", "CRI-83 Transformation Tome", "MEG-125 Rare Candy", "SCR-131 Area Zero Underdepths",
                                          "SCR-133 Crispin", "SFA-59 Janine's Secret Art", "TWM-95 Munkidori: ability Adrena-Brain"});
}
