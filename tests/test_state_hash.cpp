#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "agents/random/random_agent.hpp"
#include "agents/planner/state_hash.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "harness/match.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::GameState;
using pokebot::hash_state;
using pokebot::kActiveSlot;

namespace {

    CardDef tackler(const char* id, const char* name) {
        CardDef c = test::basic_pokemon(id, name);
        pokebot::Attack tackle;
        tackle.name = "Tackle";
        tackle.cost = {pokebot::EnergyType::Colorless};
        tackle.damage = 30;
        c.attacks.push_back(tackle);
        return c;
    }

    // TST-1 and TST-3 Tacklers, TST-2 Energy, TST-5 Big Tackler (Stage 1 from Tackler).
    CardDB sample_db() {
        CardDB db;
        db.add(tackler("TST-1", "Tackler"));
        db.add(test::basic_energy("TST-2", "Darkness Energy", pokebot::EnergyType::Darkness));
        db.add(tackler("TST-3", "Tackler B"));
        db.add(test::evolution("TST-5", "Big Tackler", pokebot::Stage::Stage1, "Tackler"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 3}, {4, 5}, {48, 2}});

    // Turn 4: each player has a Tackler Active and one on the Bench; you hold an Energy, a Tackler B and a Big Tackler.
    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 2);
        int me = 0;

        Fixture() {
            test::play_setup(game);
            test::end_turns(game, 3);
            me = game.current_player;
            for (const int p : {me, pokebot::opponent_of(me)}) {
                test::clear_bench(game, p);
                test::set_active(game, p, "TST-1");
                test::add_to_bench(game, p, "TST-3");
            }
            for (const char* id : {"TST-2", "TST-3", "TST-5"}) {
                test::put_in_hand(game, me, id);
            }
            pokebot::detail::note_visible(game);
        }
        Fixture(const Fixture&) = delete;
        Fixture& operator=(const Fixture&) = delete;

        // Plays the actions in order, each the first legal one of its kind with that card (and target).
        GameState after(std::initializer_list<std::pair<ActionKind, const char*>> actions) const {
            GameState next = game;
            for (const auto& [kind, id] : actions) {
                const int target = kind == ActionKind::AttachEnergy ? kActiveSlot : -1;
                pokebot::apply(next, test::find_action(next, kind, id, target));
            }
            return next;
        }
    };

    // The state with the random number generator and the order of each hand left out, as hash_state does.
    GameState normalized(GameState game) {
        game.rng = pokebot::Rng{};
        for (auto& player : game.players) {
            std::sort(player.hand.begin(), player.hand.end());
        }
        return game;
    }

} // namespace

TEST_CASE("the same actions in either order reach the same state and the same hash", "[planner]") {
    Fixture f;
    const GameState attach_first = f.after({{ActionKind::AttachEnergy, "TST-2"}, {ActionKind::PlayBasic, "TST-3"}});
    const GameState bench_first = f.after({{ActionKind::PlayBasic, "TST-3"}, {ActionKind::AttachEnergy, "TST-2"}});
    CHECK(normalized(attach_first) == normalized(bench_first));
    CHECK(hash_state(attach_first) == hash_state(bench_first));

    const GameState evolve_first = f.after({{ActionKind::Evolve, "TST-5"}, {ActionKind::AttachEnergy, "TST-2"}});
    const GameState energy_first = f.after({{ActionKind::AttachEnergy, "TST-2"}, {ActionKind::Evolve, "TST-5"}});
    CHECK(hash_state(evolve_first) == hash_state(energy_first));
    CHECK(hash_state(evolve_first) != hash_state(attach_first));
}

TEST_CASE("the hash changes with what decides the game, not with the random number generator or the hand's order", "[planner]") {
    Fixture f;
    const std::uint64_t base = hash_state(f.game);
    const auto changed = [&](auto change) {
        GameState game = f.game;
        change(game);
        return hash_state(game) != base;
    };
    auto& mine = f.game.player(f.me);
    REQUIRE(mine.deck.size() >= 2);

    CHECK(changed([&](GameState& g) { g.player(f.me).active->damage_counters = 1; }));
    CHECK(changed([&](GameState& g) { g.flags.energy_attached = true; }));
    CHECK(changed([&](GameState& g) { g.player(f.me).discard.push_back(g.player(f.me).hand.pop_back()); }));
    CHECK(changed([&](GameState& g) {
        auto& deck = g.player(f.me).deck;
        const auto other = std::find_if(deck.begin(), deck.end(), [&](auto card) { return g.def(card).id != g.def(deck[0]).id; });
        if (other != deck.end()) {
            std::swap(deck[0], *other); // two different cards change places
        }
    }));
    CHECK(changed([&](GameState& g) { g.pending.back().amount = 7; }));
    CHECK(changed([&](GameState& g) { g.knowledge.located[static_cast<std::size_t>(f.me)].erase(g.player(f.me).hand[0]); }));
    CHECK(changed([&](GameState& g) { g.turn = static_cast<std::uint16_t>(g.turn + 1); }));

    CHECK_FALSE(changed([&](GameState& g) { g.rng.next(); }));
    CHECK_FALSE(changed([&](GameState& g) { std::reverse(g.player(f.me).hand.begin(), g.player(f.me).hand.end()); }));
}

TEST_CASE("on the real decks, states with the same hash are the same state", "[planner]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::size_t n = real->decks.size();
    long long states = 0;
    long long repeats = 0;
    for (std::uint64_t seed = 0; seed < 16; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table = CardTable(real->db, real->decks[i % n], real->decks[(i / n) % n]).shuffled(~seed);
        pokebot::BiasedRandomAgent agent(seed);
        GameState game = pokebot::new_game(table, seed);
        std::unordered_map<std::uint64_t, GameState> seen;
        for (int step = 0; step < 3000 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply_legal(game, pokebot::ask(agent, game, actions));
            const auto [found, added] = seen.try_emplace(hash_state(game), game);
            ++states;
            if (!added) {
                INFO("seed " << seed << ", step " << step);
                REQUIRE(normalized(found->second) == normalized(game));
                ++repeats;
            }
        }
    }
    INFO(states << " states, " << repeats << " repeated");
    CHECK(states > 1000);
}

TEST_CASE("a closed set holds each state once", "[planner]") {
    Fixture f;
    pokebot::ClosedSet closed;
    CHECK_FALSE(closed.contains(f.game));
    CHECK(closed.insert(f.game));
    CHECK(closed.contains(f.game));
    GameState same = f.game;
    same.rng.next();
    CHECK_FALSE(closed.insert(same)); // differs only in the random number generator
    CHECK(closed.insert(f.after({{ActionKind::AttachEnergy, "TST-2"}})));
    CHECK(closed.size() == 2);
    closed.clear();
    CHECK(closed.size() == 0);
}
