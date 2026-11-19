#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/random_agent.hpp"
#include "engine/describe/describe.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "harness/match.hpp"
#include "harness/replay.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using Json = nlohmann::ordered_json;
using nlohmann::json;
using pokebot::Action;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::GameState;
using pokebot::Replay;
using pokebot::ReplayError;

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

    // TST-1, TST-8 to TST-10 Basics, TST-2 Energy, TST-3 Seek (search, revealed), TST-4 Peek (search, not revealed).
    CardDB sample_db() {
        CardDB db;
        db.add(test::basic_pokemon("TST-1", "Basicmon"));
        db.add(test::basic_energy("TST-2", "Darkness Energy", pokebot::EnergyType::Darkness));
        db.add(item("TST-3", "Seek", R"([{"op": "search_deck", "reveal": true, "filter": {"kind": "pokemon"}}])"));
        db.add(item("TST-4", "Peek", R"([{"op": "search_deck", "filter": {"kind": "pokemon"}}])"));
        db.add(test::basic_pokemon("TST-8", "Plainmon"));
        db.add(test::basic_pokemon("TST-9", "Othermon"));
        db.add(test::basic_pokemon("TST-10", "Lastmon"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 8}, {4, 9}, {4, 10}, {4, 3}, {4, 4}, {36, 2}});

    Replay sample_replay(std::uint64_t seed) {
        Replay replay;
        replay.seed = seed;
        replay.players[0] = pokebot::ReplayPlayer{"first.txt", "biased", kDeck};
        replay.players[1] = pokebot::ReplayPlayer{"second.txt", "random", kDeck};
        return replay;
    }

    // Plays a game with play_game and fills in the replay from it.
    Replay played(const CardDB& db, std::uint64_t seed) {
        Replay replay = sample_replay(seed);
        pokebot::BiasedRandomAgent first(seed * 2);
        pokebot::RandomAgent second(seed * 2 + 1);
        const pokebot::GameRecord record = pokebot::play_game(CardTable(db, kDeck, kDeck), seed, first, second);
        replay.actions = record.moves;
        replay.result = record.result;
        return replay;
    }

    bool contains(const std::string& text, const std::string& part) {
        return text.find(part) != std::string::npos;
    }

} // namespace

TEST_CASE("a replay survives saving and loading", "[replay]") {
    const CardDB db = sample_db();
    const Replay replay = [&] {
        Replay r = played(db, 3);
        r.note = "a note\nover two lines";
        return r;
    }();
    REQUIRE(replay.actions.size() > 50);

    CHECK(pokebot::replay_from_json(Json::parse(pokebot::to_json(replay).dump())) == replay);

    const auto path = std::filesystem::temp_directory_path() / "pokebot-test-replay.json";
    pokebot::save_replay(replay, path);
    CHECK(pokebot::load_replay(path) == replay);
    std::filesystem::remove(path);
}

TEST_CASE("playing a replay back reaches exactly the same game", "[replay]") {
    const CardDB db = sample_db();
    for (std::uint64_t seed = 0; seed < 30; ++seed) {
        INFO("seed " << seed);
        Replay replay = sample_replay(seed);
        const CardTable table = pokebot::replay_table(db, replay);
        GameState game = pokebot::new_game(table, seed);
        pokebot::RandomAgent agent(seed);
        while (!game.is_over() && replay.actions.size() < 20000) {
            const auto actions = pokebot::legal_actions(game);
            replay.actions.push_back(pokebot::ask(agent, game, actions));
            pokebot::apply_legal(game, replay.actions.back());
        }
        replay.result = game.result;

        int steps = 0;
        const GameState end = pokebot::play_back(table, replay, [&](const GameState&, const Action& action, const GameState&) {
            CHECK(action == replay.actions[static_cast<std::size_t>(steps)]);
            ++steps;
        });
        CHECK(end == game);
        CHECK(steps == static_cast<int>(replay.actions.size()));
    }
}

TEST_CASE("a game from play_game replays to the same ending", "[replay]") {
    const CardDB db = sample_db();
    for (std::uint64_t seed = 0; seed < 30; ++seed) {
        INFO("seed " << seed);
        const Replay replay = played(db, seed);
        const GameState end = pokebot::play_back(pokebot::replay_table(db, replay), replay);
        CHECK(end.result == replay.result);
        CHECK(end.is_over());
    }
}

TEST_CASE("a replay plays back to exactly play_game's final state", "[replay]") {
    const CardDB db = sample_db();
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        INFO("seed " << seed);
        Replay replay = sample_replay(seed);
        pokebot::BiasedRandomAgent first(seed * 2);
        pokebot::RandomAgent second(seed * 2 + 1);
        GameState end;
        pokebot::MatchOptions options;
        options.final_state = &end;
        const pokebot::GameRecord record = pokebot::play_game(CardTable(db, kDeck, kDeck), seed, first, second, options);
        CHECK(end.cards == nullptr);
        CHECK(end.result == record.result);
        replay.actions = record.moves;
        replay.result = record.result;

        GameState replayed = pokebot::play_back(pokebot::replay_table(db, replay), replay);
        replayed.cards = nullptr;
        CHECK(replayed == end);
    }
}

TEST_CASE("real-deck games replay to the same ending after saving", "[replay]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::size_t n = real->decks.size();
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        INFO("seed " << seed);
        const auto i = static_cast<std::size_t>(seed);
        Replay replay;
        replay.seed = seed;
        replay.players[0].deck = real->decks[i % n];
        replay.players[1].deck = real->decks[(i / n) % n];
        pokebot::BiasedRandomAgent first(seed * 2);
        pokebot::BiasedRandomAgent second(seed * 2 + 1);
        const pokebot::GameRecord record = pokebot::play_game(CardTable(real->db, replay.players[0].deck, replay.players[1].deck), seed, first, second);
        replay.actions = record.moves;
        replay.result = record.result;

        const Replay loaded = pokebot::replay_from_json(Json::parse(pokebot::to_json(replay).dump()));
        REQUIRE(loaded == replay);
        const GameState end = pokebot::play_back(pokebot::replay_table(real->db, loaded), loaded);
        CHECK(end.result == record.result);
        CHECK(static_cast<int>(end.turn) == record.turns);
    }
}

TEST_CASE("a replay that doesn't play back says which action failed", "[replay]") {
    const CardDB db = sample_db();
    Replay replay = played(db, 5);
    const CardTable table = pokebot::replay_table(db, replay);

    Replay wrong_action = replay;
    wrong_action.actions[2] = Action{ActionKind::Attack, pokebot::kNoCard, pokebot::kActiveSlot, 0};
    try {
        pokebot::play_back(table, wrong_action);
        FAIL("an illegal action was played back");
    } catch (const ReplayError& e) {
        CHECK(contains(e.what(), "action 3 (Attack - 0 0) isn't legal"));
    }

    Replay wrong_result = replay;
    wrong_result.result = replay.result == pokebot::GameResult::Player0Wins ? pokebot::GameResult::Player1Wins : pokebot::GameResult::Player0Wins;
    CHECK_THROWS_AS(pokebot::play_back(table, wrong_result), ReplayError);

    Replay too_long = replay;
    too_long.actions.push_back(too_long.actions.back());
    CHECK_THROWS_AS(pokebot::play_back(table, too_long), ReplayError);
}

TEST_CASE("files that aren't replays are rejected", "[replay]") {
    const CardDB db = sample_db();
    const Json good = pokebot::to_json(played(db, 1));
    CHECK_NOTHROW(pokebot::replay_from_json(good));

    Json other = good;
    other["format"] = "something else";
    CHECK_THROWS_AS(pokebot::replay_from_json(other), ReplayError);

    Json future = good;
    future["version"] = 2;
    CHECK_THROWS_AS(pokebot::replay_from_json(future), ReplayError);

    Json bad_action = good;
    bad_action["actions"][0] = "Fly 1 2 3";
    CHECK_THROWS_AS(pokebot::replay_from_json(bad_action), ReplayError);

    Json bad_card = good;
    bad_card["actions"][0] = "ChooseCard 120 0 0";
    CHECK_THROWS_AS(pokebot::replay_from_json(bad_card), ReplayError);

    Json one_player = good;
    one_player["players"].erase(1);
    CHECK_THROWS_AS(pokebot::replay_from_json(one_player), ReplayError);

    CHECK_THROWS_AS(pokebot::replay_from_json(Json::array()), ReplayError);
    CHECK_THROWS_AS(pokebot::load_replay("no/such/replay.Json"), ReplayError);
}

TEST_CASE("the opponent doesn't see what a search that doesn't reveal found", "[replay][describe]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    for (const bool reveal : {false, true}) {
        GameState game = pokebot::new_game(table, 5);
        test::play_setup(game);
        test::end_turns(game, 3);
        const int me = game.current_player;
        const int them = pokebot::opponent_of(me);
        const char* id = reveal ? "TST-3" : "TST-4";
        test::put_in_hand(game, me, id);
        pokebot::apply(game, test::find_action(game, ActionKind::PlayTrainer, id));

        const Action choice = test::find_action(game, ActionKind::ChooseCard);
        const GameState before = game;
        pokebot::apply(game, choice);
        const std::string name = before.def(choice.card).name;
        INFO("reveal " << reveal);
        CHECK(pokebot::describe(before, choice, game, me) == "choose " + name);
        CHECK(pokebot::describe(before, choice, game, them) == (reveal ? "choose " + name : "choose an unseen card"));
    }
}

TEST_CASE("an action is described to a player without the cards they don't know", "[replay][describe]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    int hidden = 0;
    for (std::uint64_t seed = 0; seed < 30; ++seed) {
        GameState game = pokebot::new_game(table, seed);
        pokebot::BiasedRandomAgent agent(seed);
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            const Action action = pokebot::ask(agent, game, actions);
            const GameState before = game;
            pokebot::apply_legal(game, action);
            if (action.card == pokebot::kNoCard) {
                continue;
            }
            for (int viewer = 0; viewer < pokebot::kPlayers; ++viewer) {
                const bool known = pokebot::knows_where(before, viewer, action.card) || pokebot::knows_where(game, viewer, action.card);
                const std::string text = pokebot::describe(before, action, game, viewer);
                INFO("seed " << seed << ", step " << step << ": " << text);
                CHECK((contains(text, "an unseen card") || contains(text, "a face-down Pokemon")) == !known);
                hidden += known ? 0 : 1;
            }
        }
    }
    CHECK(hidden > 100); // setup and Peek hide cards often
}

TEST_CASE("print_replay shows the whole game, or one player's view of it", "[replay]") {
    const CardDB db = sample_db();
    const Replay replay = played(db, 2);
    const CardTable table = pokebot::replay_table(db, replay);

    std::ostringstream everything;
    pokebot::print_replay(everything, table, replay);
    CHECK(contains(everything.str(), "Game 2: first.txt (player 0, biased) vs second.txt (player 1, random)"));
    CHECK(contains(everything.str(), "Showing everything"));
    CHECK(contains(everything.str(), "Ended by: "));
    CHECK_FALSE(contains(everything.str(), "an unseen card"));
    CHECK_FALSE(contains(everything.str(), "a face-down Pokemon"));

    for (int viewer = 0; viewer < pokebot::kPlayers; ++viewer) {
        std::ostringstream view;
        pokebot::print_replay(view, table, replay, viewer);
        CHECK(contains(view.str(), "As player " + std::to_string(viewer) + " saw it"));
        CHECK(contains(view.str(), "You (player " + std::to_string(viewer)));
        CHECK(contains(view.str(), "a face-down Pokemon")); // the opponent's setup
        CHECK(contains(view.str(), "Ended by: "));
    }
}
