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
   actor's moves in ŵ. Each is applied to a copy, and the result is keyed by the
   [information-set key](info_set.md) of the **searcher's** observation; moves with the same key count
   once. The [tree](tree.md) marks the existing children legal in ŵ available. If a legal move has no
   child yet, the first such (in the and-tree's order, best first) is added and the descent stops:
   expansion. Otherwise UCB1 picks among the legal children and the descent goes on from that child's
   state in ŵ.
3. **Evaluate.** [`play_out`](../flat_mc/playout.md) from the reached state, r turns of Greedy,
   scored with e for the searcher, or u if the game is over (`play_out` handles that itself).
4. **Backpropagate** along the path, the value turned into player 0's.

**No chance nodes.** A draw, a flip or a shuffle happens inside ŵ when a move is applied, and what
the searcher sees of it is already in the key, so "the child matching the observed outcome" is the
child with that key. Different outcomes are different children of the same parent.

**Searcher and opponent nodes alike are information sets of the searcher.** An opponent move is keyed
by what the searcher sees of it, so the opponent playing the same card under different numbers, or
choosing hidden cards, leads to one node: single-observer IS-MCTS.

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

## Counters

`stats()`: searches, iterations, tree nodes, playout actions, seconds in all and in playouts, and the
macro-action counters added up over the searches.

## Tests

[`tests/test_is_mcts.cpp`](../../../../tests/test_is_mcts.cpp), tag `[is_mcts]`, with small budgets:

- on the Planner's sample cards, the most visited plan taking a win two actions away;
- every iteration passing through exactly one root move (visits add up to the iterations), at most one
  node added an iteration, the root's plans legal and served from the cache, the means in [−1, 1],
  and the same seed giving exactly the same search;
- on the real decks, positions from both seats searched, every root plan legal;
- on the real decks, games that differ only in cards the searcher can't see getting exactly the same
  search: the same plans, visits and means.
