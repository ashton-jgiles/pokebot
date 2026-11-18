#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/random_agent.hpp"
#include "engine/describe/describe.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
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
using pokebot::DecisionKind;
using pokebot::EnergyType;
using pokebot::GameState;
using pokebot::kNoCard;
using pokebot::Observation;
using pokebot::observe;
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

    // TST-1 Basicmon, TST-2 Energy, TST-3 Seek (search, revealed), TST-7 Look (top 3, take 1),
    // TST-8 to TST-10: more Basics, so mulligans are rare.
    CardDB sample_db() {
        CardDB db;
        db.add(test::basic_pokemon("TST-1", "Basicmon"));
        db.add(test::basic_energy("TST-2", "Darkness Energy", EnergyType::Darkness));
        db.add(item("TST-3", "Seek", R"([{"op": "search_deck", "reveal": true, "filter": {"kind": "pokemon"}}])"));
        db.add(item("TST-7", "Look", R"([{"op": "look_at_top", "count": 3, "take": 1}])"));
        db.add(test::basic_pokemon("TST-8", "Plainmon"));
        db.add(test::basic_pokemon("TST-9", "Othermon"));
        db.add(test::basic_pokemon("TST-10", "Lastmon"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 8}, {4, 9}, {4, 10}, {4, 3}, {4, 7}, {36, 2}});

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
            pokebot::detail::shuffle_deck(game, me);
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        // Puts a card in the hand, as if drawn.
        void give(const char* id) {
            test::put_in_hand(game, me, id);
            pokebot::detail::note_visible(game);
        }

        void play(const char* id) {
            test::put_in_hand(game, me, id);
            pokebot::apply(game, test::find_action(game, ActionKind::PlayTrainer, id));
        }
    };

    bool own(int viewer, CardIndex card) {
        return pokebot::owner_of(card) == viewer;
    }

    // Every card the observation names, outside its two lists of cards whose place isn't known.
    template <class Visit>
    void each_named_card(const Observation& view, Visit visit) {
        for (const auto& player : view.players) {
            for (const auto* zone : {&player.hand, &player.discard, &player.lost_zone}) {
                for (const CardIndex card : *zone) {
                    visit(card);
                }
            }
            for (const auto& card : player.deck) {
                visit(card.card);
            }
            for (const CardIndex card : player.resolving) {
                visit(card);
            }
            if (player.stadium != pokebot::kNoCard) {
                visit(player.stadium);
            }
            const auto in_play = [&](const pokebot::PokemonView& seen) {
                for (const CardIndex card : seen.pokemon.stack) {
                    visit(card);
                }
                for (const CardIndex card : seen.pokemon.energy) {
                    visit(card);
                }
                if (seen.pokemon.tool != pokebot::kNoCard) {
                    visit(seen.pokemon.tool);
                }
            };
            if (player.active) {
                in_play(*player.active);
            }
            for (const auto& pokemon : player.bench) {
                in_play(pokemon);
            }
        }
    }

    // Swaps the first two cards, one from each zone, that `viewer` doesn't know the place of and that
    // aren't the same card. Returns false if there are none.
    template <class ZoneA, class ZoneB>
    bool swap_hidden(const GameState& game, int viewer, ZoneA& a, ZoneB& b) {
        for (auto& x : a) {
            for (auto& y : b) {
                if (!pokebot::knows_where(game, viewer, x) && !pokebot::knows_where(game, viewer, y) && game.def(x).id != game.def(y).id) {
                    std::swap(x, y);
                    return true;
                }
            }
        }
        return false;
    }

    // Swaps hidden cards (the opponent's hand or Prize cards with their deck, and the viewer's Prize cards
    // with their deck) and checks the viewer sees no difference. Returns false if nothing could be swapped.
    bool hidden_swaps_look_the_same(const GameState& game, int viewer) {
        GameState changed = game;
        auto& theirs = changed.player(pokebot::opponent_of(viewer));
        auto& mine = changed.player(viewer);
        const bool hand = swap_hidden(game, viewer, theirs.hand, theirs.deck);
        const bool prizes = swap_hidden(game, viewer, theirs.prizes, theirs.deck);
        const bool own_prizes = swap_hidden(game, viewer, mine.prizes, mine.deck);
        if (!(hand || prizes || own_prizes)) {
            return false;
        }
        CHECK(observe(changed, viewer) == observe(game, viewer));
        return true;
    }

    // Every card the observation names is one whose place the viewer knows, with its definition; the
    // viewer's other cards are in deck_or_prizes; and the observation can be described.
    void check_names(const GameState& game, int viewer) {
        const Observation view = observe(game, viewer);
        each_named_card(view, [&](CardIndex card) {
            INFO("viewer " << viewer << ", card " << static_cast<int>(card));
            REQUIRE(pokebot::knows_where(game, viewer, card));
            REQUIRE(view.def(card) != nullptr);
        });
        for (const CardIndex card : view.deck_or_prizes) {
            REQUIRE(own(viewer, card));
            REQUIRE_FALSE(pokebot::knows_where(game, viewer, card));
        }
        for (const auto& player : view.players) {
            REQUIRE(player.hand.size() <= player.hand_size);
        }
        REQUIRE_FALSE(describe(view).empty());
    }

} // namespace

TEST_CASE("you see your own hand, and only the size of the opponent's", "[observation]") {
    Fixture f;
    f.give("TST-2");
    const Observation mine = observe(f.game, f.me);
    CHECK(mine.viewer == f.me);
    CHECK(mine.me().hand == f.game.player(f.me).hand);
    CHECK(mine.opponent().hand.empty());
    CHECK(mine.opponent().hand_size == f.game.player(f.them).hand.size());
    CHECK(mine.opponent().deck_size == f.game.player(f.them).deck.size());
    CHECK(mine.opponent().prize_count == 6);

    const Observation theirs = observe(f.game, f.them);
    CHECK(theirs.opponent().hand.empty());
    CHECK(theirs.opponent().hand_size == 1);
    CHECK(theirs.opponent().active->pokemon == *f.game.player(f.me).active);
}

TEST_CASE("an observation has card definitions only for cards the viewer has seen", "[observation]") {
    Fixture f;
    for (int viewer = 0; viewer < pokebot::kPlayers; ++viewer) {
        const Observation view = observe(f.game, viewer);
        for (std::size_t i = 0; i < pokebot::kMaxCards; ++i) {
            const auto card = static_cast<CardIndex>(i);
            if (own(viewer, card) || pokebot::has_seen(f.game, viewer, card)) {
                REQUIRE(view.def(card) == &f.game.def(card));
            } else {
                REQUIRE(view.def(card) == nullptr);
            }
        }
        CHECK(view.opponent_seen.size() < pokebot::kDeckSize);
        CHECK(std::is_sorted(view.opponent_seen.begin(), view.opponent_seen.end()));
    }
}

TEST_CASE("your deck and Prize cards are one sorted list", "[observation]") {
    Fixture f;
    const Observation view = observe(f.game, f.me);
    const auto& p = f.game.player(f.me);
    CHECK(view.me().deck.empty());
    CHECK(view.deck_or_prizes.size() == p.deck.size() + p.prizes.size());
    CHECK(std::is_sorted(view.deck_or_prizes.begin(), view.deck_or_prizes.end()));
    for (const CardIndex card : p.prizes) {
        CHECK(view.deck_or_prizes.contains(card));
    }
}

TEST_CASE("a revealed card shows in the opponent's view of the hand", "[observation]") {
    Fixture f;
    f.play("TST-3");
    const Action choose = test::find_action(f.game, ActionKind::ChooseCard);
    pokebot::apply(f.game, choose);
    const Observation theirs = observe(f.game, f.them);
    CHECK(theirs.opponent().hand == pokebot::FixedVector<CardIndex, pokebot::kDeckSize>{choose.card});
    CHECK(theirs.def(choose.card) != nullptr);
    CHECK(theirs.opponent_seen.contains(choose.card));
}

TEST_CASE("cards put on the bottom of the deck are shown where they are", "[observation]") {
    Fixture f;
    f.play("TST-7");
    REQUIRE(f.game.decision().kind == DecisionKind::ChooseCards);
    pokebot::apply(f.game, test::find_action(f.game, ActionKind::ChooseCard));
    const auto& deck = f.game.player(f.me).deck;
    const Observation view = observe(f.game, f.me);
    REQUIRE(view.me().deck.size() == 2);
    CHECK(view.me().deck[0] == pokebot::DeckCard{deck[1], static_cast<std::uint8_t>(deck.size() - 2)});
    CHECK(view.me().deck[1] == pokebot::DeckCard{deck[0], static_cast<std::uint8_t>(deck.size() - 1)});
    CHECK_FALSE(view.deck_or_prizes.contains(deck[0]));
    CHECK(view.opponent().deck.empty());
}

TEST_CASE("the opponent's setup Pokemon are face down until both players have set up", "[observation]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    GameState game = pokebot::new_game(table, 5);
    while (!(game.decision().kind == DecisionKind::SetupActive && game.decision().player == 1)) {
        pokebot::apply(game, pokebot::legal_actions(game).front());
    }
    CHECK_FALSE(observe(game, 1).player(0).active.has_value());
    CHECK(observe(game, 1).player(0).face_down == 1 + game.player(0).bench.size());
    CHECK(observe(game, 0).player(0).active.has_value());

    test::play_setup(game);
    CHECK(observe(game, 1).player(0).active.has_value());
    CHECK(observe(game, 1).player(0).face_down == 0);
}

TEST_CASE("games that differ only in hidden cards look the same", "[observation]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    pokebot::Rng chooser(3);
    int compared = 0;
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        GameState game = pokebot::new_game(table, seed);
        const int stop = static_cast<int>(chooser.below(60)) + 20;
        for (int step = 0; step < stop && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply(game, actions[chooser.below(static_cast<std::uint32_t>(actions.size()))]);
        }
        if (game.is_over()) {
            continue;
        }
        for (int viewer = 0; viewer < pokebot::kPlayers; ++viewer) {
            INFO("seed " << seed << ", viewer " << viewer);
            compared += hidden_swaps_look_the_same(game, viewer) ? 1 : 0;
        }
    }
    CHECK(compared > 50);
}

TEST_CASE("an observation changes when something the viewer can see does", "[observation]") {
    Fixture f;
    f.give("TST-2");
    GameState changed = f.game;
    auto& p = changed.player(f.me);
    std::swap(p.hand[0], p.deck[0]);
    CHECK(observe(changed, f.them) == observe(f.game, f.them));
    CHECK_FALSE(observe(changed, f.me) == observe(f.game, f.me));
}

TEST_CASE("in random games an observation names only cards whose place the viewer knows", "[observation]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    pokebot::Rng chooser(17);
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        GameState game = pokebot::new_game(table, seed);
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply(game, actions[chooser.below(static_cast<std::uint32_t>(actions.size()))]);
            for (int viewer = 0; viewer < pokebot::kPlayers; ++viewer) {
                INFO("seed " << seed);
                check_names(game, viewer);
            }
        }
    }
}

TEST_CASE("on the real decks, observations name only cards the viewer knows and hide the rest", "[observation]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::size_t n = real->decks.size();
    int compared = 0;
    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table = CardTable(real->db, real->decks[i % n], real->decks[(i / n) % n]).shuffled(~seed);
        pokebot::BiasedRandomAgent agent(seed);
        GameState game = pokebot::new_game(table, seed);
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply_legal(game, pokebot::ask(agent, game, actions));
            for (int viewer = 0; viewer < pokebot::kPlayers; ++viewer) {
                INFO("seed " << seed << ", step " << step);
                check_names(game, viewer);
                if (step % 20 == 0) {
                    compared += hidden_swaps_look_the_same(game, viewer) ? 1 : 0;
                }
            }
        }
    }
    CHECK(compared > 1000);
}

TEST_CASE("describe shows your hand, and the opponent's as a count", "[observation][describe]") {
    Fixture f;
    f.give("TST-2");
    const std::string mine = describe(observe(f.game, f.me));
    CHECK(mine.find("You (player " + std::to_string(f.me)) != std::string::npos);
    CHECK(mine.find("Hand:   Darkness Energy") != std::string::npos);
    const std::size_t size = f.game.player(f.them).hand.size();
    CHECK(mine.find("Hand:   " + std::to_string(size) + (size == 1 ? " card\n" : " cards\n")) != std::string::npos);

    const std::string theirs = describe(observe(f.game, f.them));
    CHECK(theirs.find("Opponent (player " + std::to_string(f.me)) != std::string::npos);
    CHECK(theirs.find("Hand:   1 card\n") != std::string::npos);
}

namespace {

    // Plays like the biased agent, and counts anything wrong with what it's shown.
    class Spy final : public pokebot::Agent {
    public:
        Spy(int seat, std::uint64_t seed) : seat_(seat), inner_(seed) {}

        std::string name() const override { return "spy"; }

        Action choose(const Observation& observation, const std::vector<Action>& actions) override {
            ++asked;
            if (observation.viewer != seat_ || observation.decision.player != seat_) {
                ++wrong_viewer;
            }
            for (const Action& action : actions) {
                if (action.card != kNoCard && observation.def(action.card) == nullptr) {
                    ++unseen_cards;
                }
            }
            return inner_.choose(observation, actions);
        }

        int asked = 0;
        int wrong_viewer = 0;
        int unseen_cards = 0;

    private:
        int seat_;
        pokebot::BiasedRandomAgent inner_;
    };

} // namespace

TEST_CASE("play_game shows each agent its own player's view", "[observation][agents]") {
    const CardDB db = sample_db();
    const CardTable table(db, kDeck, kDeck);
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        Spy first(0, seed * 2);
        Spy second(1, seed * 2 + 1);
        pokebot::play_game(table, seed, first, second);
        INFO("seed " << seed);
        CHECK(first.asked > 0);
        CHECK(second.asked > 0);
        CHECK(first.wrong_viewer + second.wrong_viewer == 0);
        CHECK(first.unseen_cards + second.unseen_cards == 0);
    }
}

TEST_CASE("on the real decks, the actions offered name only cards the agent knows the place of", "[observation][agents]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::size_t n = real->decks.size();
    int offered = 0;
    int unknown = 0;
    std::string first_unknown;
    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table = CardTable(real->db, real->decks[i % n], real->decks[(i / n) % n]).shuffled(~seed);
        pokebot::BiasedRandomAgent agent(seed);
        GameState game = pokebot::new_game(table, seed);
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            const int player = game.decision().player;
            for (const Action& action : actions) {
                if (action.card == kNoCard) {
                    continue;
                }
                ++offered;
                if (!pokebot::knows_where(game, player, action.card)) {
                    if (unknown == 0) {
                        first_unknown = "seed " + std::to_string(seed) + ", step " + std::to_string(step) + ": " + pokebot::describe(game, action);
                    }
                    ++unknown;
                }
            }
            pokebot::apply_legal(game, pokebot::ask(agent, game, actions));
        }
    }
    CHECK(offered > 10000);
    INFO(first_unknown);
    CHECK(unknown == 0);
}
