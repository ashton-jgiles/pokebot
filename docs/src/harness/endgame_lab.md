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
Prize cards are one world however they're numbered. [`count_placements(copies, first, second)`](../agents/endgame/chance.md) gives
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

At the same decisions it also runs the [endgame solver](../agents/endgame/endgame_solver.md) from
the deciding player's observation, over `solver_worlds` (4) worlds from the deck model to
`solver_depth` (2), with the lab's plan settings and no node budget, and records the depth reached,
nodes, chance nodes, alpha-beta and Star1 cuts, and-trees, plans dropped, value and seconds: what a
full solve costs, for choosing the solver's node budget.

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
| `model` | none | The deck model, for counting lists and dealing the solver's worlds |
| `solver_worlds`, `solver_depth` | 4, 2 | The endgame solver's worlds and depth |

`summarize(result, p)` gives, for decisions with `p` Prize cards left as the smaller count, the
medians (log₁₀ worlds, hidden hand, deck cards, lists), the share of decisions with at most 10², 10⁴,
10⁶ and 10⁸ worlds, and the trees' totals with their median and 90th-percentile seconds.

## In the sim

`--endgame-lab N [--every K] [--endgame-prizes P] [--endgame-worlds M] [--endgame-depth D]` runs it with the sim's `--planner-and-budget`,
`--planner-samples`, `--mcts-plans`, `--mcts-opponent-budget` and `--mcts-opponent-plans`, so the
trees use the moves IS-MCTS uses, and prints a world table, a tree table and a solver table by Prize
cards left.

## Results

A first scratch measurement (400 games, M9's planning) is in the [roadmap](../../roadmap.md).

**Step 9.1 (2026-11-23):** `--endgame-lab 160`, release build: 160 Greedy games (10 a pairing),
15,754 main-phase decisions (98 a game), a tree at one in every 5 qualifying decisions, 95 s. 68 games
got down to 1 Prize card for someone and 33 more to 2: 63% of games reach P_end = 2.

| Prize cards left (the smaller count) | Decisions a game | Median worlds (own, opponent, total) | At most 10⁴ / 10⁶ / 10⁸ | Hidden hand (median, at most 2) | Distinct deck cards |
|---|---|---|---|---|---|
| 1 | 3.0 | 10³·⁰, 10⁸·³, 10¹⁰·⁹ | 0.6% / 2.8% / 8.3% | 10, 0.8% | 15 / 16 |
| 2 | 7.4 | 10²·⁶, 10⁷·⁹, 10¹¹·¹ | 0.3% / 1.9% / 10.8% | 8, 3.3% | 15 / 16 |
| 3 | 8.0 | 10³·³, 10⁸·⁵, 10¹²·⁰ | 0.0% / 0.5% / 4.5% | 7, 3.5% | 16 / 17 |
| 4 | 11.2 | 10³·⁹, 10⁹·⁸, 10¹³·⁸ | 0.1% / 0.3% / 2.6% | 9, 2.8% | 15 / 16 |
| 5 | 8.1 | 10⁴·⁶, 10⁹·⁷, 10¹⁴·² | 0.0% / 0.2% / 0.7% | 7, 6.5% | 17 / 18 |
| 6 | 60.8 | 10⁵·², 10¹⁰·⁰, 10¹⁵·⁰ | 0.0% / 0.1% / 0.4% | 5, 9.8% | 20 / 20 |

The deck model kept 2 lists possible (3 before any Prize card was taken), which doubles or triples
the counts above.

| Prize cards left | Trees | Plans | Ending mid-turn (1 / 2–3 / 4+ cards drawn) | Using the random generator | Draw branches | Replies | Leaves | Seconds: mean, median, p90 |
|---|---|---|---|---|---|---|---|---|
| 1 | 95 | 15.7 | 6.5 (5.0 / 1.4 / 0.0) | 1.2 | 108 | 433 | 442 | 0.271, 0.154, 0.635 |
| 2 | 237 | 15.2 | 6.6 (4.8 / 1.7 / 0.1) | 1.4 | 107 | 425 | 433 | 0.271, 0.145, 0.723 |

- **Exact enumeration at the root almost never applies:** at most 10⁴ worlds in 0.3% to 0.6% of
  endgame decisions. The solver samples the opponent's hand.
- **The opponent's turn is the cost:** about 9 plans end the turn, each with about 12 draw branches
  and 4 replies, so one unpruned world takes 0.15 s at the median, more than an IS-MCTS search.
- **Draws in the searcher's own turn are common:** 6.5 of 16 plans stop mid-turn, nearly all after
  drawing 1 to 3 cards, so they are a chance layer too.
- **Coin flips and shuffles are rare:** about 1.3 plans a tree.

**Step 9.4 (2026-11-23):** the same `--endgame-lab 160`, now also running the
[endgame solver](../agents/endgame/endgame_solver.md) at each tree (4 worlds from the deck model,
alpha-beta and Star1, depth 1 then 2, no node budget): 1,150 s, nearly all of it in the solver.

| Prize cards left | Solves | Depth 0 / 1 / 2 | Nodes | Chance nodes | Alpha-beta cuts | Star1 cuts | And-trees | Plans dropped | Seconds: mean, median, p90 |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 95 | 0 / 0 / 95 | 38,967 | 3,391 | 84 | 8,497 | 1,350 | 0 | 4.20, 1.18, 5.65 |
| 2 | 237 | 3 / 0 / 234 | 38,742 | 3,005 | 89 | 4,581 | 1,193 | 0 | 2.74, 1.26, 6.33 |

- **A full depth-2 solve costs about 1.2 s at the median and 3 s on average**, against IS-MCTS's
  0.1 s a search, with a long tail (p90 about 6 s). Nodes cost about 0.1 ms each.
- **And-trees are probably most of it, but that isn't profiled:** about 1,200 to 1,350 a solve, which at the
  [budget lab](budget_lab.md)'s 2 ms for 25 nodes would be about 2.5 s. The chance outcomes (replaying an action,
  observing and hashing each result for both players) aren't timed.
- **Star1 does most of the pruning** (4,600 to 8,500 outcomes cut a solve); alpha-beta cuts few
  moves, as each decision has only 4.
- **No root plan was dropped:** the searcher's plans played in every world. The 3 depth-0 solves had a
  single plan.

## Tests

[`tests/test_endgame_lab.cpp`](../../../tests/test_endgame_lab.cpp), tag `[endgame]`:

- placements counted by card: two copies of one card and one of another give 2 ways to pick one card;
  five different cards give 10 ways to pick two; no way to pick 2 and 1 from two copies;
- on the real decks, every world count at least 1, trees measured with values in [−1, 1] and leaves
  covering the playable plans, solver values in [−1, 1] with some solve reaching depth 2, and the same
  seed giving the same counts and values.
