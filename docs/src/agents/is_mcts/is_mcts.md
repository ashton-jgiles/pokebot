# `src/agents/is_mcts/is_mcts` — the IS-MCTS search

**Files:** [`src/agents/is_mcts/is_mcts.hpp`](../../../../src/agents/is_mcts/is_mcts.hpp), [`src/agents/is_mcts/is_mcts.cpp`](../../../../src/agents/is_mcts/is_mcts.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Model 2 (proposal Section 8): information-set Monte Carlo tree search over macro-actions. One tree is
shared by many determinizations, so the search plans for the hidden cards it can't see without
pretending to know them.

```cpp
IsMcts search(model, seed);   // defaults: 200 iterations, C = 0.7, r = 1
const std::vector<RootChoice> choices = search.search(observation);
choices.front().plan;         // the most visited move; every RootChoice has its visits and mean
```

## One iteration

1. **Determinize.** Deal a world ŵ from the observation with the [deck model](../flat_mc/deck_model.md),
   from a fresh seed.
2. **Select or expand, down the tree.** At each node, [`MacroActions`](macro_actions.md) gives the
   actor's moves in ŵ. Each is applied to a copy with
   [`apply_plan_checked`](../planner/plan.md). A move that can't be played in ŵ (a cached plan whose
   "deterministic" action asks for a choice in this world) is left out, as not available here, and
   counted; a node with no playable move is evaluated where it stands. The edges are keyed as below.
   The [tree](tree.md) marks the existing children legal in ŵ available. If a legal edge has no
   child yet, the first such (in the and-tree's order, best first) is added and the descent stops:
   expansion. Otherwise UCB1 picks among the legal children and the descent goes on from that child's
   state in ŵ.
3. **Evaluate.** [`play_out`](../flat_mc/playout.md) from the reached state, r turns of Greedy,
   scored with e for the searcher, or u if the game is over (`play_out` handles that itself).
4. **Backpropagate** along the path, the value turned into player 0's.

**Move nodes, then outcome nodes, at the searcher's decisions.** The searcher's move is keyed by the
move itself (each action's kind, card, target and amount), and under it an **outcome node** is keyed
by the [information-set key](info_set.md) of what the searcher sees after it. So a plan that draws,
flips or searches the deck keeps all its visits on one move node, whichever cards come; the outcomes
it led to are its children. Step 8.5 keyed the searcher's moves by the observation after them, which
split a plan ending in a draw into as many root children as there were draws, each with a visit or
two, so the plans with chance in them never built up visits and lost to the plans without. Reaching
a new outcome of a known move is an expansion too, and is counted.

**Opponent nodes are information sets of the searcher.** An opponent move is keyed by what the
searcher sees after it, so the opponent playing the same card under different numbers, or choosing
hidden cards, leads to one node: single-observer IS-MCTS. A draw, a flip or a shuffle happens inside
ŵ when a move is applied, so there are no separate chance nodes: the outcome is in the key.

## Matched worlds (`matched_worlds`)

Optional, off by default. Flat MC compares plans on the same worlds with the same playout seeds
(common random numbers); IS-MCTS deals a fresh world each iteration, so two root moves are compared
on different cards. With `matched_worlds`, the root move is picked first (an untried one, else UCB1
over all the root's children), and the iteration's world and playout seed are the k-th of one
sequence shared by every root move, k being how often that move has been visited. So the n-th visit
to every root move sees the same world and the same playout seed. Below the root, the search is as
usual. An iteration whose root move can't be played in its matched world is skipped, and counted;
the root's visits then add up to the iterations that ran.

## Choosing by mean (`final_by_mean`)

Optional, off by default. The front of the result is normally the most visited move. With
`final_by_mean`, it is the move with the best mean among those visited at least `final_min_visits`
times (3), the rest of the list keeping its order; with none visited that often, the most visited.

## The search

- **A root with one move** (or none) returns it without iterating; the root's moves are worked out
  once in one dealt world first.
- **Each search has its own tree and `MacroActions`,** so the searcher's plan cache lasts one search.
- **`max_depth`** (32) caps one iteration's descent, as a safety; trees are far shallower.
- **Seeds:** every world, every playout and the macro-actions' scorer are seeded from the `IsMcts`'s
  own generator, so the same observation and seed give the same search, and the arena's results
  don't depend on the thread count.
- **The output:** the root's children, most visited first (ties to the higher mean), each with its
  plan, visits and mean value for the searcher. The front is what the agent plays; the list is the
  analysis.

| `IsMctsOptions` | Default | Meaning |
|---|---|---|
| `iterations` | 200 | Iterations a search (to be matched to Flat MC's time in 8.5) |
| `exploration` | 0.7 | UCB1's C, for rewards in [−1, 1] |
| `max_depth` | 32 | The most nodes one iteration descends |
| `playout` | depth 1, Greedy with 1 world | r and the playout policy |
| `macros` | [`MacroOptions`](macro_actions.md) | The and-tree budgets for each side |
| `matched_worlds` | false | The n-th visit to every root move sees the same world and playout seed |
| `final_by_mean` | false | Play the best mean among the moves visited often enough, not the most visited |
| `final_min_visits` | 3 | How often a move must be visited for `final_by_mean` to pick it |

## Counters

`stats()`: searches, iterations, tree nodes, playout actions, moves unplayable in a world, outcome
nodes added, matched iterations skipped, seconds in all and in playouts, and the macro-action counters added up over the searches. In the game that
first showed the problem (seed 18, basic-box against dragapult), 42 moves in 2,496 iterations were
unplayable in the world they were tried in.

## Tests

[`tests/test_is_mcts.cpp`](../../../../tests/test_is_mcts.cpp), tag `[is_mcts]`, with small budgets:

- on the Planner's sample cards, the most visited plan taking a win two actions away;
- every iteration passing through exactly one root move (visits add up to the iterations), at most two
  nodes added an iteration (a move and its outcome), the root's plans legal and served from the cache,
  the means in [−1, 1], and the same seed giving exactly the same search;
- on the real decks, positions from both seats searched, every root plan legal, and opponent and-trees
  searched;
- on the real decks, games that differ only in cards the searcher can't see getting exactly the same
  search: the same plans, visits and means;
- on the real decks, the root's moves being distinct plans however their outcomes fall;
- with matched worlds, the visits and skipped iterations adding up to the iterations, and the same
  seed searching the same;
- with `final_by_mean`, the first move having the best mean among those visited often enough.
