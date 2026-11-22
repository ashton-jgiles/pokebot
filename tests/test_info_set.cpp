#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/flat_mc/deck_model.hpp"
#include "agents/is_mcts/info_set.hpp"
#include "agents/random/random_agent.hpp"
#include "agents/world.hpp"
#include "engine/engine.hpp"
#include "engine/knowledge/knowledge.hpp"
#include "engine/observation/observation.hpp"
#include "support/real_decks.hpp"

using pokebot::Action;
using pokebot::ActionKind;
using pokebot::ArchetypeLibrary;
using pokebot::CardTable;
using pokebot::DeckModel;
using pokebot::GameState;
using pokebot::hash_observation;
using pokebot::Observation;
using pokebot::observe;
using pokebot::Rng;
using pokebot::World;

namespace {

    // A real-deck game played by biased random agents for `steps` actions after setup.
    GameState played(const CardTable& table, std::uint64_t seed, int steps) {
        GameState game = pokebot::new_game(table, seed);
        pokebot::BiasedRandomAgent agent(seed);
        for (int step = 0; (step < steps || game.turn == 0) && !game.is_over(); ++step) {
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply_legal(game, agent.choose(observe(game, game.decision().player), actions));
        }
        return game;
    }

    // The first of those games, from `seed` on, that's still going with both Active Pokemon in play
    // and at least two cards in player 0's hand.
    GameState ongoing(const CardTable& table, std::uint64_t seed, int steps) {
        for (;; ++seed) {
            GameState game = played(table, seed, steps);
            if (!game.is_over() && game.player(0).active && game.player(1).active && game.player(0).hand.size() > 1) {
                return game;
            }
        }
    }

} // namespace

TEST_CASE("equal observations hash the same, whatever the order of the hand", "[is_mcts]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const CardTable table = CardTable(real->db, real->decks[0], real->decks[1]).shuffled(3);
    int checked = 0;
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        const GameState game = played(table, seed, 40);
        if (game.is_over()) {
            continue;
        }
        for (int viewer = 0; viewer < pokebot::kPlayers; ++viewer) {
            const Observation view = observe(game, viewer);
            CHECK(hash_observation(view) == hash_observation(observe(game, viewer)));
            Observation reordered = view;
            auto& hand = reordered.players[static_cast<std::size_t>(viewer)].hand;
            if (hand.size() > 1) {
                std::reverse(hand.begin(), hand.end());
                CHECK(hash_observation(reordered) == hash_observation(view));
                ++checked;
            }
        }
    }
    CHECK(checked > 5);
}

TEST_CASE("the same move in two worlds hashes the same, though the opponent's cards are numbered differently", "[is_mcts]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const std::filesystem::path root = POKEBOT_SOURCE_DIR;
    const ArchetypeLibrary library = ArchetypeLibrary::load(root / "data/archetypes", real->db);
    const DeckModel model(library);
    const CardTable table = CardTable(real->db, real->decks[0], real->decks[1]).shuffled(5);
    int compared = 0;
    for (std::uint64_t seed = 0; seed < 40 && compared < 10; ++seed) {
        GameState game = played(table, seed, 30);
        while (!game.is_over() && game.decision().kind != pokebot::DecisionKind::MainPhase) {
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply_legal(game, actions.front());
        }
        if (game.is_over()) {
            continue;
        }
        const int opponent = game.decision().player;
        const int viewer = pokebot::opponent_of(opponent);
        const Observation view = observe(game, viewer);
        Rng a(seed);
        Rng b(seed + 1000);
        const World first(view, a, &model);
        const World second(view, b, &model);
        const auto energy_attach = [&](const GameState& world) {
            for (const Action& action : pokebot::legal_actions(world)) {
                if (action.kind == ActionKind::AttachEnergy && action.target == pokebot::kActiveSlot && !pokebot::knows_where(game, viewer, action.card)) {
                    return std::pair{true, action};
                }
            }
            return std::pair{false, Action{}};
        };
        const auto [found_first, attach_first] = energy_attach(first.game());
        const auto [found_second, attach_second] = energy_attach(second.game());
        if (!found_first || !found_second || attach_first.card == attach_second.card ||
            first.game().def(attach_first.card).id != second.game().def(attach_second.card).id) {
            continue;
        }
        GameState x = first.game();
        GameState y = second.game();
        pokebot::apply_legal(x, attach_first);
        pokebot::apply_legal(y, attach_second);
        INFO("seed " << seed);
        CHECK(observe(x, viewer) != observe(y, viewer));
        CHECK(hash_observation(observe(x, viewer)) == hash_observation(observe(y, viewer)));
        ++compared;
    }
    CHECK(compared > 3);
}

TEST_CASE("a change the viewer can see changes the hash", "[is_mcts]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const CardTable table = CardTable(real->db, real->decks[0], real->decks[1]).shuffled(7);
    const GameState game = ongoing(table, 2, 40);
    const Observation view = observe(game, 0);
    const std::uint64_t base = hash_observation(view);

    Observation damaged = view;
    REQUIRE(damaged.players[1].active.has_value());
    damaged.players[1].active->pokemon.damage_counters += 1;
    CHECK(hash_observation(damaged) != base);

    Observation bigger_hand = view;
    bigger_hand.players[1].hand_size += 1;
    CHECK(hash_observation(bigger_hand) != base);

    Observation other_viewer = view;
    other_viewer.viewer = 1;
    CHECK(hash_observation(other_viewer) != base);

    Observation later = view;
    later.turn += 1;
    CHECK(hash_observation(later) != base);
}

TEST_CASE("the hash tells different cards apart, and an unseen card from a seen one", "[is_mcts]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    const CardTable table = CardTable(real->db, real->decks[0], real->decks[1]).shuffled(9);
    const GameState game = ongoing(table, 4, 40);
    const Observation view = observe(game, 0);
    const auto& hand = view.players[0].hand;
    REQUIRE(hand.size() > 1);
    const std::uint64_t base = hash_observation(view);

    Observation hidden = view;
    hidden.defs[hand[0]] = nullptr;
    CHECK(hash_observation(hidden) != base);

    for (std::size_t i = 1; i < hand.size(); ++i) {
        if (view.def(hand[i])->id != view.def(hand[0])->id) {
            Observation swapped = view;
            swapped.defs[hand[0]] = view.defs[hand[i]];
            CHECK(hash_observation(swapped) != base);
            break;
        }
    }
}
