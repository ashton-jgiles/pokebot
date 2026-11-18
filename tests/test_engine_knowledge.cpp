#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine/engine.hpp"
#include "engine/invariants/invariants.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using nlohmann::json;
using pokebot::Action;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardIndex;
using pokebot::CardTable;
using pokebot::DecisionKind;
using pokebot::EnergyType;
using pokebot::GameState;
using pokebot::has_seen;
using pokebot::knows_where;
using pokebot::TrainerKind;

namespace {

    CardDef item(const char* id, const char* name, const char* effects) {
        CardDef c;
        c.id = id;
        c.name = name;
        c.kind = pokebot::CardKind::Trainer;
        c.trainer_kind = TrainerKind::Item;
        c.text = "test card";
        c.effects = json::parse(effects);
        return c;
    }

    // TST-1 Basicmon, TST-2 Energy, TST-3 Seek (search, revealed), TST-4 Peek (search, not revealed),
    // TST-5 Shuffle (hand into deck), TST-6 Fetch (a Pokemon from the discard pile), TST-7 Look (top 3, take 1),
    // TST-8 to TST-10: more Basics, so mulligans are rare.
    CardDB sample_db() {
        CardDB db;
        db.add(test::basic_pokemon("TST-1", "Basicmon"));
        db.add(test::basic_energy("TST-2", "Darkness Energy", EnergyType::Darkness));
        db.add(item("TST-3", "Seek", R"([{"op": "search_deck", "reveal": true, "filter": {"kind": "pokemon"}}])"));
        db.add(item("TST-4", "Peek", R"([{"op": "search_deck", "filter": {"kind": "pokemon"}}])"));
        db.add(item("TST-5", "Shuffle", R"([{"op": "shuffle_hand_into_deck"}])"));
        db.add(item("TST-6", "Fetch", R"([{"op": "from_discard", "filter": {"kind": "pokemon"}}])"));
        db.add(item("TST-7", "Look", R"([{"op": "look_at_top", "count": 3, "take": 1}])"));
        db.add(test::basic_pokemon("TST-8", "Plainmon"));
        db.add(test::basic_pokemon("TST-9", "Othermon"));
        db.add(test::basic_pokemon("TST-10", "Lastmon"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 8}, {4, 9}, {4, 10}, {4, 3}, {4, 4}, {4, 5}, {4, 6}, {4, 7}, {24, 2}});

    // Turn 4: `me` is the current player, with an empty hand.
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
            auto& p = game.player(me);
            for (const auto card : p.hand) {
                p.deck.push_back(card);
            }
            p.hand.clear();
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        void play(const char* id) {
            test::put_in_hand(game, me, id);
            pokebot::apply(game, test::find_action(game, ActionKind::PlayTrainer, id));
        }

        // Chooses the first card offered in the open ChooseCards decision, and returns it.
        CardIndex choose() {
            REQUIRE(game.decision().kind == DecisionKind::ChooseCards);
            const Action action = test::find_action(game, ActionKind::ChooseCard);
            pokebot::apply(game, action);
            return action.card;
        }

        // Setup may have put every copy of one Basic into play, so find one still in the deck or hand.
        const char* basic_left() const {
            for (const char* id : {"TST-1", "TST-8", "TST-9", "TST-10"}) {
                if (test::has_card(game, me, id)) {
                    return id;
                }
            }
            FAIL("no Basic Pokemon left");
            return "";
        }
    };

    std::size_t seen_of(const GameState& game, int viewer, int owner) {
        std::size_t count = 0;
        for (std::size_t i = 0; i < pokebot::kDeckSize; ++i) {
            if (has_seen(game, viewer, static_cast<CardIndex>(static_cast<std::size_t>(owner) * pokebot::kDeckSize + i))) {
                ++count;
            }
        }
        return count;
    }

} // namespace

TEST_CASE("cards in the open are seen by both players, a hand only by its owner", "[knowledge]") {
    Fixture f;
    const auto& mine = f.game.player(f.me);
    const auto& theirs = f.game.player(f.them);
    for (const auto card : mine.active->stack) {
        CHECK(knows_where(f.game, f.them, card));
    }
    for (const auto card : theirs.hand) {
        CHECK(knows_where(f.game, f.them, card));
        CHECK_FALSE(knows_where(f.game, f.me, card));
    }
}

TEST_CASE("Pokemon put down during setup stay face down until both players have set up", "[knowledge]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    GameState game = pokebot::new_game(table, 5);
    while (!(game.decision().kind == DecisionKind::SetupActive && game.decision().player == 1)) {
        pokebot::apply(game, pokebot::legal_actions(game).front());
    }
    REQUIRE(game.player(0).active.has_value());
    const CardIndex active = game.player(0).active->id();
    CHECK(knows_where(game, 0, active));
    CHECK_FALSE(knows_where(game, 1, active));

    test::play_setup(game);
    CHECK(knows_where(game, 1, active));
}

TEST_CASE("a revealed search shows the card to the opponent until it's shuffled back into the deck", "[knowledge]") {
    Fixture f;
    f.play("TST-3");
    const CardIndex found = f.choose();
    REQUIRE(f.game.player(f.me).hand.contains(found));
    CHECK(knows_where(f.game, f.them, found));

    f.play("TST-5"); // the hand, Basicmon too, is shuffled into the deck
    REQUIRE(f.game.player(f.me).deck.contains(found));
    CHECK_FALSE(knows_where(f.game, f.them, found));
    CHECK_FALSE(knows_where(f.game, f.me, found));
    CHECK(has_seen(f.game, f.them, found)); // but they remember seeing it
    CHECK(pokebot::check_invariants(f.game).empty());
}

TEST_CASE("a search that doesn't reveal shows the opponent nothing", "[knowledge]") {
    Fixture f;
    f.play("TST-4");
    const CardIndex found = f.choose();
    CHECK(knows_where(f.game, f.me, found));
    CHECK_FALSE(knows_where(f.game, f.them, found));
}

TEST_CASE("a card picked up from the discard pile stays known to the opponent", "[knowledge]") {
    Fixture f;
    f.game.player(f.me).discard.push_back(test::take_from_deck(f.game, f.me, f.basic_left()));
    f.play("TST-6");
    const CardIndex fetched = f.choose();
    REQUIRE(f.game.player(f.me).hand.contains(fetched));
    CHECK(knows_where(f.game, f.them, fetched));
}

TEST_CASE("looking at the top cards tells only you where they are", "[knowledge]") {
    Fixture f;
    test::put_in_hand(f.game, f.me, "TST-7");
    const auto& deck = f.game.player(f.me).deck;
    const std::vector<CardIndex> top(deck.end() - 3, deck.end());
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::PlayTrainer, "TST-7"));
    for (const CardIndex card : top) {
        CHECK(knows_where(f.game, f.me, card));
        CHECK_FALSE(knows_where(f.game, f.them, card));
    }
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    pokebot::apply(f.game, pokebot::legal_actions(f.game).front());
    for (const CardIndex card : top) {
        CHECK(knows_where(f.game, f.me, card)); // one in hand, two at the bottom of the deck
    }
}

TEST_CASE("a mulligan shows the hand to the opponent", "[knowledge]") {
    const CardDB db = sample_db();
    const auto few_basics = test::deck({{1, 1}, {4, 3}, {4, 4}, {4, 5}, {4, 6}, {4, 7}, {39, 2}});
    const CardTable table(db, few_basics, few_basics);
    int checked = 0;
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        const GameState game = pokebot::new_game(table, seed);
        for (int p = 0; p < pokebot::kPlayers; ++p) {
            const int opponent = pokebot::opponent_of(p);
            if (game.player(p).mulligans > 0) {
                CHECK(seen_of(game, opponent, p) >= 7);
                ++checked;
            } else {
                CHECK(seen_of(game, opponent, p) == 0);
            }
            for (const auto card : game.player(p).deck) {
                CHECK_FALSE(knows_where(game, opponent, card)); // shown, then shuffled away
            }
        }
    }
    CHECK(checked > 0);
}

TEST_CASE("in random games every card a player can see is recorded, and no Prize card is known", "[knowledge]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    pokebot::Rng chooser(11);
    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        GameState game = pokebot::new_game(table, seed);
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply(game, actions[chooser.below(static_cast<std::uint32_t>(actions.size()))]);
            pokebot::each_visible_card(game, [&](CardIndex card, int owner, bool owner_only) {
                REQUIRE(knows_where(game, owner, card));
                if (!owner_only) {
                    REQUIRE(knows_where(game, pokebot::opponent_of(owner), card));
                }
            });
            const auto problems = pokebot::check_invariants(game);
            INFO("seed " << seed << ": " << (problems.empty() ? "" : problems.front()));
            REQUIRE(problems.empty());
        }
    }
}
