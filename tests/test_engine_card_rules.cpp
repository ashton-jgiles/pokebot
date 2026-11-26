#include <filesystem>

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

    using E = EnergyType;

    CardDef trainer(const char* id, const char* name, pokebot::TrainerKind kind, const char* field, const char* value) {
        CardDef c;
        c.id = id;
        c.name = name;
        c.kind = pokebot::CardKind::Trainer;
        c.trainer_kind = kind;
        c.text = "test card";
        if (std::string(field) == "passive") {
            c.passive = json::parse(value);
        } else {
            c.effects = json::parse(value);
        }
        return c;
    }

    CardDef with_ability(CardDef c, const char* field, const char* value) {
        pokebot::Ability ability;
        ability.name = "Power";
        ability.text = "test ability";
        if (std::string(field) == "passive") {
            ability.passive = json::parse(value);
        } else {
            ability.effects = json::parse(value);
        }
        c.abilities = {ability};
        return c;
    }

    CardDef special_energy(const char* id, const char* name) {
        CardDef c;
        c.id = id;
        c.name = name;
        c.kind = pokebot::CardKind::Energy;
        c.energy_kind = pokebot::EnergyKind::Special;
        c.provides = {E::Colorless};
        c.text = "test card";
        return c;
    }

    CardDB sample_db() {
        const auto with = [](const char* name, std::vector<E> cost, int damage, const char* effects) {
            auto a = test::attack(name, std::move(cost), damage);
            a.effects = json::parse(effects);
            return a;
        };
        CardDef attacker = test::basic_pokemon("TST-1", "Attacker", 200);
        attacker.attacks = {
            with("Hit", {E::Colorless}, 50, "[]"),
            with("Spread", {E::Colorless}, 0, R"([{"op": "place_counters", "count": 2, "on": "bench"}])"),
            with("Hand", {E::Colorless}, 0, R"([{"op": "counters_to", "on": "active", "per": 1, "for_each": {"of": "hand"}}])"),
            with("Leaf", {E::Grass, E::Grass}, 30, "[]"),
            with("Sleep", {E::Colorless}, 0, R"([{"op": "special_condition", "condition": "asleep"}])"),
            with("Spray", {E::Colorless}, 0, R"([{"op": "damage_to", "each": true, "on": "bench", "amount": 20}])"),
        };
        CardDef baby = test::basic_pokemon("TST-2", "Grassbaby", 60);
        baby.types = {E::Grass};
        CardDef grown = test::evolution("TST-3", "Grassgrown", pokebot::Stage::Stage1, "Grassbaby", 90);
        grown.types = {E::Grass};
        CardDef grower = with_ability(test::basic_pokemon("TST-4", "Grower", 100), "passive",
                                      R"([{"modify": "energy_provides", "applies_to": "yours", "energy": {"kind": "energy", "basic": true, "type": "grass"}, "type": "grass", "set": 2}])");
        grower.types = {E::Grass};
        CardDef locker = with_ability(test::basic_pokemon("TST-10", "Locker", 90), "passive",
                                      R"([{"modify": "no_abilities", "applies_to": "opponents", "if_active": true, "target_active": true, "except": "Power"}])");
        locker.types = {E::Psychic};
        CardDef drawer = with_ability(test::basic_pokemon("TST-11", "Drawer", 60), "effects", R"([{"op": "draw"}])");
        drawer.abilities[0].name = "Draw";
        CardDef ace = trainer("TST-14", "Ace", pokebot::TrainerKind::Item, "effects", R"([{"op": "draw"}])");
        ace.rules.ace_spec = true;
        CardDef nullifier = with_ability(test::basic_pokemon("TST-15", "Nullifier", 100), "passive", R"([{"modify": "no_ace_spec", "applies_to": "opponents", "if_holder_tool": true}])");
        CardDef festival = with_ability(test::basic_pokemon("TST-21", "Festivalmon", 100), "passive", R"([{"modify": "attacks_per_turn", "set": 2, "if_stadium": "Festival Grounds"}])");
        festival.attacks = {with("Hit", {E::Colorless}, 10, "[]")};
        CardDef shield = with_ability(test::basic_pokemon("TST-22", "Shieldmon", 100), "passive",
                                      R"([{"modify": "prevent_damage", "applies_to": "yours", "target_benched": true}, {"modify": "prevent_effects", "applies_to": "yours", "target_benched": true}])");
        CardDef mega = test::basic_pokemon("TST-24", "Megamon ex", 300);
        mega.rules.ex = true;
        mega.rules.mega = true;
        CardDef mist = special_energy("TST-19", "Mist");
        mist.passive = json::parse(R"([{"modify": "prevent_effects"}])");
        CardDef enrich = special_energy("TST-20", "Enrich");
        enrich.trigger = pokebot::Trigger::AttachedFromHand;
        enrich.effects = json::parse(R"([{"op": "draw", "count": 2}])");
        CardDef supporter_kieran = trainer("TST-17", "Kieran", pokebot::TrainerKind::Supporter, "effects",
                                           R"([{"op": "choose_one", "options": [[{"op": "switch_active"}], [{"op": "draw"}]]}])");
        CardDef gladion = trainer("TST-18", "Gladion", pokebot::TrainerKind::Supporter, "effects",
                                  R"([{"op": "lasting", "effect": "damage_bonus", "on": "player", "amount": 80, "until": "this_turn", "attacker": {"rule_box": false}}])");
        gladion.requirements = json::parse(R"([{"hand_besides_at_most": 0}])");
        CardDef wally = trainer("TST-23", "Wally", pokebot::TrainerKind::Supporter, "effects",
                                R"([{"op": "heal_to", "all": true, "target": {"mega": true}}, {"op": "if", "condition": {"moved_at_least": 1}, "then": [{"op": "target_energy_to_hand"}]}])");
        CardDef bugs = trainer("TST-25", "Bugs", pokebot::TrainerKind::Item, "effects",
                               R"([{"op": "look_at_top", "count": 7, "take": 2, "up_to": true, "rest": "shuffle", "filter": {"kind": "energy", "basic": true, "type": "grass"}}])");

        CardDB db;
        for (const auto& card : {attacker, baby, grown, grower, locker, drawer, ace, nullifier, festival, shield, mega, mist, enrich, supporter_kieran, gladion, wally, bugs,
                                 trainer("TST-6", "Forest", pokebot::TrainerKind::Stadium, "passive",
                                         R"([{"modify": "evolve_same_turn", "applies_to": "all", "filter": {"type": "grass"}, "evolution": {"type": "grass"}}])"),
                                 trainer("TST-7", "Festival Grounds", pokebot::TrainerKind::Stadium, "passive",
                                         R"([{"modify": "special_condition_immune", "applies_to": "all", "if_energy_attached": true}])"),
                                 trainer("TST-8", "Cage", pokebot::TrainerKind::Stadium, "passive", R"([{"modify": "prevent_bench_counters"}])"),
                                 trainer("TST-9", "Tower", pokebot::TrainerKind::Stadium, "passive", R"([{"modify": "no_abilities", "applies_to": "all", "filter": {"type": "colorless"}}])"),
                                 trainer("TST-12", "Pearl", pokebot::TrainerKind::Tool, "passive", R"([{"modify": "prizes_given", "add": -1, "filter": {"name_prefix": "Lil"}}])"),
                                 trainer("TST-16", "Band", pokebot::TrainerKind::Tool, "passive", R"([{"modify": "max_hp", "add": 10}])"),
                                 test::basic_pokemon("TST-13", "Lil Target", 50)}) {
            db.add(card);
        }
        db.add(test::basic_energy("TST-5", "Grass Energy", E::Grass));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {2, 2}, {2, 3}, {2, 4}, {2, 6}, {2, 7}, {2, 8}, {2, 9}, {2, 10}, {4, 11}, {2, 12}, {2, 13}, {1, 14}, {2, 15}, {2, 16}, {2, 17}, {2, 18}, {2, 19}, {2, 20}, {2, 21}, {2, 22}, {2, 23}, {2, 24}, {2, 25}, {9, 5}});

    Action attack(std::uint8_t index) {
        return Action{ActionKind::Attack, pokebot::kNoCard, kActiveSlot, index};
    }

    // Turn 2. Both players: Attacker Active with a Grass Energy, nothing on the Bench.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 20);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 1);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            for (const int p : {me, them}) {
                active(p, "TST-1");
                test::clear_bench(game, p);
                attach(p, kActiveSlot, "TST-5");
            }
        }

        pokebot::CardIndex fetch(int player, const std::string& id) {
            auto& p = game.player(player);
            for (std::size_t i = 0; i < p.deck.size(); ++i) {
                if (game.def(p.deck[i]).id == id) {
                    const pokebot::CardIndex card = p.deck[i];
                    p.deck.erase_at(i);
                    return card;
                }
            }
            for (std::size_t i = 0; i < p.hand.size(); ++i) {
                if (game.def(p.hand[i]).id == id) {
                    const pokebot::CardIndex card = p.hand[i];
                    p.hand.erase_at(i);
                    return card;
                }
            }
            for (std::size_t i = 0; i < p.prizes.size(); ++i) {
                if (game.def(p.prizes[i]).id == id) {
                    const pokebot::CardIndex card = p.prizes[i];
                    p.prizes[i] = p.deck.pop_back();
                    return card;
                }
            }
            FAIL("no " << id << " left to take");
            return pokebot::kNoCard;
        }

        void active(int player, const std::string& id) {
            auto& p = game.player(player);
            if (p.active) {
                for (const pokebot::CardIndex card : p.active->stack) {
                    p.deck.push_back(card);
                }
                for (const pokebot::CardIndex card : p.active->energy) {
                    p.deck.push_back(card);
                }
                if (p.active->tool != pokebot::kNoCard) {
                    p.deck.push_back(p.active->tool);
                }
            }
            p.active = pokebot::detail::new_pokemon(fetch(player, id), 0);
        }

        void bench(int player, const std::string& id) { game.player(player).bench.push_back(pokebot::detail::new_pokemon(fetch(player, id), 0)); }

        void attach(int player, pokebot::Slot slot, const std::string& id) { pokebot::detail::pokemon_at(game.player(player), slot).energy.push_back(fetch(player, id)); }

        void hand(int player, const std::string& id) { game.player(player).hand.push_back(fetch(player, id)); }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        pokebot::PlayerState& mine() { return game.player(me); }
        pokebot::PlayerState& theirs() { return game.player(them); }

        bool offers(ActionKind kind, const std::string& card, int amount = -1) const {
            for (const Action& action : pokebot::legal_actions(game)) {
                if (action.kind == kind && (card.empty() || (action.card != pokebot::kNoCard && game.def(action.card).id == card)) && (amount < 0 || action.amount == amount)) {
                    return true;
                }
            }
            return false;
        }

        void check_cards() const {
            CHECK(test::cards_accounted_for(game.player(0)) == 60);
            CHECK(test::cards_accounted_for(game.player(1)) == 60);
            CHECK(pokebot::check_invariants(game).empty());
        }
    };

} // namespace

TEST_CASE("a Special Energy's passive works on its Pokemon, and attaching one from the hand can trigger it", "[card_rules]") {
    {
        Fixture f;
        f.attach(f.them, kActiveSlot, "TST-19");
        pokebot::apply(f.game, attack(4));
        CHECK_FALSE(f.theirs().active->special.asleep);
        f.check_cards();
    }
    {
        Fixture f;
        f.hand(f.me, "TST-20");
        const std::size_t hand = f.mine().hand.size();
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::AttachEnergy, "TST-20", kActiveSlot));
        CHECK(f.mine().hand.size() == hand - 1 + 2);
        f.check_cards();
    }
}

TEST_CASE("Abilities are lost to a Stadium, or on the Active Pokemon facing one that takes them", "[card_rules]") {
    {
        Fixture f;
        f.active(f.me, "TST-11");
        CHECK(f.offers(ActionKind::UseAbility, "TST-11"));
        f.mine().stadium = f.fetch(f.me, "TST-9");
        CHECK_FALSE(f.offers(ActionKind::UseAbility, "TST-11"));
    }
    {
        Fixture f;
        f.active(f.me, "TST-11");
        f.bench(f.me, "TST-11");
        f.active(f.them, "TST-10");
        int offered = 0;
        for (const Action& action : pokebot::legal_actions(f.game)) {
            offered += action.kind == ActionKind::UseAbility ? 1 : 0;
            if (action.kind == ActionKind::UseAbility) {
                CHECK(action.target != kActiveSlot);
            }
        }
        CHECK(offered == 1);
        f.check_cards();
    }
}

TEST_CASE("a passive can make each Basic Energy provide two", "[card_rules]") {
    Fixture f;
    CHECK_FALSE(f.offers(ActionKind::Attack, "", 3));
    f.bench(f.me, "TST-4");
    CHECK(f.offers(ActionKind::Attack, "", 3));
}

TEST_CASE("a Stadium can let Pokemon evolve the turn they're played", "[card_rules]") {
    Fixture f;
    test::end_turns(f.game, 2);
    REQUIRE(f.game.current_player == f.me);
    f.bench(f.me, "TST-2");
    f.mine().bench.back().turn_played = f.game.turn;
    f.hand(f.me, "TST-3");
    CHECK_FALSE(f.offers(ActionKind::Evolve, "TST-3"));
    f.mine().stadium = f.fetch(f.me, "TST-6");
    CHECK(f.offers(ActionKind::Evolve, "TST-3"));
}

TEST_CASE("Pokemon with Energy are immune to Special Conditions under a Stadium", "[card_rules]") {
    {
        Fixture f;
        f.mine().stadium = f.fetch(f.me, "TST-7");
        pokebot::apply(f.game, attack(4));
        CHECK_FALSE(f.theirs().active->special.asleep);
    }
    {
        Fixture f;
        f.mine().stadium = f.fetch(f.me, "TST-7");
        for (const pokebot::CardIndex card : f.theirs().active->energy) {
            f.theirs().deck.push_back(card);
        }
        f.theirs().active->energy.clear();
        pokebot::apply(f.game, attack(4));
        CHECK(f.theirs().active->special.asleep);
        f.check_cards();
    }
}

TEST_CASE("a Stadium can stop damage counters being put on Benched Pokemon by the opponent", "[card_rules]") {
    {
        Fixture f;
        f.bench(f.them, "TST-13");
        pokebot::apply(f.game, attack(1));
        CHECK(f.game.decision().kind == DecisionKind::ChoosePokemon);
    }
    {
        Fixture f;
        f.bench(f.them, "TST-13");
        f.mine().stadium = f.fetch(f.me, "TST-8");
        pokebot::apply(f.game, attack(1));
        CHECK(f.game.decision().kind != DecisionKind::ChoosePokemon);
        CHECK(f.theirs().bench[0].damage_counters == 0);
        f.check_cards();
    }
}

TEST_CASE("an ACE SPEC lock, one fewer Prize card, and attacking twice", "[card_rules]") {
    {
        Fixture f;
        f.hand(f.me, "TST-14");
        f.active(f.them, "TST-15");
        CHECK(f.offers(ActionKind::PlayTrainer, "TST-14"));
        f.theirs().active->tool = f.fetch(f.them, "TST-16");
        CHECK_FALSE(f.offers(ActionKind::PlayTrainer, "TST-14"));
    }
    {
        Fixture f;
        f.active(f.them, "TST-13");
        f.bench(f.them, "TST-1");
        f.theirs().active->tool = f.fetch(f.them, "TST-12");
        pokebot::apply(f.game, attack(0));
        CHECK(f.mine().prizes.size() == 6);
        f.check_cards();
    }
    {
        Fixture f;
        f.active(f.them, "TST-13");
        f.bench(f.them, "TST-1");
        pokebot::apply(f.game, attack(0));
        CHECK(f.mine().prizes.size() == 5);
    }
    {
        Fixture f;
        f.active(f.me, "TST-21");
        f.attach(f.me, kActiveSlot, "TST-5");
        f.mine().stadium = f.fetch(f.me, "TST-7");
        pokebot::apply(f.game, attack(0));
        CHECK(f.game.decision().kind == DecisionKind::ExtraAttack);
        CHECK(f.game.current_player == f.me);
    }
    {
        Fixture f;
        f.active(f.me, "TST-21");
        f.attach(f.me, kActiveSlot, "TST-5");
        pokebot::apply(f.game, attack(0));
        CHECK(f.game.current_player == f.them);
    }
}

TEST_CASE("counters for each card in hand, choosing one of two, the last card in hand, healing a Mega and taking its Energy, and looking at the top", "[card_rules]") {
    {
        Fixture f;
        const std::size_t hand = f.mine().hand.size();
        pokebot::apply(f.game, attack(2));
        CHECK(f.theirs().active->damage_counters == hand);
    }
    {
        Fixture f;
        f.bench(f.me, "TST-1");
        f.hand(f.me, "TST-17");
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-17"));
        REQUIRE(f.game.decision().kind == DecisionKind::ChooseYesNo);
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::AnswerYes));
        CHECK(f.game.decision().kind == DecisionKind::ChoosePokemon);
    }
    {
        Fixture f;
        auto& p = f.mine();
        while (!p.hand.empty()) {
            p.deck.push_back(p.hand.pop_back());
        }
        f.hand(f.me, "TST-18");
        CHECK(f.offers(ActionKind::PlayTrainer, "TST-18"));
        f.hand(f.me, "TST-5");
        CHECK_FALSE(f.offers(ActionKind::PlayTrainer, "TST-18"));
    }
    {
        Fixture f;
        f.bench(f.me, "TST-24");
        f.mine().bench[0].damage_counters = 3;
        f.attach(f.me, pokebot::bench_slot(0), "TST-5");
        f.hand(f.me, "TST-23");
        const std::size_t hand = f.mine().hand.size();
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-23"));
        REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
        pokebot::apply(f.game, pokebot::legal_actions(f.game)[0]);
        CHECK(f.mine().bench[0].damage_counters == 0);
        CHECK(f.mine().bench[0].energy.empty());
        CHECK(f.mine().hand.size() == hand);
        f.check_cards();
    }
    {
        Fixture f;
        f.hand(f.me, "TST-25");
        const std::size_t hand = f.mine().hand.size();
        pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-25"));
        if (f.game.decision().kind == DecisionKind::ChooseCards) {
            pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseDone));
        }
        CHECK(f.mine().hand.size() == hand - 1);
        f.check_cards();
    }
}

TEST_CASE("an Ability can protect your Benched Pokemon from damage", "[card_rules]") {
    Fixture f;
    f.bench(f.them, "TST-22");
    f.bench(f.them, "TST-13");
    pokebot::apply(f.game, attack(5));
    CHECK(f.theirs().bench[0].damage_counters == 0);
    CHECK(f.theirs().bench[1].damage_counters == 0);
    f.check_cards();
}

TEST_CASE("the four incoming decks have every effect, and random games with them keep every card accounted for", "[card_rules]") {
    const std::filesystem::path root = POKEBOT_SOURCE_DIR;
    if (!std::filesystem::exists(root / "data/cards/generated")) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const CardDB db = CardDB::load(root / "data/cards/generated", root / "data/cards/effects");
    std::vector<pokebot::Decklist> decks;
    for (const char* name : {"alakazam", "festival-lead", "mega-lucario", "mega-venasaur"}) {
        decks.push_back(pokebot::load_decklist(root / "data/decks/incoming" / (std::string(name) + ".txt")));
        for (const auto& entry : decks.back().entries) {
            INFO(name << ": " << pokebot::card_id(entry));
            CHECK(pokebot::missing_effects(db.at(pokebot::card_id(entry))).empty());
        }
    }
    pokebot::Rng chooser(2026);
    for (std::uint64_t seed = 0; seed < 64; ++seed) {
        const CardTable table(db, decks[seed % decks.size()], decks[(seed / decks.size()) % decks.size()]);
        GameState game = pokebot::new_game(table, seed);
        for (int step = 0; step < 20000 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            REQUIRE_FALSE(actions.empty());
            pokebot::apply(game, actions[chooser.below(static_cast<std::uint32_t>(actions.size()))]);
            if (step % 50 == 0) {
                REQUIRE(pokebot::check_invariants(game).empty());
            }
        }
        CHECK(game.is_over());
        CHECK(test::cards_accounted_for(game.player(0)) == 60);
        CHECK(test::cards_accounted_for(game.player(1)) == 60);
        CHECK(pokebot::check_invariants(game).empty());
    }
}
