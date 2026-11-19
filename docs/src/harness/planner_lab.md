# `src/harness/planner_lab` — measuring the turn planner

**Files:** [`src/harness/planner_lab.hpp`](../../../src/harness/planner_lab.hpp), [`src/harness/planner_lab.cpp`](../../../src/harness/planner_lab.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

The proposal's evaluation of Model 1 asks for two things: the or-tree mode finds the win in every
test position, and the number of nodes expanded with and without the closed set, and with and without
f_bound. The lab measures both on positions from real games, as well as the hand-built positions in
the tests. `pokebot-sim --planner-lab N` ([sim](../../tools/sim/main.md)) runs it.

```cpp
PlannerLabOptions options; // 16 games, one in 5 main-phase decisions, or-tree budget 200, limit 20,000
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

A win the or-tree finds is checked by playing the plan. Positions where any search in 2 or 3 ran out of
its `limit` are counted but left out of the rest; the others are "finished".

## What it reports

| Field | Counts |
|---|---|
| `positions`, `finished` | Positions searched, and those every search finished |
| `wins` | Finished positions with a win this turn (the unlimited or-tree found one: it's complete, so there's a win exactly when it finds one) |
| `wins_found` | Of those, the ones the Planner's budgeted or-tree found too |
| `false_wins` | Wins the budgeted or-tree reported that didn't win when played (should be 0) |
| `or_closed`, `or_open` | Or-tree nodes with the closed set and without, summed |
| `and_both`, `and_closed`, `and_bound`, `and_neither` | And-tree nodes with the closed set and f_bound, the closed set only, f_bound only, and neither, summed |
| `pruned` | Children f_bound pruned, with the closed set on |
| `bound_changed` | Positions where f_bound changed the best plan's value: as the upper bound isn't proven, this is its measured soundness |

The searches run on the real game state. That's fine for measuring: a plan's Det actions come out the
same whatever the hidden cards are, and uncertain plan endings are scored in sampled worlds dealt from
what the player sees.

## Tests

[`tests/test_planner_lab.cpp`](../../../tests/test_planner_lab.cpp), tag `[planner]`:

- on sample cards, the counts add up: no false wins, no more found than exist, and the closed set and
  f_bound never expand more nodes than without them, and f_bound never changes the best value;
- on the real decks, the same, and the Planner's 200-node or-tree finds every win there is.
