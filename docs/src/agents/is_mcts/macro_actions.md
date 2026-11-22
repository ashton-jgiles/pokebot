# `src/agents/is_mcts/macro_actions` — the moves at a node

**Files:** [`src/agents/is_mcts/macro_actions.hpp`](../../../../src/agents/is_mcts/macro_actions.hpp), [`src/agents/is_mcts/macro_actions.cpp`](../../../../src/agents/is_mcts/macro_actions.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

IS-MCTS (M8) moves by macro-actions: whole turns, as the proposal's Section 8 has them, found by
Model 1's and-tree from the acting player's own side. `MacroActions::moves(world)` gives them for
whoever decides in a determinization ŵ.

```cpp
MacroActions macros(searcher, seed);   // one for each search
const std::vector<Plan>& moves = macros.moves(world);
```

## Which moves

| Who decides, and what | Moves | Cached |
|---|---|---|
| The searcher, in its main phase or at an extra attack | The [and-tree](../planner/and_tree.md)'s top `searcher_plans` (16) plans with budget `searcher_budget` (300), as in Flat MC | Yes, by the [information set](info_set.md) |
| The opponent, in its main phase or at an extra attack | A cheap and-tree: top `opponent_plans` (4) with budget `opponent_budget` (25, chosen from the [budget lab](../../harness/budget_lab.md#results-step-83-2026-11-22)) | No |
| Anyone, any other decision (a new Active, a search, a discard) | Every legal action as a one-action plan | No |
| The game is over | None | |

If an and-tree finds no plan, the node falls back to single actions, so a node is never without
moves while the game goes on.

**Why the searcher's plans are cached.** The searcher sees the same thing in every world, so a node's
plans are the same in all of them; they're worked out on the first visit and reused after, keyed by
the information-set key of the searcher's observation. At about 23 ms an and-tree search, this is
what makes IS-MCTS affordable.

**Why the opponent's aren't.** The opponent's options depend on its hand in ŵ, which changes from
world to world, so a cache would rarely hit. Its search is kept cheap instead: the budget lab found
that 25 nodes take 1.9 ms against 15 ms for 300, find as many plans, and give up only 0.031 of e
on average (its top plan is the full search's 79% of the time).

**Nothing leaks.** Plans are found in ŵ from the actor's own side: the and-tree's plan values read
only what both players can see, and any worlds its scorer deals come from the actor's own
observation. So the opponent's plans use its sampled hand (which it knows in ŵ), and neither side's
plans are chosen with cards that side couldn't see.

The reference `moves` returns is good until the next call; cached lists last as long as the
`MacroActions`, which is one search.

## Counters

`stats()`: the searcher's and-tree searches and cache hits, the opponent's searches, nodes given
single actions, and the seconds in each kind of search.

## Tests

[`tests/test_macro_actions.cpp`](../../../../tests/test_macro_actions.cpp), tag `[is_mcts]`:

- a finished game has no moves (sample cards);
- on the real decks, at decisions through random games for both players: every move a legal plan, at
  most K of them when planning (unless it fell back to single actions), and every legal action
  alone otherwise;
- on the real decks, the searcher's plans worked out once per information set and the same from the
  cache, while the opponent's are searched each time;
- the budget lab, below.
