# `src/agents/endgame/expectiminimax` — the endgame solver's search in one world

**Files:** [`src/agents/endgame/expectiminimax.hpp`](../../../../src/agents/endgame/expectiminimax.hpp), [`src/agents/endgame/expectiminimax.cpp`](../../../../src/agents/endgame/expectiminimax.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Model 3 (proposal Section 9): an expectiminimax search over turn segments in one world ŵ, pruned by
alpha-beta at the players' decisions and Star1 at chance nodes. The
[endgame solver](endgame_solver.md) runs it over the root's worlds with iterative deepening under a
budget.

```cpp
const SolveResult r = expectiminimax(world, options);   // depth 2 by default
r.plans[r.best].plan;   // the best plan for the player to move
r.value;                // its value, in [-1, 1]
```

## The tree

- **Decision nodes, main phase:** the moves are the [and-tree](../planner/and_tree.md)'s top plans
  for whoever is to move: `root_plans` (16) at `root_budget` (300) at the root, `plans` (4) at
  `budget` (25) deeper, for either player. The and-tree ranks them best first for the actor, which is
  the order pruning wants at both kinds of node. A plan uses one level of depth.
- **Decision nodes, other decisions** (a new Active, an effect's choice): every legal action, without
  using depth. The searcher maximizes and the opponent minimizes, so a forced choice is searched
  rather than handed to Greedy.
- **Applying a plan:** its deterministic actions directly; its last action through
  [`chance_outcomes`](chance.md). One outcome is no chance node; several are an AND-chance node, its
  outcomes searched most likely first.
- **Leaves:** u when the game is over; e at depth 0, or where no move is found.

## Pruning

Values lie in [−1, 1]. With `prune` (the default):

- **Alpha-beta** at the decision nodes, fail-soft: a maximizing node stops once its value reaches β,
  a minimizing one once it falls to α, and the moves it skips are counted.
- **Star1** at chance nodes: each outcome is searched with the window that could still change the
  result, ((α − S − 1·R) / p, (β − S + 1·R) / p) for an outcome of probability p, S the weighted sum so
  far and R the probability left after it; the node stops once S plus the best (+1) or worst (−1) the
  rest could add can't come back into (α, β), and the outcomes it skips are counted.
- **The root** searches each plan with α at the best value so far: a plan that beats it has an exact
  value, the others an upper bound (`exact = false`). Without pruning every value is exact.

Move lists (the plan scorer's seed) and chance sampling are seeded from each node's
[`hash_state`](../planner/state_hash.md), not from a generator shared along the search, so the search
with and without pruning sees exactly the same tree. The tests use that to check pruning against the
search without it.

| `ExpectiminimaxOptions` | Default | Meaning |
|---|---|---|
| `depth` | 2 | Plans along a path before e |
| `root_budget`, `root_plans` | 300, 16 | The and-tree at the root |
| `budget`, `plans` | 25, 4 | The and-tree deeper, for either player |
| `samples` | 1 | The plan scorer's sampled worlds |
| `prune` | true | Alpha-beta and Star1; false is the brute-force reference |
| `max_nodes` | 0 | Stop once this many nodes (decisions, chance nodes and leaves) are visited; 0 = no limit |
| `chance` | `ChanceOptions` | How chance outcomes are enumerated or sampled |
| `weights` | `default_weights()` | θ for e |

**Over several worlds:** `solve_worlds(worlds, plans, options)` takes the root's plans (from
`root_moves(game, options)`) and scores each by its average over the worlds, equally weighted: a
chance node over the worlds under the OR node over the plans, with Star1 against the best plan so
far. `expectiminimax(game)` is `solve_worlds({&game}, root_moves(game))`. Each plan must be playable in
every world. With `max_nodes`, a search that reaches it stops, returns the plans it finished and is
marked `complete = false`. `visited(stats)` counts decisions, chance nodes and leaves.

`ExpectiminimaxStats`: decision nodes, chance nodes, leaves, and-tree searches, moves cut by
alpha-beta, outcomes cut by Star1, the chance outcomes' own counters and seconds.

**Known limits (for 9.4 and the lab to measure):** choices outside the main phase are searched in
full, which can be wide (picking from a deck search branches on every card); there is no
transposition table.

## Tests

[`tests/test_expectiminimax.cpp`](../../../../tests/test_expectiminimax.cpp), tag `[endgame]`:

- a Knock Out of the opponent's last Pokémon is worth 1, the best plan ending with the attack;
- without pruning, every root value is exact and the best is the largest;
- at depths 1 and 2, with a coin attack and a draw, pruning finds the same value as the search
  without it, its exact values match and its bounds are bounds, and it visits no more nodes;
- the same position solves to the same plans, values and node counts;
- on the real decks, in endgames from Greedy games, pruned and unpruned depth-2 searches agree,
  with some nodes cut.
