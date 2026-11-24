# `src/agents/endgame/endgame_solver` — the endgame solver over sampled worlds

**Files:** [`src/agents/endgame/endgame_solver.hpp`](../../../../src/agents/endgame/endgame_solver.hpp), [`src/agents/endgame/endgame_solver.cpp`](../../../../src/agents/endgame/endgame_solver.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Model 3's root (proposal Section 9): from the searcher's observation, the best plan by its average
value over sampled worlds, each world searched by [expectiminimax](expectiminimax.md), deepening one
turn segment at a time under a node budget.

```cpp
EndgameSolver solver(&model, seed, options);        // 4 worlds, depth 2, 10,000 nodes by default
const EndgameResult r = solver.solve(observation);
if (r.depth > 0 || r.plans.size() == 1) {
    r.plans[r.best].plan;                           // the plan to play
}
```

## The root

1. **Worlds:** `worlds` (4) worlds are dealt from the observation with the
   [deck model](../flat_mc/deck_model.md), each from a fresh seed, with equal weight.
2. **Plans:** the [and-tree](../planner/and_tree.md)'s top plans in the first world (`root_plans` at
   `root_budget`), kept only if each one plays in every world (`apply_plan_checked`; a deterministic
   action can depend on how the searcher's hidden cards split between deck and Prize cards). The
   others are dropped and counted.
3. **Values:** each plan's value is its average over the worlds: the root is a chance node over the
   worlds, under the OR node over the plans, as the proposal's output arg max over τ of Σ Pr(ŵ) V(δ(ŵ, τ))
   says. `solve_worlds` searches it with Star1 against the best plan so far: once the worlds searched
   so far, plus +1 for those left, can't beat it, the plan stops with that upper bound.

**The proposal enumerated the worlds when there are at most N_end; this samples them always.** The
[endgame lab](../../harness/endgame_lab.md) found at most 10⁴ worlds in under 1% of endgame decisions,
and enumerating them would need a way to build a world for each placement.

## Iterative deepening

Depth 1, then 2, up to `max_depth`, sharing `max_nodes` (0 = no budget). Each depth searches the
root plans in the order the last one ranked them, so alpha-beta and Star1 cut more. The answer is the
deepest depth that finished: a depth that runs out of budget is thrown away.

- One playable plan: it is returned without searching (depth 0).
- No depth finished: no plans and depth 0, so the agent falls back on something else (IS-MCTS in 9.5).

| `EndgameOptions` | Default | Meaning |
|---|---|---|
| `worlds` | 4 | Worlds sampled at the root (M) |
| `max_depth` | 2 | The deepest depth tried |
| `max_nodes` | 10,000 | Nodes (decisions, chance nodes and leaves) over all depths; 0 = no budget. At about 0.1 ms a node, about 1 s: the [endgame lab](../../harness/endgame_lab.md)'s full depth-2 solves took 39,000 nodes on average and 1.2 s at the median, so about half reach depth 2 and the rest stop at depth 1. Tuned in 9.6 |
| `search` | `ExpectiminimaxOptions` | The search in each world (its `depth` and `max_nodes` are set by the solver) |

`EndgameResult`: the plans with their values (exact for the best, bounds for the rest, as in
[expectiminimax](expectiminimax.md)), the best, its value, the depth reached and the nodes visited.
`EndgameStats`: solves, iterations, how often each depth was reached, plans dropped, and the search's
counters added up.

## Tests

[`tests/test_endgame_solver.cpp`](../../../../tests/test_endgame_solver.cpp), tag `[endgame]`:

- one world through `solve_worlds` is the same search as `expectiminimax`; two copies of a world give
  the world's values;
- over three sampled worlds, pruning at the root and below agrees with the search without it and
  visits no more nodes;
- a node budget stops the search and marks it incomplete; the solver reaches depth 2 without a
  budget and stops sooner with a small one;
- the solver takes a Knock Out of the opponent's last Pokémon, and the same seed solves the same;
- on the real decks, solving endgames from the searcher's observation reaches depth 2 with values
  in [−1, 1].
