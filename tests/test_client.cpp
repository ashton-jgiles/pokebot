#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/human_agent.hpp"
#include "agents/random/random_agent.hpp"
#include "engine/describe/describe.hpp"
#include "engine/engine.hpp"
#include "engine/observation/observation.hpp"
#include "harness/match.hpp"
#include "harness/replay.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using nlohmann::json;
using pokebot::Action;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::DecisionKind;
using pokebot::GameState;
using pokebot::HumanAgent;
using pokebot::Observation;
using pokebot::observe;

namespace {

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

    // TST-1, TST-8 to TST-10 Basics, TST-2 Energy, TST-3 Seek (search, revealed), TST-5 Boss (switch in an opponent's Benched Pokemon).
    CardDB sample_db() {
        CardDB db;
        db.add(test::basic_pokemon("TST-1", "Basicmon"));
        db.add(test::basic_energy("TST-2", "Darkness Energy", pokebot::EnergyType::Darkness));
        db.add(item("TST-3", "Seek", R"([{"op": "search_deck", "reveal": true, "filter": {"kind": "pokemon"}}])"));
        db.add(item("TST-5", "Boss", R"([{"op": "switch_active", "player": "opponent"}])"));
        db.add(test::basic_pokemon("TST-8", "Plainmon"));
        db.add(test::basic_pokemon("TST-9", "Othermon"));
        db.add(test::basic_pokemon("TST-10", "Lastmon"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 8}, {4, 9}, {4, 10}, {4, 3}, {4, 5}, {36, 2}});

    // Turn 4, in the main phase of `me`, with a choice of several actions.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 5);
        int me = 0;
        int them = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 3);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            test::put_in_hand(game, me, "TST-3");
            pokebot::apply(game, Action{ActionKind::EndTurn, pokebot::kNoCard, pokebot::kActiveSlot, 0});
            pokebot::apply(game, Action{ActionKind::EndTurn, pokebot::kNoCard, pokebot::kActiveSlot, 0}); // back to me, the hand now known
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        Observation view() const { return observe(game, me); }
    };

    bool contains(const std::string& text, const std::string& part) {
        return text.find(part) != std::string::npos;
    }

    std::size_t count(const std::string& text, const std::string& part) {
        std::size_t n = 0;
        for (std::size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + 1)) {
            ++n;
        }
        return n;
    }

} // namespace

TEST_CASE("the human agent takes the numbered choice that's typed", "[client]") {
    Fixture f;
    const auto actions = pokebot::legal_actions(f.game);
    REQUIRE(actions.size() >= 3);
    std::istringstream in("2\n");
    std::ostringstream out;
    HumanAgent you(in, out);
    CHECK(you.choose(f.view(), actions) == actions[1]);
    CHECK(contains(out.str(), "main phase:"));
    CHECK(contains(out.str(), " 1. " + pokebot::describe(f.view(), actions[0])));
    CHECK(contains(out.str(), "You (player " + std::to_string(f.me)));
}

TEST_CASE("the human agent asks again until a choice is typed", "[client]") {
    Fixture f;
    const auto actions = pokebot::legal_actions(f.game);
    std::istringstream in("attack\n0\n999\n\n1\n");
    std::ostringstream out;
    HumanAgent you(in, out);
    CHECK(you.choose(f.view(), actions) == actions[0]);
    CHECK(count(out.str(), "Type a number from 1 to " + std::to_string(actions.size())) == 4);
}

TEST_CASE("b shows the board again, and q or the end of the input quits", "[client]") {
    Fixture f;
    const auto actions = pokebot::legal_actions(f.game);
    {
        std::istringstream in("b\n1\n");
        std::ostringstream out;
        HumanAgent you(in, out);
        you.choose(f.view(), actions);
        CHECK(count(out.str(), "You (player ") == 2);
    }
    {
        std::istringstream in("q\n");
        std::ostringstream out;
        HumanAgent you(in, out);
        CHECK_THROWS_AS(you.choose(f.view(), actions), pokebot::QuitGame);
    }
    {
        std::istringstream in("");
        std::ostringstream out;
        HumanAgent you(in, out);
        CHECK_THROWS_AS(you.choose(f.view(), actions), pokebot::QuitGame);
    }
}

TEST_CASE("the only choice is taken without asking", "[client]") {
    Fixture f;
    const std::vector<Action> one{pokebot::legal_actions(f.game).back()};
    std::istringstream in("");
    std::ostringstream out;
    HumanAgent you(in, out);
    CHECK(you.choose(f.view(), one) == one.front());
    CHECK(contains(out.str(), "(the only choice)"));
}

TEST_CASE("a choice partway through an effect doesn't show the board again", "[client]") {
    Fixture f;
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-3"));
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    CHECK(pokebot::describe_decision(f.view()) == "choose cards for an effect (Seek)");

    std::istringstream in("1\n1\n");
    std::ostringstream out;
    HumanAgent you(in, out);
    you.choose(f.view(), pokebot::legal_actions(f.game)); // the first choice this turn shows the board
    you.choose(f.view(), pokebot::legal_actions(f.game));
    CHECK(count(out.str(), "You (player ") == 1);
}

TEST_CASE("a person can play a whole game through play_game", "[client]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    std::string typed;
    for (int i = 0; i < 20000; ++i) {
        typed += "1\n";
    }
    std::istringstream in(typed);
    std::ostringstream out;
    HumanAgent you(in, out);
    pokebot::RandomAgent other(3);

    int shown = 0;
    pokebot::MatchOptions options;
    options.on_action = [&](const GameState& before, const Action&, const GameState& after) {
        CHECK(after.turn >= before.turn);
        ++shown;
    };
    const pokebot::GameRecord record = pokebot::play_game(table, 3, you, other, options);
    CHECK(record.result != pokebot::GameResult::Ongoing);
    CHECK(shown == record.actions);
    CHECK(contains(out.str(), "main phase:"));
    CHECK_FALSE(contains(out.str(), "Opponent (player 0)"));
}

TEST_CASE("an action described from an observation reads the same as from the game", "[client][describe]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    int compared = 0;
    for (std::uint64_t seed = 0; seed < 30; ++seed) {
        GameState game = pokebot::new_game(table, seed);
        pokebot::BiasedRandomAgent agent(seed);
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            const Observation view = observe(game, game.decision().player);
            for (const Action& action : actions) {
                INFO("seed " << seed << ", step " << step);
                CHECK(pokebot::describe(view, action) == pokebot::describe(game, action));
                ++compared;
            }
            pokebot::apply_legal(game, pokebot::ask(agent, game, actions));
        }
    }
    CHECK(compared > 1000);
}

TEST_CASE("on the real decks, an action described from an observation reads the same as from the game", "[client][describe]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::size_t n = real->decks.size();
    int different = 0;
    std::string first_difference;
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table = CardTable(real->db, real->decks[i % n], real->decks[(i / n) % n]).shuffled(~seed);
        GameState game = pokebot::new_game(table, seed);
        pokebot::BiasedRandomAgent agent(seed);
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            const Observation view = observe(game, game.decision().player);
            for (const Action& action : actions) {
                const std::string seen = pokebot::describe(view, action);
                const std::string real_text = pokebot::describe(game, action);
                if (seen != real_text) {
                    if (different == 0) {
                        first_difference = "seed " + std::to_string(seed) + ", step " + std::to_string(step) + ": '" + seen + "' but '" + real_text + "'";
                    }
                    ++different;
                }
            }
            pokebot::apply_legal(game, pokebot::ask(agent, game, actions));
        }
    }
    INFO(first_difference);
    CHECK(different == 0);
}

TEST_CASE("choosing one of the opponent's Pokemon says so", "[client][describe]") {
    Fixture f;
    for (const char* id : {"TST-1", "TST-8", "TST-9", "TST-10"}) {
        if (test::has_card(f.game, f.them, id)) {
            test::add_to_bench(f.game, f.them, id);
            break;
        }
    }
    REQUIRE_FALSE(f.game.player(f.them).bench.empty());
    test::put_in_hand(f.game, f.me, "TST-5");
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-5"));
    REQUIRE(f.game.decision().kind == DecisionKind::ChoosePokemon);
    CHECK(pokebot::choice_owner(f.game) == f.them);
    CHECK(f.view().choice_owner == f.them);

    const Action action = test::find_action(f.game, ActionKind::ChoosePokemon);
    const std::string name = f.game.def(f.game.player(f.them).bench[action.target - 1u].top()).name;
    const std::string expected = "choose the opponent's Bench " + std::to_string(action.target) + " " + name;
    CHECK(pokebot::describe(f.game, action) == expected);
    CHECK(pokebot::describe(f.view(), action) == expected);
}

TEST_CASE("print_replay can stop before each turn", "[client][replay]") {
    const CardDB db = sample_db();
    pokebot::Replay replay;
    replay.seed = 4;
    replay.players[0].deck = kDeck;
    replay.players[1].deck = kDeck;
    pokebot::RandomAgent first(8);
    pokebot::RandomAgent second(9);
    const pokebot::GameRecord record = pokebot::play_game(CardTable(db, kDeck, kDeck), 4, first, second);
    replay.actions = record.moves;
    replay.result = record.result;

    std::ostringstream out;
    int turns = 0;
    pokebot::print_replay(out, pokebot::replay_table(db, replay), replay, pokebot::kEveryone, [&] { ++turns; });
    CHECK(turns > 5);
    CHECK(static_cast<std::size_t>(turns) == count(out.str(), "\nTurn ") - 1); // once per turn shown; the last board is the end
}
