# `src/harness/planner_lab` — measuring the turn planner

**Files:** [`src/harness/planner_lab.hpp`](../../../src/harness/planner_lab.hpp), [`src/harness/planner_lab.cpp`](../../../src/harness/planner_lab.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

The proposal's evaluation of Model 1 asks for two things: the or-tree mode finds the win in every
test position, and the number of nodes expanded with and without the closed set, and with and without
f_bound. The lab measures both on positions from real games, as well as the hand-built positions in
the tests. `pokebot-sim --planner-lab N` ([sim](../../tools/sim/main.md)) runs it.

```cpp
PlannerLabOptions options; // 16 games, one in 5 main-phase decisions, or-tree budget 200, limit 5,000, 1 sampled world
const PlannerLabResult result = run_planner_lab(db, decks, options);
```

## What it does

It plays `games` games of [Greedy](../agents/greedy_agent.md) against Greedy, game i on deck pairing
(i mod n, (i / n) mod n). At one in every `every` main-phase decisions, it searches the position from
the deciding player's side:

1. the [or-tree](../agents/or_tree.md) for a win this turn, with the Planner's budget (`or_budget`);
2. the same, with no budget to speak of (`limit` nodes), with the closed set and without;
3. the [and-tree](../agents/and_tree.md) for the best plan, with `limit` nodes, in all four
   combinations of the closed set and f_bound.

A win the or-tree finds is checked by playing the plan. Positions where any search in 2 or 3 runs out of
its `limit` are counted but left out of the rest; the others are "finished". The searches run cheapest
first (the closed-set or-tree, then the and-tree with both savings, then the ones without), and the
lab moves on as soon as one runs out, so a position too big to finish costs as little as possible.
Uncertain plan endings are scored in `samples` worlds, 1 by default, like the Planner.
`on_progress(games done, games)` is called after each game.

## What it reports

| Field | Counts |
|---|---|
| `positions`, `finished` | Positions searched, and those every search finished |
| `wins` | Finished positions with a win this turn (the unlimited or-tree found one: it's complete, so there's a win exactly when it finds one) |
| `wins_found` | Of those, the ones the Planner's budgeted or-tree found too |
| `win_nodes`, `win_nodes_max` | Nodes the unlimited or-tree expanded to reach each win, summed and at most: the budget the Planner would need to find them all |
| `false_wins` | Wins the budgeted or-tree reported that didn't win when played (should be 0) |
| `or_closed`, `or_open` | Or-tree nodes with the closed set and without, summed |
| `and_both`, `and_closed`, `and_bound`, `and_neither` | And-tree nodes with the closed set and f_bound, the closed set only, f_bound only, and neither, summed |
| `pruned` | Children f_bound pruned, with the closed set on |
| `bound_changed` | Positions where f_bound changed the best plan's value: as the upper bound isn't proven, this is its measured soundness |

The searches run on the real game state. That's fine for measuring: a plan's Det actions come out the
same whatever the hidden cards are, and uncertain plan endings are scored in sampled worlds dealt from
what the player sees.

## Results

Release build, `pokebot-sim --planner-lab N --every 5`, seed 1, limit 5,000:

| Games | Positions | Finished | Wins | Found at 200 / 500 / 1,000 nodes | Nodes to a win (average, most) | False wins |
|---:|---:|---:|---:|---|---|---:|
| 16 | 334 | 254 | 8 | 5 / 6 / 7 | | 0 |
| 64 | 1,234 | 928 | 12 | 9 at 200 | 214.5, 1,420 | 0 |

Nodes per finished position, 64 games:

| Search | Closed set and f_bound | Closed set only | f_bound only | Neither |
|---|---:|---:|---:|---:|
| Or-tree (win) | | 165.1 | | 386.0 |
| And-tree (best plan) | 163.4 (0.3 children pruned) | 168.9 | 381.0 | 396.4 |

- **The or-tree's wins are real:** there were no false wins. But the Planner's 200-node budget misses
  a quarter of them. Positions with a win this turn are bigger than most, and the deepest win took
  1,420 nodes.
- **The closed set is the big saving:** it cuts both searches by about 2.3 times.
- **f_bound is safe but saves little:** it changed the best value in none of 928 positions, but it cuts
  the and-tree by only 3%. Its bound is 1 whenever a Knock Out could win, and otherwise loose, so it
  rarely falls below the best plan found so far.
- **A quarter of the positions are big:** 306 of 1,234 didn't finish within 5,000 nodes. In a real
  game, those are the turns where the Planner uses its whole budget. The lab took 113 s for 64 games.

## Tests

[`tests/test_planner_lab.cpp`](../../../tests/test_planner_lab.cpp), tag `[planner]`:

- on sample cards, the counts add up: no false wins, no more found than exist, and the closed set and
  f_bound never expand more nodes than without them, and f_bound never changes the best value;
- on the real decks, the same, and the Planner's 200-node or-tree finds every win there is.
