#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/flat_mc/deck_model.hpp"
#include "agents/flat_mc/flat_mc_agent.hpp"
#include "agents/random/random_agent.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "engine/observation/observation.hpp"
#include "harness/match.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using pokebot::Action;
using pokebot::ArchetypeLibrary;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::DeckModel;
using pokebot::FlatMcAgent;
using pokebot::FlatMcOptions;
using pokebot::GameState;

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

        void play_turn(pokebot::Agent& agent) {
            const auto turn = game.turn;
            while (!game.is_over() && game.turn == turn && game.decision().player == me) {
                const auto actions = pokebot::legal_actions(game);
                pokebot::apply(game, agent.choose(pokebot::observe(game, me), actions));
            }
        }
    };

    FlatMcOptions small(int plans, int playouts) {
        FlatMcOptions o;
        o.plans = plans;
        o.playouts = playouts;
        return o;
    }

    FlatMcOptions quick() {
        FlatMcOptions o = small(2, 4);
        o.or_budget = 50;
        o.and_budget = 100;
        o.playout.policy = pokebot::PlayoutPolicy::Biased;
        return o;
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

} // namespace

TEST_CASE("flat MC takes a win two actions away without playing out", "[flat_mc]") {
    Fixture f;
    test::clear_bench(f.game, f.them);
    test::set_active(f.game, f.me, "TST-5");
    test::attach_from_deck(f.game, f.me, pokebot::bench_slot(0), "TST-2");
    f.game.player(f.them).active->damage_counters = 3;
    f.seen();
    const DeckModel model(f.library);
    FlatMcAgent agent(1, model);
    f.play_turn(agent);
    CHECK(f.game.result == (f.me == 0 ? pokebot::GameResult::Player0Wins : pokebot::GameResult::Player1Wins));
    CHECK(agent.stats().wins == 1);
    CHECK(agent.stats().playouts == 0);
}

TEST_CASE("flat MC plays out each of its plans and plays the one with the best mean", "[flat_mc]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-4");
    const DeckModel model(f.library);
    FlatMcAgent agent(1, model, small(4, 8));
    f.play_turn(agent);
    const auto& ranked = agent.analysis();
    REQUIRE(ranked.size() > 1);
    const int each = std::max(1, 8 / static_cast<int>(ranked.size()));
    for (std::size_t i = 0; i < ranked.size(); ++i) {
        CHECK(ranked[i].playouts == each);
        CHECK(ranked[i].mean >= -1.0);
        CHECK(ranked[i].mean <= 1.0);
        if (i > 0) {
            CHECK(ranked[i - 1].mean >= ranked[i].mean);
        }
    }
    CHECK(agent.stats().searched == 1);
    CHECK(agent.stats().plans == 1);
    CHECK(agent.stats().playouts == static_cast<long long>(ranked.size()) * each);
    CHECK(agent.stats().followed == static_cast<long long>(ranked.front().plan.actions.size()));
}

TEST_CASE("with one plan flat MC plays the and-tree's best plan without playing out", "[flat_mc]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-4");
    const DeckModel model(f.library);
    FlatMcAgent agent(1, model, small(1, 32));
    f.play_turn(agent);
    CHECK(f.game.player(f.me).prizes.size() == 5);
    CHECK(agent.stats().searched == 0);
    CHECK(agent.stats().playouts == 0);
    CHECK(agent.stats().followed == 3);
}

TEST_CASE("flat MC plays whole games on the sample cards, the same for the same seed", "[flat_mc]") {
    const CardDB db = sample_db();
    ArchetypeLibrary library;
    library.add("sample", "main", kDeck, db);
    const DeckModel model(library);
    const CardTable table(db, kDeck, kDeck);
    const auto play = [&](std::uint64_t seed) {
        FlatMcAgent agent(seed, model, small(3, 6));
        pokebot::RandomAgent random(seed);
        return pokebot::play_game(table, seed, agent, random);
    };
    for (std::uint64_t seed = 0; seed < 3; ++seed) {
        INFO("seed " << seed);
        const auto record = play(seed);
        CHECK(record.end != pokebot::GameEnd::ActionLimit);
        CHECK(play(seed).moves == record.moves);
    }
}

TEST_CASE("on the real decks, flat MC plays whole games against random", "[flat_mc]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::filesystem::path root = POKEBOT_SOURCE_DIR;
    const ArchetypeLibrary library = ArchetypeLibrary::load(root / "data/archetypes", real->db);
    const DeckModel model(library);
    for (std::uint64_t seed = 0; seed < 2; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table(real->db, real->decks[i % real->decks.size()], real->decks[(i + 1) % real->decks.size()]);
        FlatMcAgent agent(seed, model, quick());
        pokebot::RandomAgent random(seed);
        INFO("seed " << seed);
        const auto record = pokebot::play_game(table, seed, agent, random);
        CHECK(record.end != pokebot::GameEnd::ActionLimit);
        CHECK(agent.stats().plans > 0);
        CHECK(agent.stats().playouts > 0);
    }
}

TEST_CASE("on the real decks, games that differ only in hidden cards get the same choice from flat MC", "[flat_mc]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::filesystem::path root = POKEBOT_SOURCE_DIR;
    const ArchetypeLibrary library = ArchetypeLibrary::load(root / "data/archetypes", real->db);
    const DeckModel model(library);
    const std::size_t n = real->decks.size();
    pokebot::Rng chooser(12);
    int compared = 0;
    for (std::uint64_t seed = 0; seed < 8; ++seed) {
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
        INFO("seed " << seed);
        GameState changed = game;
        auto& theirs = changed.player(pokebot::opponent_of(viewer));
        auto& mine = changed.player(viewer);
        const bool hand = swap_hidden(game, viewer, theirs.hand, theirs.deck);
        const bool prizes = swap_hidden(game, viewer, mine.prizes, mine.deck);
        if (!hand && !prizes) {
            continue;
        }
        REQUIRE(pokebot::observe(changed, viewer) == pokebot::observe(game, viewer));
        const auto actions = pokebot::legal_actions(game);
        FlatMcAgent a(seed, model, quick());
        FlatMcAgent b(seed, model, quick());
        CHECK(a.choose(pokebot::observe(game, viewer), actions) == b.choose(pokebot::observe(changed, viewer), actions));
        REQUIRE(a.analysis().size() == b.analysis().size());
        for (std::size_t k = 0; k < a.analysis().size(); ++k) {
            CHECK(a.analysis()[k].mean == b.analysis()[k].mean);
        }
        ++compared;
    }
    CHECK(compared > 3);
}

TEST_CASE("flat MC counts its searches, playouts and the time they take", "[flat_mc]") {
    Fixture f;
    f.give("TST-2");
    f.give("TST-4");
    const DeckModel model(f.library);
    FlatMcAgent agent(1, model, small(4, 8));
    f.play_turn(agent);
    const auto& stats = agent.stats();
    CHECK(stats.or_nodes > 0);
    CHECK(stats.and_nodes > 0);
    CHECK(stats.playout_actions > 0);
    CHECK(stats.playout_seconds >= 0.0);
    const auto counters = agent.counters();
    REQUIRE(!counters.empty());
    CHECK(counters.front() == std::pair<std::string, double>{"plans", 1.0});
}
