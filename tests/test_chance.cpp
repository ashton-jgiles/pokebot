#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "agents/endgame/chance.hpp"
#include "agents/is_mcts/info_set.hpp"
#include "agents/random/random_agent.hpp"
#include "engine/engine.hpp"
#include "engine/invariants/invariants.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "engine/observation/observation.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"
#include "support/test_game.hpp"

using nlohmann::json;
using pokebot::Action;
using pokebot::ActionKind;
using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::ChanceOptions;
using pokebot::ChanceStats;
using pokebot::GameState;
using pokebot::kActiveSlot;
using pokebot::Outcome;

namespace {

    using Key = std::pair<std::uint64_t, std::uint64_t>;

    CardDef attacker(const char* id, const char* name, int damage, const char* effects) {
        CardDef c = test::basic_pokemon(id, name, 300);
        pokebot::Attack a;
        a.name = "Attack";
        a.cost = {pokebot::EnergyType::Colorless};
        a.damage = damage;
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

    CardDB sample_db() {
        CardDB db;
        db.add(attacker("TST-1", "Tackler", 30, "[]"));
        db.add(test::basic_energy("TST-2", "Darkness Energy", pokebot::EnergyType::Darkness));
        db.add(attacker("TST-3", "Tackler B", 30, "[]"));
        db.add(attacker("TST-4", "Coin Tackler", 30, R"([{"op": "flip_coins"}, {"op": "if", "condition": {"heads_at_least": 1}, "then": [{"op": "damage"}]}])"));
        db.add(attacker("TST-8", "Combo", 10, R"([{"op": "flip_until_tails"}, {"op": "add_damage", "per": 10, "for_each": {"of": "heads"}}, {"op": "damage"}])"));
        db.add(item("TST-6", "Draw Two", R"([{"op": "draw", "count": 2}])"));
        db.add(item("TST-7", "Seek", R"([{"op": "search_deck", "filter": {"kind": "pokemon"}}])"));
        db.add(item("TST-9", "Shuffle and Draw", R"([{"op": "shuffle_hand_into_deck"}, {"op": "draw", "count": 3}])"));
        return db;
    }

    const pokebot::Decklist kDeck = test::deck({{4, 1}, {4, 3}, {4, 4}, {4, 8}, {4, 6}, {4, 7}, {4, 9}, {32, 2}});

    struct Fixture {
        CardDB db = sample_db();
        CardTable table{db, kDeck, kDeck};
        GameState game = pokebot::new_game(table, 2);
        pokebot::Rng rng{5};
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
                pokebot::detail::shuffle_deck(game, p);
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

        void power_up() {
            test::attach_from_deck(game, me, kActiveSlot, "TST-2");
            seen();
        }

        void trim(int player, std::size_t keep) {
            auto& p = game.player(player);
            while (p.deck.size() > keep) {
                p.discard.push_back(p.deck.pop_back());
            }
            seen();
        }

        std::vector<Outcome> outcomes(const Action& action, const ChanceOptions& options = {}, ChanceStats* stats = nullptr) {
            return pokebot::chance_outcomes(game, action, rng, options, stats);
        }
    };

    Key key(const GameState& game) {
        return {pokebot::hash_observation(pokebot::observe(game, 0)), pokebot::hash_observation(pokebot::observe(game, 1))};
    }

    double total(const std::vector<Outcome>& outcomes) {
        double sum = 0.0;
        for (const Outcome& o : outcomes) {
            sum += o.probability;
        }
        return sum;
    }

    bool near(double a, double b) {
        return std::abs(a - b) < 1e-9;
    }

    int damage_on(const GameState& game, int player) {
        return static_cast<int>(game.player(player).active->damage_counters);
    }

} // namespace

TEST_CASE("an action without chance has one outcome, the action's own result", "[endgame]") {
    Fixture f;
    f.give("TST-2");
    const Action attach = test::find_action(f.game, ActionKind::AttachEnergy);
    const auto outcomes = f.outcomes(attach);
    REQUIRE(outcomes.size() == 1);
    CHECK(outcomes[0].probability == 1.0);
    GameState applied = f.game;
    pokebot::apply_legal(applied, attach);
    CHECK(key(outcomes[0].state) == key(applied));
}

TEST_CASE("the opponent's draw at the start of its turn branches on each card, weighted by its copies", "[endgame]") {
    Fixture f;
    f.trim(f.them, 6);
    std::map<const CardDef*, int> copies;
    for (const auto card : f.game.player(f.them).deck) {
        ++copies[&f.game.def(card)];
    }
    ChanceStats stats;
    const auto outcomes = f.outcomes(test::find_action(f.game, ActionKind::EndTurn), {}, &stats);
    CHECK(outcomes.size() == copies.size());
    CHECK(near(total(outcomes), 1.0));
    CHECK(stats.enumerated == 1);
    const auto& before = f.game.player(f.them).hand;
    for (const Outcome& o : outcomes) {
        const auto& hand = o.state.player(f.them).hand;
        REQUIRE(hand.size() == before.size() + 1);
        const auto drawn = std::find_if(hand.begin(), hand.end(), [&](auto card) { return std::find(before.begin(), before.end(), card) == before.end(); });
        REQUIRE(drawn != hand.end());
        CHECK(near(o.probability, copies[&o.state.def(*drawn)] / 6.0));
    }
}

TEST_CASE("drawing two cards gives the same outcomes and probabilities as every order of a small deck", "[endgame]") {
    Fixture f;
    f.trim(f.me, 5);
    f.give("TST-6");
    const Action play = test::find_action(f.game, ActionKind::PlayTrainer, "TST-6");
    std::vector<pokebot::CardIndex> order(f.game.player(f.me).deck.begin(), f.game.player(f.me).deck.end());
    std::sort(order.begin(), order.end());
    std::map<Key, int> seen;
    int orders = 0;
    do {
        GameState game = f.game;
        std::copy(order.begin(), order.end(), game.player(f.me).deck.begin());
        pokebot::apply_legal(game, play);
        ++seen[key(game)];
        ++orders;
    } while (std::next_permutation(order.begin(), order.end()));
    const auto outcomes = f.outcomes(play);
    CHECK(outcomes.size() == seen.size());
    CHECK(near(total(outcomes), 1.0));
    for (const Outcome& o : outcomes) {
        const auto it = seen.find(key(o.state));
        REQUIRE(it != seen.end());
        CHECK(near(o.probability, static_cast<double>(it->second) / orders));
    }
}

TEST_CASE("a coin flip splits the outcomes in half, before the opponent's draw splits them again", "[endgame]") {
    Fixture f;
    test::set_active(f.game, f.me, "TST-4");
    f.power_up();
    ChanceStats stats;
    const auto outcomes = f.outcomes(test::find_action(f.game, ActionKind::Attack), {}, &stats);
    CHECK(stats.flipped == 1);
    CHECK(near(total(outcomes), 1.0));
    std::map<int, double> by_damage;
    for (const Outcome& o : outcomes) {
        by_damage[damage_on(o.state, f.them)] += o.probability;
    }
    REQUIRE(by_damage.size() == 2);
    CHECK(near(by_damage[0], 0.5));
    CHECK(near(by_damage[3], 0.5));
}

TEST_CASE("flipping until tails stops at max_flips, and the probabilities still add up to 1", "[endgame]") {
    Fixture f;
    test::set_active(f.game, f.me, "TST-8");
    f.power_up();
    ChanceOptions options;
    options.max_flips = 3;
    const auto outcomes = f.outcomes(test::find_action(f.game, ActionKind::Attack), options);
    CHECK(near(total(outcomes), 1.0));
    std::map<int, double> by_damage;
    for (const Outcome& o : outcomes) {
        by_damage[damage_on(o.state, f.them)] += o.probability;
    }
    REQUIRE(by_damage.size() == 4);
    CHECK(near(by_damage[1], 0.5));
    CHECK(near(by_damage[2], 0.25));
    CHECK(near(by_damage[3], 0.125));
    CHECK(near(by_damage[4], 0.125));
}

TEST_CASE("a search shows the whole deck, which is one outcome however it's ordered", "[endgame]") {
    Fixture f;
    f.give("TST-7");
    const auto outcomes = f.outcomes(test::find_action(f.game, ActionKind::PlayTrainer, "TST-7"));
    REQUIRE(outcomes.size() == 1);
    CHECK(near(outcomes[0].probability, 1.0));
}

TEST_CASE("shuffling the hand in before drawing is sampled, and the samples add up to 1", "[endgame]") {
    Fixture f;
    f.give("TST-9");
    ChanceStats stats;
    ChanceOptions options;
    options.samples = 6;
    const auto outcomes = f.outcomes(test::find_action(f.game, ActionKind::PlayTrainer, "TST-9"), options, &stats);
    CHECK(stats.sampled == 1);
    CHECK(!outcomes.empty());
    CHECK(outcomes.size() <= 6);
    CHECK(near(total(outcomes), 1.0));
    for (const Outcome& o : outcomes) {
        CHECK(o.state.player(f.me).hand.size() == 3);
    }
}

TEST_CASE("on the real decks, every action's outcomes add up to 1 and keep the game's invariants", "[endgame]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::size_t n = real->decks.size();
    pokebot::Rng rng(3);
    ChanceStats stats;
    for (std::uint64_t seed = 0; seed < 4; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table(real->db, real->decks[i % n], real->decks[(i + 1) % n]);
        pokebot::BiasedRandomAgent agent(seed);
        GameState game = pokebot::new_game(table, seed);
        for (int step = 0; step < 400 && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            if (game.turn > 0 && step % 6 == 0) {
                for (const Action& action : actions) {
                    const auto outcomes = pokebot::chance_outcomes(game, action, rng, {}, &stats);
                    REQUIRE(!outcomes.empty());
                    CHECK(near(total(outcomes), 1.0));
                    for (const Outcome& o : outcomes) {
                        CHECK(pokebot::check_invariants(o.state).empty());
                    }
                }
            }
            pokebot::apply(game, agent.choose(pokebot::observe(game, game.decision().player), actions));
        }
    }
    CHECK(stats.actions > 0);
    CHECK(stats.enumerated > 0);
}
