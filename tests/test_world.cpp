#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/random_agent.hpp"
#include "agents/world.hpp"
#include "engine/engine.hpp"
#include "engine/invariants/invariants.hpp"
#include "engine/observation/observation.hpp"
#include "harness/match.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using nlohmann::json;
using pokebot::Action;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardIndex;
using pokebot::CardTable;
using pokebot::GameState;
using pokebot::Observation;
using pokebot::observe;
using pokebot::Rng;
using pokebot::World;

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

    // Basics, Energy, a deck search that isn't revealed and a look at the top of the deck that puts
    // two cards on the bottom, so the viewer knows the place of some deck cards.
    CardDB sample_db() {
        CardDB db;
        db.add(test::basic_pokemon("TST-1", "Basicmon"));
        db.add(test::basic_energy("TST-2", "Darkness Energy", pokebot::EnergyType::Darkness));
        db.add(item("TST-3", "Seek Two", R"([{"op": "search_deck", "count": 2, "filter": {"kind": "pokemon"}}])"));
        db.add(item("TST-7", "Look", R"([{"op": "look_at_top", "count": 3, "take": 1}])"));
        db.add(test::basic_pokemon("TST-8", "Plainmon"));
        db.add(test::basic_pokemon("TST-9", "Othermon"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 8}, {4, 9}, {4, 3}, {4, 7}, {40, 2}});

    std::vector<CardIndex> sorted(std::vector<CardIndex> cards) {
        std::sort(cards.begin(), cards.end());
        return cards;
    }

    std::vector<CardIndex> deck_and_prizes(const GameState& game, int player) {
        const auto& p = game.player(player);
        std::vector<CardIndex> cards(p.deck.begin(), p.deck.end());
        cards.insert(cards.end(), p.prizes.begin(), p.prizes.end());
        return sorted(cards);
    }

    // A world built from each player's view of `game` observes back to that view, keeps the
    // invariants, and offers the deciding player the same actions as the game.
    void check_worlds(const GameState& game, Rng& rng) {
        for (int viewer = 0; viewer < pokebot::kPlayers; ++viewer) {
            INFO("viewer " << viewer);
            const Observation view = observe(game, viewer);
            const World world(view, rng);
            REQUIRE(observe(world.game(), viewer) == view);
            const auto problems = pokebot::check_invariants(world.game());
            INFO((problems.empty() ? std::string() : problems.front()));
            REQUIRE(problems.empty());
            if (game.decision().player == viewer) {
                const auto actions = pokebot::legal_actions(game);
                const auto offered = pokebot::legal_actions(world.game());
                REQUIRE(std::is_permutation(actions.begin(), actions.end(), offered.begin(), offered.end()));
            }
        }
    }

    // Plays random actions until the game is past setup and `steps` actions in, or over.
    void play_random(GameState& game, Rng& chooser, int steps) {
        for (int step = 0; (step < steps || game.turn == 0) && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply(game, actions[chooser.below(static_cast<std::uint32_t>(actions.size()))]);
        }
    }

} // namespace

TEST_CASE("a world observes back to the observation it was built from", "[world]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    Rng chooser(11);
    Rng rng(12);
    int checked = 0;
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        GameState game = pokebot::new_game(table, seed);
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            if (game.turn > 0) {
                INFO("seed " << seed << ", step " << step);
                check_worlds(game, rng);
                ++checked;
            }
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply(game, actions[chooser.below(static_cast<std::uint32_t>(actions.size()))]);
        }
    }
    CHECK(checked > 1000);
}

TEST_CASE("on the real decks, a world observes back to its observation and offers the same actions", "[world]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::size_t n = real->decks.size();
    Rng rng(5);
    for (std::uint64_t seed = 0; seed < 48; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table = CardTable(real->db, real->decks[i % n], real->decks[(i / n) % n]).shuffled(~seed);
        pokebot::BiasedRandomAgent agent(seed);
        GameState game = pokebot::new_game(table, seed);
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            if (game.turn > 0) {
                INFO("seed " << seed << ", step " << step);
                check_worlds(game, rng);
            }
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply_legal(game, pokebot::ask(agent, game, actions));
        }
    }
}

TEST_CASE("a world deals your deck and Prize cards from the same cards, in different ways", "[world]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    GameState game = pokebot::new_game(table, 3);
    Rng chooser(4);
    play_random(game, chooser, 40);
    REQUIRE_FALSE(game.is_over());
    const int me = game.current_player;
    const Observation view = observe(game, me);

    Rng rng(9);
    std::set<std::vector<CardIndex>> prizes;
    for (int i = 0; i < 20; ++i) {
        const World world(view, rng);
        CHECK(deck_and_prizes(world.game(), me) == deck_and_prizes(game, me));
        CHECK(world.game().player(me).hand == game.player(me).hand);
        const auto& dealt = world.game().player(me).prizes;
        prizes.insert(sorted(std::vector<CardIndex>(dealt.begin(), dealt.end())));
    }
    CHECK(prizes.size() > 1);
}

TEST_CASE("in a world, opponent cards you haven't seen are unknown cards and the rest keep their definition", "[world]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    GameState game = pokebot::new_game(table, 8);
    Rng chooser(2);
    play_random(game, chooser, 60);
    REQUIRE_FALSE(game.is_over());
    const int me = game.current_player;
    const Observation view = observe(game, me);
    Rng rng(1);
    const World world(view, rng);

    int unknown = 0;
    for (std::size_t i = 0; i < pokebot::kMaxCards; ++i) {
        const auto card = static_cast<CardIndex>(i);
        if (view.def(card) == nullptr) {
            CHECK(pokebot::owner_of(card) != me);
            CHECK(&world.game().def(card) == &pokebot::unknown_card());
            ++unknown;
        } else {
            CHECK(&world.game().def(card) == view.def(card));
        }
    }
    CHECK(unknown > 0);
    CHECK(static_cast<std::size_t>(unknown) + view.opponent_seen.size() == pokebot::kDeckSize);
}

TEST_CASE("an action that uses no chance and reveals nothing looks the same in a world as in the game", "[world]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    Rng chooser(21);
    Rng rng(22);
    int tried = 0;
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        GameState game = pokebot::new_game(table, seed);
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            const Action action = actions[chooser.below(static_cast<std::uint32_t>(actions.size()))];
            const bool plain = action.kind == ActionKind::AttachEnergy || action.kind == ActionKind::PlayBasic || action.kind == ActionKind::Retreat;
            if (game.turn > 0 && plain) {
                const int me = game.decision().player;
                const World world(observe(game, me), rng);
                GameState tried_in_world = world.game();
                pokebot::apply(tried_in_world, action);
                pokebot::apply(game, action);
                INFO("seed " << seed << ", step " << step);
                REQUIRE(observe(tried_in_world, me) == observe(game, me));
                ++tried;
            } else {
                pokebot::apply(game, action);
            }
        }
    }
    CHECK(tried > 500);
}

TEST_CASE("a card the opponent is choosing from their hand is stood in for by one dealt into their hand", "[world]") {
    CardDB db = sample_db();
    db.add(item("TST-11", "Toss Two", R"([{"op": "discard_from_hand", "count": 2}])"));
    const pokebot::Decklist deck = test::deck({{4, 1}, {4, 8}, {4, 9}, {4, 11}, {44, 2}});
    const CardTable table(db, deck, deck);
    GameState game = pokebot::new_game(table, 5);
    test::play_setup(game);
    test::end_turns(game, 3);
    const int them = game.current_player;
    const int me = pokebot::opponent_of(them);
    test::put_in_hand(game, them, "TST-11");
    pokebot::apply(game, test::find_action(game, ActionKind::PlayTrainer, "TST-11"));
    pokebot::apply(game, test::find_action(game, ActionKind::ChooseCard));
    REQUIRE(game.decision().kind == pokebot::DecisionKind::ChooseCards); // one more to discard

    const Observation view = observe(game, me);
    REQUIRE(view.effects.size() == 1);
    REQUIRE(view.effects[0].chosen == pokebot::FixedVector<CardIndex, pokebot::kDeckSize>{pokebot::kNoCard});
    Rng rng(3);
    const World world(view, rng);
    CHECK(observe(world.game(), me) == view);
    const auto& run = world.game().effects[0];
    REQUIRE(run.chosen.size() == 1);
    CHECK(world.game().player(them).hand.contains(run.chosen[0]));
}

TEST_CASE("a world can't be built during setup", "[world]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    const GameState game = pokebot::new_game(table, 1);
    Rng rng(1);
    CHECK_THROWS_AS(World(observe(game, 0), rng), std::invalid_argument);
}
