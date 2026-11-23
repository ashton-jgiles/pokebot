# `src/harness/budget_lab` — what a cheap and-tree costs and gives up

**Files:** [`src/harness/budget_lab.hpp`](../../../src/harness/budget_lab.hpp), [`src/harness/budget_lab.cpp`](../../../src/harness/budget_lab.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

IS-MCTS needs the opponent's macro-actions at every opponent node it visits, in a different world
each time, so it can't afford the full 300-node [and-tree](../agents/planner/and_tree.md) there
(about 23 ms mid-game). This lab measures how much smaller budgets save and lose, so the opponent's
budget in [macro-actions](../agents/is_mcts/macro_actions.md) is chosen from data.

```bash
./build/release/tools/sim/pokebot-sim.exe --budget-lab 64 --every 5
```

## What it does

It plays `games` Greedy-against-Greedy games, as the [planner lab](planner_lab.md) does, and at
one in every `every` main-phase decisions runs the and-tree from the deciding player's side at the
`reference` budget (the sim's `--planner-and-budget`, 300) and at each of `budgets` (25, 50, 100,
150), all keeping the top `plans` (4) with the same scorer seed.

Plan values depend only on the state and the seed, so they're comparable across budgets, and a
smaller budget searches a prefix of the same depth-first search: its best plan can never beat the
reference's. For each budget it reports:

| Column | Meaning |
|---|---|
| time/search | Milliseconds per search |
| same best | How often its best plan's value equals the reference's |
| value lost | The reference's best value minus its own, on average (e is in [−1, 1]) |
| plans | Plans found per search |

A cheap search that found no plan where the reference found one counts as losing its best value
plus 1; in practice every search finds at least ending the turn.

## Results (step 8.3, 2026-11-22)

`--budget-lab 64 --every 5`, release build: 64 Greedy games, 1,234 positions, 45 s.

| Budget | Time/search | Same best | Value lost | Plans |
|---|---|---|---|---|
| 300 (reference) | 15.02 ms | 100% | 0 | 3.84 |
| 25 | 1.88 ms | 79.1% | 0.031 | 3.84 |
| 50 | 3.35 ms | 84.8% | 0.021 | 3.84 |
| 100 | 5.90 ms | 90.7% | 0.010 | 3.84 |
| 150 | 8.47 ms | 94.2% | 0.006 | 3.84 |

Even 25 nodes finds as many plans as 300 and gives up only 0.031 of e on average (e is in [−1, 1]),
at an eighth of the time. Most of a full search goes on confirming plans it has already found. The
opponent's budget is set to 25; 50 and 100 are measured against it in step 8.6.

## Tests

In [`tests/test_macro_actions.cpp`](../../../tests/test_macro_actions.cpp): on the real decks, one
game with a 150-node reference and budgets 25 and 150: the 25-node search never beats the reference,
and the 150-node one matches it every time.
