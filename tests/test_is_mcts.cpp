#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/flat_mc/deck_model.hpp"
#include "agents/is_mcts/is_mcts.hpp"
#include "agents/random/random_agent.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "engine/observation/observation.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using pokebot::ArchetypeLibrary;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::DeckModel;
using pokebot::GameState;
using pokebot::IsMcts;
using pokebot::IsMctsOptions;
using pokebot::RootChoice;

namespace {

    CardDef with_attack(CardDef c, const char* name, int damage) {
        pokebot::Attack a;
        a.name = name;
        a.cost = {pokebot::EnergyType::Colorless};
        a.damage = damage;
        c.attacks.push_back(a);
        return c;
    }

    CardDB sample_db() {
        CardDB db;
        db.add(with_attack(test::basic_pokemon("TST-1", "Tackler"), "Tackle", 30));
        db.add(test::basic_energy("TST-2", "Darkness Energy", pokebot::EnergyType::Darkness));
        db.add(with_attack(test::basic_pokemon("TST-3", "Tackler B"), "Tackle", 30));
        db.add(with_attack(test::evolution("TST-4", "Big Tackler", pokebot::Stage::Stage1, "Tackler"), "Big Tackle", 60));
        db.add(with_attack(test::basic_pokemon("TST-5", "Glider", 60, 0), "Tackle", 30));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 3}, {4, 4}, {4, 5}, {44, 2}});

    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 2);
        ArchetypeLibrary library;
        int me = 0;
        int them = 0;

        Fixture() {
            library.add("sample", "main", kDeck, db);
            test::play_setup(game);
            test::end_turns(game, 3);
            me = game.current_player;
            them = pokebot::opponent_of(me);
            for (const int p : {me, them}) {
                test::clear_bench(game, p);
                test::set_active(game, p, "TST-1");
                test::add_to_bench(game, p, "TST-3");
                pokebot::detail::shuffle_deck(game, p);
            }
            auto& hand = game.player(me).hand;
            while (!hand.empty()) {
                game.player(me).discard.push_back(hand.pop_back());
            }
            seen();
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        void seen() { pokebot::detail::note_visible(game); }

        void give(const char* id) {
            test::put_in_hand(game, me, id);
            seen();
        }
    };

    IsMctsOptions small(int iterations) {
        IsMctsOptions o;
        o.iterations = iterations;
        o.macros.searcher_budget = 100;
        o.macros.searcher_plans = 6;
        o.macros.opponent_budget = 25;
        o.macros.opponent_plans = 3;
        return o;
    }

    bool legal_plan(const GameState& game, const pokebot::Plan& plan) {
        GameState walk = game;
        for (const pokebot::Action& action : plan.actions) {
            const auto actions = pokebot::legal_actions(walk);
            if (std::find(actions.begin(), actions.end(), action) == actions.end()) {
                return false;
            }
            pokebot::apply_legal(walk, action);
        }
        return !plan.actions.empty();
    }

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

    void same_search(const std::vector<RootChoice>& a, const std::vector<RootChoice>& b) {
        REQUIRE(a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i) {
            CHECK(a[i].plan.actions == b[i].plan.actions);
            CHECK(a[i].visits == b[i].visits);
            CHECK(a[i].mean == b[i].mean);
        }
    }

} // namespace

TEST_CASE("IS-MCTS's most visited plan takes a win two actions away", "[is_mcts]") {
    Fixture f;
    test::clear_bench(f.game, f.them);
    test::set_active(f.game, f.me, "TST-5");
    test::attach_from_deck(f.game, f.me, pokebot::bench_slot(0), "TST-2");
    f.game.player(f.them).active->damage_counters = 3;
    f.seen();
    const DeckModel model(f.library);
    IsMcts search(model, 1, small(40));
    const std::vector<RootChoice> choices = search.search(pokebot::observe(f.game, f.me));
    REQUIRE(!choices.empty());
    const GameState after = pokebot::apply_plan(f.game, choices.front().plan);
    CHECK(after.result == (f.me == 0 ? pokebot::GameResult::Player0Wins : pokebot::GameResult::Player1Wins));
}

TEST_CASE("every IS-MCTS iteration passes through one root move, and the same seed searches the same", "[is_mcts]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-4");
    const DeckModel model(f.library);
    IsMcts search(model, 7, small(60));
    const std::vector<RootChoice> choices = search.search(pokebot::observe(f.game, f.me));
    REQUIRE(choices.size() > 1);
    long long visits = 0;
    for (std::size_t i = 0; i < choices.size(); ++i) {
        visits += choices[i].visits;
        CHECK(choices[i].mean >= -1.0);
        CHECK(choices[i].mean <= 1.0);
        CHECK(legal_plan(f.game, choices[i].plan));
        if (i > 0) {
            CHECK(choices[i - 1].visits >= choices[i].visits);
        }
    }
    CHECK(visits == 60);
    CHECK(search.stats().iterations == 60);
    CHECK(search.stats().nodes <= 61);
    CHECK(search.stats().macros.cache_hits > 0);

    IsMcts again(model, 7, small(60));
    same_search(choices, again.search(pokebot::observe(f.game, f.me)));
}

TEST_CASE("on the real decks, IS-MCTS searches positions from both seats", "[is_mcts]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::filesystem::path root = POKEBOT_SOURCE_DIR;
    const ArchetypeLibrary library = ArchetypeLibrary::load(root / "data/archetypes", real->db);
    const DeckModel model(library);
    const CardTable table = CardTable(real->db, real->decks[0], real->decks[1]).shuffled(5);
    GameState game = pokebot::new_game(table, 5);
    pokebot::BiasedRandomAgent agent(5);
    int searched = 0;
    for (int step = 0; step < 300 && !game.is_over() && searched < 6; ++step) {
        const auto actions = pokebot::legal_actions(game);
        if (game.turn > 0 && actions.size() > 1 && step % 15 == 0) {
            INFO("step " << step);
            const int me = game.decision().player;
            IsMcts search(model, static_cast<std::uint64_t>(step), small(20));
            const std::vector<RootChoice> choices = search.search(pokebot::observe(game, me));
            REQUIRE(!choices.empty());
            long long visits = 0;
            for (const RootChoice& choice : choices) {
                CHECK(legal_plan(game, choice.plan));
                visits += choice.visits;
            }
            CHECK((visits == 20 || (choices.size() == 1 && visits == 0)));
            ++searched;
        }
        pokebot::apply_legal(game, agent.choose(pokebot::observe(game, game.decision().player), actions));
    }
    CHECK(searched >= 3);
}

TEST_CASE("on the real decks, games that differ only in hidden cards get the same IS-MCTS search", "[is_mcts]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::filesystem::path root = POKEBOT_SOURCE_DIR;
    const ArchetypeLibrary library = ArchetypeLibrary::load(root / "data/archetypes", real->db);
    const DeckModel model(library);
    const std::size_t n = real->decks.size();
    pokebot::Rng chooser(21);
    int compared = 0;
    for (std::uint64_t seed = 0; seed < 8 && compared < 3; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table = CardTable(real->db, real->decks[i % n], real->decks[(i / n) % n]).shuffled(~seed);
        pokebot::BiasedRandomAgent agent(seed);
        GameState game = pokebot::new_game(table, seed);
        const int stop = static_cast<int>(chooser.below(60)) + 20;
        for (int step = 0; !game.is_over() && (step < stop || game.turn == 0 || game.decision().kind != pokebot::DecisionKind::MainPhase); ++step) {
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply_legal(game, agent.choose(pokebot::observe(game, game.decision().player), actions));
        }
        if (game.is_over()) {
            continue;
        }
        const int viewer = game.decision().player;
        GameState changed = game;
        auto& theirs = changed.player(pokebot::opponent_of(viewer));
        auto& mine = changed.player(viewer);
        const bool hand = swap_hidden(game, viewer, theirs.hand, theirs.deck);
        const bool prizes = swap_hidden(game, viewer, mine.prizes, mine.deck);
        if (!hand && !prizes) {
            continue;
        }
        INFO("seed " << seed);
        REQUIRE(pokebot::observe(changed, viewer) == pokebot::observe(game, viewer));
        IsMcts a(model, seed, small(15));
        IsMcts b(model, seed, small(15));
        same_search(a.search(pokebot::observe(game, viewer)), b.search(pokebot::observe(changed, viewer)));
        ++compared;
    }
    CHECK(compared > 1);
}
