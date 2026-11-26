#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "engine/engine.hpp"
#include "engine/invariants/invariants.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using nlohmann::json;
using pokebot::Action;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::DecisionKind;
using pokebot::EnergyType;
using pokebot::GameState;
using pokebot::kActiveSlot;

namespace {

    CardDef with_trigger(CardDef c, const char* trigger, const char* effects) {
        pokebot::Ability ability;
        ability.name = "Reaction";
        ability.text = "test ability";
        ability.trigger = pokebot::trigger_from_string(trigger);
        ability.once_per_turn = std::string(trigger) != "damaged_by_attack";
        ability.effects = json::parse(effects);
        c.abilities = {ability};
        return c;
    }

    CardDB sample_db() {
        using E = EnergyType;
        const auto with = [](const char* name, int damage, const char* effects) {
            auto a = test::attack(name, {E::Colorless}, damage);
            a.effects = json::parse(effects);
            return a;
        };
        CardDef disrupt = test::basic_pokemon("TST-1", "Disruptmon", 200);
        disrupt.attacks = {
            with("Whirl", 0, R"([{"op": "switch_active", "player": "opponent", "chooser": "opponent"}, {"op": "damage_to", "on": "active", "amount": 30}])"),
            with("Refill", 0, R"([{"op": "draw", "until": 7}])"),
            with("Strip", 20, R"([{"op": "discard_tools"}, {"op": "damage"}])"),
            with("Snatch", 0, R"([{"op": "discard_random"}])"),
            with("Mill", 0, R"([{"op": "discard_deck_top", "player": "opponent", "count": 2}])"),
            with("Hit", 30, "[]"),
            with("Payback", 10, R"([{"op": "if", "condition": {"bench_damaged": true}, "then": [{"op": "add_damage", "amount": 50}]}, {"op": "damage"}])"),
        };
        CardDef grown = test::evolution("TST-5", "Grownmon", pokebot::Stage::Stage1, "Babymon", 90);
        CardDef tool;
        tool.id = "TST-7";
        tool.name = "Charm";
        tool.kind = pokebot::CardKind::Trainer;
        tool.trainer_kind = pokebot::TrainerKind::Tool;
        tool.text = "test card";
        tool.passive = json::parse(R"([{"modify": "max_hp", "add": 10}])");

        CardDB db;
        for (const auto& card : {disrupt, test::basic_pokemon("TST-4", "Babymon", 60), tool,
                                 with_trigger(test::basic_pokemon("TST-2", "Spikemon", 100), "damaged_by_attack", R"([{"op": "special_condition", "condition": "poisoned"}])"),
                                 with_trigger(test::basic_pokemon("TST-3", "Barbmon", 100), "damaged_by_attack", R"([{"op": "counters_to", "on": "active", "count": 2}])"),
                                 with_trigger(grown, "evolved", R"([{"op": "may", "effects": [{"op": "draw", "count": 2}]}])")}) {
            db.add(card);
        }
        db.add(test::basic_energy("TST-6", "Psychic Energy", E::Psychic));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 2}, {4, 3}, {4, 4}, {4, 5}, {36, 6}, {4, 7}});

    Action attack(std::uint8_t index) {
        return Action{ActionKind::Attack, pokebot::kNoCard, kActiveSlot, index};
    }

    // Turn 2. `me`: Disruptmon Active with 1 Energy, Disruptmon on the Bench. `them`: Disruptmon Active; Bench 0 Babymon, Bench 1 Spikemon.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 18);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            for (const int p : {me, them}) {
                test::set_active(game, p, "TST-1");
                test::clear_bench(game, p);
            }
            test::attach_from_deck(game, me, kActiveSlot, "TST-6");
            test::add_to_bench(game, me, "TST-1");
            test::add_to_bench(game, them, "TST-4");
            test::add_to_bench(game, them, "TST-2");
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        pokebot::PlayerState& mine() { return game.player(me); }
        pokebot::PlayerState& theirs() { return game.player(them); }
        std::string id_of(const pokebot::PokemonInPlay& pokemon) const { return game.def(pokemon.top()).id; }

        void check_cards() const {
            CHECK(test::cards_accounted_for(game.player(0)) == 60);
            CHECK(test::cards_accounted_for(game.player(1)) == 60);
            CHECK(pokebot::check_invariants(game).empty());
        }
    };

} // namespace

TEST_CASE("switching out the opponent's Active: the opponent chooses, then the new Active takes damage", "[disruption]") {
    Fixture f;
    pokebot::apply(f.game, attack(0));
    REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
    CHECK(f.game.decision().player == f.them);
    CHECK(pokebot::legal_actions(f.game).size() == 2);
    pokebot::apply(f.game, Action{ActionKind::ChoosePokemon, pokebot::kNoCard, pokebot::bench_slot(0), 0});
    CHECK(f.id_of(*f.theirs().active) == "TST-4");
    CHECK(f.theirs().active->damage_counters == 3);
    f.check_cards();
}

TEST_CASE("drawing until the hand has that many cards", "[disruption]") {
    Fixture f;
    const std::size_t before = f.mine().hand.size();
    pokebot::apply(f.game, attack(1));
    CHECK(f.mine().hand.size() == std::max<std::size_t>(before, 7));
    f.check_cards();
}

TEST_CASE("discarding the opponent's Tool before damage, a random card from their hand, and the top of their deck", "[disruption]") {
    {
        Fixture f;
        const pokebot::CardIndex charm = test::take_from_deck(f.game, f.them, "TST-7");
        f.theirs().active->tool = charm;
        pokebot::apply(f.game, attack(2));
        CHECK(f.theirs().active->tool == pokebot::kNoCard);
        CHECK(f.theirs().discard.contains(charm));
        CHECK(f.theirs().active->damage_counters == 2);
        f.check_cards();
    }
    {
        Fixture f;
        const std::size_t discard = f.theirs().discard.size();
        pokebot::apply(f.game, attack(3));
        CHECK(f.theirs().discard.size() == discard + 1);
        f.check_cards();
    }
    {
        Fixture f;
        const std::size_t discard = f.theirs().discard.size();
        pokebot::apply(f.game, attack(4));
        CHECK(f.theirs().discard.size() == discard + 2);
        f.check_cards();
    }
}

TEST_CASE("a condition on damage counters on your Benched Pokemon", "[disruption]") {
    {
        Fixture f;
        pokebot::apply(f.game, attack(6));
        CHECK(f.theirs().active->damage_counters == 1);
    }
    {
        Fixture f;
        f.mine().bench[0].damage_counters = 1;
        pokebot::apply(f.game, attack(6));
        CHECK(f.theirs().active->damage_counters == 6);
        f.check_cards();
    }
}

TEST_CASE("an Active Pokemon damaged by the opponent's attack answers: Poisoned, or damage counters", "[disruption]") {
    {
        Fixture f;
        test::set_active(f.game, f.them, "TST-2");
        pokebot::apply(f.game, attack(5));
        CHECK(f.mine().active->special.poison > 0);
        f.check_cards();
    }
    {
        Fixture f;
        test::set_active(f.game, f.them, "TST-3");
        pokebot::apply(f.game, attack(5));
        CHECK(f.mine().active->damage_counters == 2);
        CHECK(f.theirs().active->damage_counters == 3);
        f.check_cards();
    }
}

TEST_CASE("evolving from the hand can trigger the new Pokemon's Ability", "[disruption]") {
    Fixture f;
    test::end_turns(f.game, 2);
    REQUIRE(f.game.current_player == f.me);
    test::add_to_bench(f.game, f.me, "TST-4");
    test::put_in_hand(f.game, f.me, "TST-5");
    const std::size_t hand = f.mine().hand.size();
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::Evolve, "TST-5"));
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseYesNo);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::AnswerYes));
    CHECK(f.mine().hand.size() == hand - 1 + 2);
    CHECK(f.game.decision().kind == DecisionKind::MainPhase);
    f.check_cards();
}
