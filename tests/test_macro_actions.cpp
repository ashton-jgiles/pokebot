#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "agents/is_mcts/macro_actions.hpp"
#include "agents/random/random_agent.hpp"
#include "engine/engine.hpp"
#include "engine/observation/observation.hpp"
#include "harness/budget_lab.hpp"
#include "support/real_decks.hpp"
#include "support/test_cards.hpp"

using pokebot::CardDB;
using pokebot::CardDef;
using pokebot::CardTable;
using pokebot::GameState;
using pokebot::MacroActions;
using pokebot::MacroOptions;
using pokebot::Plan;

namespace {

    bool legal_plan(const GameState& game, const Plan& plan) {
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

    bool planning(const GameState& game) {
        const auto kind = game.decision().kind;
        return game.turn > 0 && (kind == pokebot::DecisionKind::MainPhase || kind == pokebot::DecisionKind::ExtraAttack);
    }

} // namespace

TEST_CASE("a finished game has no moves", "[is_mcts]") {
    CardDB db;
    CardDef mon = test::basic_pokemon("TST-1", "Attackmon", 30);
    mon.attacks.push_back(test::attack("Tackle", {pokebot::EnergyType::Colorless}, 30));
    db.add(mon);
    db.add(test::basic_energy("TST-6", "Grass Energy", pokebot::EnergyType::Grass));
    const auto deck = test::deck({{4, 1}, {56, 6}});
    const CardTable table(db, deck, deck);
    GameState game = pokebot::new_game(table, 1);
    pokebot::Rng chooser(1);
    while (!game.is_over()) {
        const auto actions = pokebot::legal_actions(game);
        pokebot::apply(game, actions[chooser.below(static_cast<std::uint32_t>(actions.size()))]);
    }
    MacroActions macros(0, 1);
    CHECK(macros.moves(game).empty());
}

TEST_CASE("on the real decks, every node's moves are legal plans of the right kind", "[is_mcts]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    MacroOptions options;
    options.searcher_budget = 100;
    options.searcher_plans = 6;
    options.opponent_budget = 30;
    options.opponent_plans = 3;
    const std::size_t n = real->decks.size();
    int checked = 0;
    for (std::uint64_t seed = 0; seed < 3; ++seed) {
        const auto i = static_cast<std::size_t>(seed);
        const CardTable table = CardTable(real->db, real->decks[i % n], real->decks[(i + 1) % n]).shuffled(~seed);
        GameState game = pokebot::new_game(table, seed);
        pokebot::BiasedRandomAgent agent(seed);
        MacroActions macros(0, seed, options);
        for (int step = 0; step < 300 && !game.is_over(); ++step) {
            if (game.turn > 0 && step % 5 == 0) {
                INFO("seed " << seed << ", step " << step);
                const std::vector<Plan> moves = macros.moves(game);
                REQUIRE(!moves.empty());
                for (const Plan& plan : moves) {
                    CHECK(legal_plan(game, plan));
                }
                if (planning(game)) {
                    const int most = game.decision().player == 0 ? options.searcher_plans : options.opponent_plans;
                    CHECK(moves.size() <= static_cast<std::size_t>(std::max(most, static_cast<int>(pokebot::legal_actions(game).size()))));
                } else {
                    CHECK(moves.size() == pokebot::legal_actions(game).size());
                    for (const Plan& plan : moves) {
                        CHECK(plan.actions.size() == 1);
                    }
                }
                ++checked;
            }
            const auto actions = pokebot::legal_actions(game);
            pokebot::apply_legal(game, agent.choose(pokebot::observe(game, game.decision().player), actions));
        }
    }
    CHECK(checked > 30);
}

TEST_CASE("on the real decks, the searcher's plans are worked out once for each information set", "[is_mcts]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    MacroOptions options;
    options.searcher_budget = 100;
    options.opponent_budget = 30;
    const CardTable table = CardTable(real->db, real->decks[0], real->decks[1]).shuffled(3);
    GameState game = pokebot::new_game(table, 3);
    pokebot::BiasedRandomAgent agent(3);
    while (!game.is_over() && !(planning(game) && game.decision().player == 0)) {
        const auto actions = pokebot::legal_actions(game);
        pokebot::apply_legal(game, agent.choose(pokebot::observe(game, game.decision().player), actions));
    }
    REQUIRE(!game.is_over());
    MacroActions macros(0, 3, options);
    const std::vector<Plan> first = macros.moves(game);
    const std::vector<Plan> again = macros.moves(game);
    CHECK(macros.stats().searcher_searches == 1);
    CHECK(macros.stats().cache_hits == 1);
    REQUIRE(first.size() == again.size());
    for (std::size_t i = 0; i < first.size(); ++i) {
        CHECK(first[i].actions == again[i].actions);
    }

    MacroActions other_side(1, 3, options);
    other_side.moves(game);
    other_side.moves(game);
    CHECK(other_side.stats().opponent_searches == 2);
    CHECK(other_side.stats().cache_hits == 0);
}

TEST_CASE("on the real decks, the budget lab finds cheap budgets never beat the reference", "[is_mcts]") {
    const auto real = test::load_real_decks();
    if (!real) {
        SKIP("no generated card data; run pokebot-import first");
    }
    std::vector<pokebot::ArenaDeck> decks;
    for (const auto& list : real->decks) {
        decks.push_back(pokebot::ArenaDeck{"deck", list});
    }
    pokebot::BudgetLabOptions options;
    options.games = 1;
    options.every = 4;
    options.reference = 150;
    options.budgets = {25, 150};
    const pokebot::BudgetLabResult result = pokebot::run_budget_lab(real->db, decks, options);
    REQUIRE(result.positions > 0);
    REQUIRE(result.rows.size() == 2);
    CHECK(result.rows[0].gap >= -1e-9);
    CHECK(result.rows[0].same_best <= result.reference.same_best);
    CHECK(result.rows[1].same_best == result.reference.same_best);
    CHECK(result.rows[1].gap <= 1e-9);
}
