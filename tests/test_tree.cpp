#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "agents/is_mcts/tree.hpp"
#include "engine/shared/rng.hpp"

using Catch::Matchers::WithinAbs;
using pokebot::Plan;
using pokebot::SearchTree;

namespace {

    Plan move(int id) {
        Plan plan;
        plan.actions.push_back(pokebot::Action{pokebot::ActionKind::EndTurn, pokebot::kNoCard, pokebot::kActiveSlot, static_cast<std::uint8_t>(id)});
        return plan;
    }

    // A two-level game: player 0 picks L (0) or R (1), then player 1 picks l (0) or r (1). The
    // value for player 0 is payoff[first][second], plus noise. Player 0's move 2, when it's
    // offered, ends the game at once worth 1.
    constexpr double kPayoff[2][2] = {{0.9, -0.5}, {0.2, 0.3}};

    // Runs `iterations` of select or expand, evaluate and backpropagate, the way the search will.
    // `available(first)` says whether player 0's move `first` can be played this iteration.
    template <class Available>
    void search(SearchTree& tree, int iterations, double exploration, Available available) {
        pokebot::Rng rng(17);
        for (int i = 0; i < iterations; ++i) {
            std::vector<std::size_t> path{tree.root()};
            std::size_t node = tree.root();
            int first = -1;
            int second = -1;
            for (int depth = 0; depth < 2; ++depth) {
                std::vector<std::size_t> legal;
                int untried = -1;
                const int moves = depth == 0 ? 3 : 2;
                for (int m = 0; m < moves; ++m) {
                    if (depth == 0 && !available(m)) {
                        continue;
                    }
                    const std::size_t child = tree.find_child(node, static_cast<std::uint64_t>(m + 1));
                    if (child == SearchTree::kNone) {
                        untried = untried < 0 ? m : untried;
                    } else {
                        legal.push_back(child);
                    }
                }
                tree.mark_available(legal);
                const std::size_t next = untried >= 0 ? tree.add_child(node, static_cast<std::uint64_t>(untried + 1), depth == 0 ? 1 : 0, move(untried))
                                                      : tree.select(node, legal, exploration);
                const int chosen = tree.node(next).move.actions.front().amount;
                if (depth == 0) {
                    first = chosen;
                } else {
                    second = chosen;
                }
                path.push_back(next);
                node = next;
                if (untried >= 0 || first == 2) {
                    break;
                }
            }
            if (second < 0) {
                second = static_cast<int>(rng.below(2));
            }
            const double base = first == 2 ? 1.0 : kPayoff[first][second];
            const double noise = (static_cast<double>(rng.below(201)) - 100.0) / 1000.0;
            tree.backpropagate(path, base + noise);
        }
    }

} // namespace

TEST_CASE("a tree starts as its root, and children are found by key", "[is_mcts]") {
    SearchTree tree(42, 0);
    CHECK(tree.size() == 1);
    CHECK(tree.node(tree.root()).key == 42);
    CHECK(tree.find_child(tree.root(), 7) == SearchTree::kNone);
    const std::size_t a = tree.add_child(tree.root(), 7, 1, move(0));
    const std::size_t b = tree.add_child(tree.root(), 8, 1, move(1));
    CHECK(tree.size() == 3);
    CHECK(tree.find_child(tree.root(), 7) == a);
    CHECK(tree.find_child(tree.root(), 8) == b);
    CHECK(tree.node(a).availability == 1);
    CHECK(tree.node(a).actor == 1);
    CHECK_THROWS_AS(tree.add_child(tree.root(), 7, 1, move(2)), std::logic_error);
    CHECK(tree.most_visited(a) == SearchTree::kNone);
}

TEST_CASE("backpropagation adds a visit and the value for player 0, negated for player 1", "[is_mcts]") {
    SearchTree tree(1, 0);
    const std::size_t child = tree.add_child(tree.root(), 2, 1, move(0));
    tree.backpropagate({tree.root(), child}, 0.5);
    tree.backpropagate({tree.root(), child}, -0.25);
    tree.backpropagate({tree.root()}, 1.0);
    CHECK(tree.node(tree.root()).visits == 3);
    CHECK(tree.node(child).visits == 2);
    CHECK_THAT(tree.node(child).mean(0), WithinAbs(0.125, 1e-12));
    CHECK_THAT(tree.node(child).mean(1), WithinAbs(-0.125, 1e-12));
    CHECK_THAT(tree.node(tree.root()).mean(0), WithinAbs(1.25 / 3.0, 1e-12));
}

TEST_CASE("UCB1 tries unvisited children first, then the best mean for the node's actor", "[is_mcts]") {
    SearchTree tree(1, 0);
    const std::size_t good = tree.add_child(tree.root(), 2, 1, move(0));
    const std::size_t bad = tree.add_child(tree.root(), 3, 1, move(1));
    const std::size_t fresh = tree.add_child(tree.root(), 4, 1, move(2));
    tree.backpropagate({tree.root(), good}, 0.8);
    tree.backpropagate({tree.root(), bad}, -0.6);
    CHECK(tree.select(tree.root(), {good, bad, fresh}, 1.0) == fresh);
    CHECK(tree.select(tree.root(), {good, bad}, 0.0) == good);
    CHECK_THROWS_AS(tree.select(tree.root(), {}, 1.0), std::invalid_argument);

    SearchTree opponent(1, 1);
    const std::size_t high = opponent.add_child(opponent.root(), 2, 0, move(0));
    const std::size_t low = opponent.add_child(opponent.root(), 3, 0, move(1));
    opponent.backpropagate({opponent.root(), high}, 0.8);
    opponent.backpropagate({opponent.root(), low}, -0.6);
    CHECK(opponent.select(opponent.root(), {high, low}, 0.0) == low);
}

TEST_CASE("the exploration term grows with a child's availability", "[is_mcts]") {
    SearchTree tree(1, 0);
    const std::size_t rare = tree.add_child(tree.root(), 2, 1, move(0));
    const std::size_t common = tree.add_child(tree.root(), 3, 1, move(1));
    tree.backpropagate({tree.root(), rare}, 0.0);
    tree.backpropagate({tree.root(), common}, 0.0);
    for (int i = 0; i < 20; ++i) {
        tree.mark_available({common});
    }
    CHECK(tree.node(common).availability == 21);
    CHECK(tree.node(rare).availability == 1);
    CHECK(tree.select(tree.root(), {rare, common}, 1.0) == common);
}

TEST_CASE("on a small game, visits concentrate on the minimax moves", "[is_mcts]") {
    SearchTree tree(0, 0);
    search(tree, 3000, 0.7, [](int first) { return first < 2; });
    const std::size_t choice = tree.most_visited(tree.root());
    REQUIRE(choice != SearchTree::kNone);
    CHECK(tree.node(choice).move.actions.front().amount == 1);
    const std::size_t reply = tree.most_visited(choice);
    REQUIRE(reply != SearchTree::kNone);
    CHECK(tree.node(reply).move.actions.front().amount == 0);
    CHECK(tree.node(choice).visits > 2 * (tree.node(tree.root()).visits - tree.node(choice).visits));
}

TEST_CASE("a move that's only sometimes legal is counted available only when it is", "[is_mcts]") {
    SearchTree tree(0, 0);
    int iteration = 0;
    search(tree, 1000, 0.7, [&](int first) {
        if (first == 0) {
            ++iteration;
        }
        return first < 2 || iteration % 3 == 0;
    });
    const std::size_t rare = tree.find_child(tree.root(), 3);
    REQUIRE(rare != SearchTree::kNone);
    const auto& node = tree.node(rare);
    CHECK(node.visits <= node.availability);
    CHECK(node.availability < tree.node(tree.root()).visits / 2);
    CHECK(node.mean(0) > 0.8);
}
