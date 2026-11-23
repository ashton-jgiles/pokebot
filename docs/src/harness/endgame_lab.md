# `src/harness/endgame_lab` — how big the endgame is

**Files:** [`src/harness/endgame_lab.hpp`](../../../src/harness/endgame_lab.hpp), [`src/harness/endgame_lab.cpp`](../../../src/harness/endgame_lab.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

The endgame solver (M9) searches the rest of the game exactly where it's small enough. This lab
measures how small it is in our games: how many hidden-card worlds a decision has, how much chance
a turn brings, and what a short search costs, so the solver's trigger, its number of sampled worlds
and its depth are chosen from data.

```bash
./build/release/tools/sim/pokebot-sim.exe --endgame-lab 160 --every 5
```

## What it does

It plays `games` Greedy-against-Greedy games over every deck pairing, as the
[budget lab](budget_lab.md) does, and at every main-phase decision records, for the deciding player:

- **Its own worlds:** the ways to split its unseen cards between its deck and its Prize cards.
- **The opponent's worlds:** the ways to fill the opponent's hidden hand and Prize cards from the
  opponent's unseen cards, taken from the opponent's true list (so the count is a lower bound: the
  searcher doesn't know the list).
- The opponent's hidden hand size, the distinct cards left in each deck, and with a
  [deck model](../agents/flat_mc/deck_model.md), how many archetype lists still have belief of at
  least `list_threshold` (0.001).

Worlds are counted by card, not by copy: copies are interchangeable, so two Basic Energy in the
Prize cards are one world however they're numbered. `count_placements(copies, first, second)` gives
the number of ways to pick two disjoint groups of `first` and `second` cards from cards with the given
numbers of copies.

At one in every `every` decisions with at most `tree_prizes` (2) Prize cards left for either
player, it also measures a **two-segment search** in the true world, without pruning:

1. the searcher's top `plans` (16) from the and-tree at `and_budget` (300);
2. for each plan that ends the turn, a branch for each distinct card the opponent could draw,
   weighted by its copies, made by moving a copy to the top of the opponent's deck;
3. in each branch, the opponent's top `opponent_plans` (4) at `opponent_budget` (25);
4. e at the leaves (u if the game is over), the minimum over replies, the expectation over draws, the
   maximum over plans.

Decisions between the segments (a new Active, an effect's choice) go to Greedy. A plan that stops in
the searcher's own turn (it drew or searched) is a leaf here and is counted by the cards it drew; in
the solver those are a chance layer of their own. A plan that used the random generator (a coin
flip or a shuffle) is counted too. The tree's `value` is what a depth-2 solver without pruning
would compute in that world.

| `EndgameLabOptions` | Default | Meaning |
|---|---|---|
| `games`, `seed` | 16, 1 | Games and the first seed |
| `every` | 1 | Which qualifying decisions get a tree |
| `tree_prizes` | 2 | Trees only where either player has at most this many Prize cards left |
| `and_budget`, `plans` | 300, 16 | The searcher's and-tree |
| `opponent_budget`, `opponent_plans` | 25, 4 | The opponent's and-tree |
| `samples` | 1 | The plan scorer's sampled worlds |
| `model` | none | The deck model, for counting lists |

`summarize(result, p)` gives, for decisions with `p` Prize cards left as the smaller count, the
medians (log₁₀ worlds, hidden hand, deck cards, lists), the share of decisions with at most 10², 10⁴,
10⁶ and 10⁸ worlds, and the trees' totals with their median and 90th-percentile seconds.

## In the sim

`--endgame-lab N [--every K] [--endgame-prizes P]` runs it with the sim's `--planner-and-budget`,
`--planner-samples`, `--mcts-plans`, `--mcts-opponent-budget` and `--mcts-opponent-plans`, so the
trees use the moves IS-MCTS uses, and prints a world table and a tree table by Prize cards left.

## Results

A first scratch measurement (400 games, M9's planning) is in the [roadmap](../../roadmap.md); the
step 9.1 run goes here.

## Tests

[`tests/test_endgame_lab.cpp`](../../../tests/test_endgame_lab.cpp), tag `[endgame]`:

- placements counted by card: two copies of one card and one of another give 2 ways to pick one card;
  five different cards give 10 ways to pick two; no way to pick 2 and 1 from two copies;
- on the real decks, every world count at least 1, trees measured with values in [−1, 1] and leaves
  covering the playable plans, and the same seed giving the same counts and values.
