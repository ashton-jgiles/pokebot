#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "agents/planner/action_class.hpp"
#include "agents/evaluation/evaluation.hpp"
#include "agents/random/random_agent.hpp"
#include "agents/world.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "engine/observation/observation.hpp"
#include "harness/match.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using nlohmann::json;
using pokebot::Action;
using pokebot::ActionClass;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::GameState;
using pokebot::kActiveSlot;
using pokebot::try_action;

namespace {

    CardDef attacker(const char* id, const char* name, const char* attack, const char* effects) {
        CardDef c = test::basic_pokemon(id, name);
        pokebot::Attack a;
        a.name = attack;
        a.cost = {pokebot::EnergyType::Colorless};
        a.damage = 30;
        a.effects = json::parse(effects);
        c.attacks.push_back(a);
        return c;
    }

    CardDef item(const char* id, const char* name, const char* effects) {
        CardDef c;
        c.id = id;
        c.name = name;
        c.kind = pokebot::CardKind::Trainer;
        c.trainer_kind = pokebot::TrainerKind::Item;
        c.text = "test card";
        c.effects = json::parse(effects);
        return c;
    }

    // TST-1 and TST-3 Tacklers (60 HP, Tackle: 30 for one Energy), TST-2 Energy, TST-4 Flipper (Flip
    // Tackle: flip a coin, then 30), TST-5 Big Tackler (Stage 1 from Tackler), TST-6 Draw Two, TST-7 Seek
    // (search the deck for a Pokemon).
    CardDB sample_db() {
        CardDB db;
        db.add(attacker("TST-1", "Tackler", "Tackle", "[]"));
        db.add(test::basic_energy("TST-2", "Darkness Energy", pokebot::EnergyType::Darkness));
        db.add(attacker("TST-3", "Tackler B", "Tackle", "[]"));
        db.add(attacker("TST-4", "Flipper", "Flip Tackle", R"([{"op": "flip_coins"}, {"op": "damage"}])"));
        db.add(test::evolution("TST-5", "Big Tackler", pokebot::Stage::Stage1, "Tackler"));
        db.add(item("TST-6", "Draw Two", R"([{"op": "draw", "count": 2}])"));
        db.add(item("TST-7", "Seek", R"([{"op": "search_deck", "filter": {"kind": "pokemon"}}])"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 3}, {4, 4}, {4, 5}, {4, 6}, {4, 7}, {36, 2}});

    // Turn 4: each player has a Tackler Active (played at the start, so it can evolve) and one on the Bench.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 2);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 3);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            for (const int p : {me, them}) {
                test::clear_bench(game, p);
                test::set_active(game, p, "TST-1");
                test::add_to_bench(game, p, "TST-3");
                pokebot::detail::shuffle_deck(game, p); // cards put back on top would still be known
            }
            seen();
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        // Records what each player can see, after cards were moved by hand.
        void seen() { pokebot::detail::note_visible(game); }

        void give(const char* id) {
            test::put_in_hand(game, me, id);
            seen();
        }

        void power_up() {
            test::attach_from_deck(game, me, kActiveSlot, "TST-2");
            seen();
        }

        ActionClass classify(ActionKind kind, const std::string& id = "") {
            return try_action(game, me, test::find_action(game, kind, id)).kind;
        }
    };

} // namespace

TEST_CASE("attaching Energy, benching, evolving and retreating are deterministic", "[planner]") {
    Fixture f;
    f.give("TST-2");
    CHECK(f.classify(ActionKind::AttachEnergy) == ActionClass::Deterministic);
    f.give("TST-3");
    CHECK(f.classify(ActionKind::PlayBasic, "TST-3") == ActionClass::Deterministic);
    f.give("TST-5");
    CHECK(f.classify(ActionKind::Evolve, "TST-5") == ActionClass::Deterministic);
    f.power_up();
    CHECK(f.classify(ActionKind::Retreat) == ActionClass::Deterministic);
}

TEST_CASE("ending the turn and attacking are final, and so is a Knock Out", "[planner]") {
    Fixture f;
    CHECK(f.classify(ActionKind::EndTurn) == ActionClass::Final);
    f.power_up();
    CHECK(f.classify(ActionKind::Attack) == ActionClass::Final);

    f.game.player(f.them).active->damage_counters = 3; // one Tackle from a Knock Out
    const auto tried = try_action(f.game, f.me, test::find_action(f.game, ActionKind::Attack));
    CHECK(tried.kind == ActionClass::Final);
    CHECK(tried.after.player(f.me).prizes.size() == 5);
    CHECK(tried.after.rng == f.game.rng); // taking a Prize card needs no chance
}

TEST_CASE("drawing, searching and flipping coins are uncertain", "[planner]") {
    Fixture f;
    f.give("TST-6");
    CHECK(f.classify(ActionKind::PlayTrainer, "TST-6") == ActionClass::Uncertain);
    f.give("TST-7");
    CHECK(f.classify(ActionKind::PlayTrainer, "TST-7") == ActionClass::Uncertain); // the search shows you your deck

    test::set_active(f.game, f.me, "TST-4");
    f.power_up();
    CHECK(f.classify(ActionKind::Attack) == ActionClass::Uncertain);

    f.game.player(f.me).active->special.asleep = true; // a coin flip at Pokemon Checkup
    CHECK(f.classify(ActionKind::EndTurn) == ActionClass::Uncertain);
}

TEST_CASE("on the real decks, a deterministic action gives the same result in any world, and a final one the same value", "[planner]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::size_t n = real->decks.size();
    pokebot::Rng rng(8);
    int counts[3] = {0, 0, 0};
    for (std::uint64_t seed = 0; seed < 16; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table = CardTable(real->db, real->decks[i % n], real->decks[(i / n) % n]).shuffled(~seed);
        pokebot::BiasedRandomAgent agent(seed);
        GameState game = pokebot::new_game(table, seed);
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            if (game.turn > 0 && step % 3 == 0) {
                const int player = game.decision().player;
                const auto view = pokebot::observe(game, player);
                const pokebot::World one(view, rng);
                const pokebot::World two(view, rng);
                for (const Action& action : actions) {
                    INFO("seed " << seed << ", step " << step << ", action kind " << static_cast<int>(action.kind));
                    const auto in_one = try_action(one.game(), player, action);
                    const auto in_two = try_action(two.game(), player, action);
                    REQUIRE(in_one.kind == in_two.kind);
                    ++counts[static_cast<int>(in_one.kind)];
                    if (in_one.kind == ActionClass::Deterministic) {
                        REQUIRE(pokebot::observe(in_one.after, player) == pokebot::observe(in_two.after, player));
                    } else if (in_one.kind == ActionClass::Final) {
                        REQUIRE(in_one.after.result == in_two.after.result);
                        REQUIRE(pokebot::evaluate(in_one.after, player) == pokebot::evaluate(in_two.after, player));
                    }
                }
            }
            pokebot::apply_legal(game, pokebot::ask(agent, game, actions));
        }
    }
    INFO(counts[0] << " deterministic, " << counts[1] << " final, " << counts[2] << " uncertain");
    CHECK(counts[0] > 100);
    CHECK(counts[1] > 100);
    CHECK(counts[2] > 100);
}
