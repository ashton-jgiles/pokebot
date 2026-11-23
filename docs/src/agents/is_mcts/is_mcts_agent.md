# `src/agents/is_mcts/is_mcts_agent` — IS-MCTS playing a game

**Files:** [`src/agents/is_mcts/is_mcts_agent.hpp`](../../../../src/agents/is_mcts/is_mcts_agent.hpp), [`src/agents/is_mcts/is_mcts_agent.cpp`](../../../../src/agents/is_mcts/is_mcts_agent.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Milestone 8's agent: the proposal's Section 6 decision procedure with Model 2 as its main search (the
endgame solver, Model 3, comes in M9).

```cpp
const ArchetypeLibrary library = ArchetypeLibrary::load("data/archetypes", db);
const DeckModel model(library);
IsMctsAgent agent(seed, model);   // 48 iterations, r = 2, matched worlds
agent.analysis();                 // the last search's root: plans, visits, means
```

`pokebot-sim --agent ismcts` or `--agents ismcts,flatmc` plays it ([sim](../../../tools/sim/main.md)).

## How it decides

1. **Setup** (turn 0): Greedy's setup rule, since a `World` can't be built before both players have
   set up.
2. **One legal action:** that one.
3. **Its main phase, or an extra attack:** a world dealt from the [deck model](../flat_mc/deck_model.md),
   and the [or-tree](../planner/or_tree.md) looks for a win; if it finds one, that plan is played.
4. **Otherwise, any decision after setup:** [IS-MCTS](is_mcts.md) searches from the observation and its
   most visited move is played. Outside the main phase the root's moves are single actions, as the
   proposal has it ("small forced decisions go through Model 2 as single-action macro-actions").
5. **Following:** a plan is followed while the observation is the one it expected, and the agent
   decides again when not, as the [Planner](../planner/planner_agent.md) and
   [Flat MC](../flat_mc/flat_mc_agent.md) do.

If a search somehow returns nothing, Greedy decides.

A plan's first action is legal in the real game: every world observes back to the same observation,
so the same actions are offered in each. Its later actions usually are too, but a Det action can
depend on how the agent's hidden cards are split between deck and Prize cards (see
[plans](../planner/plan.md)), so the agent walks the plan in its own world only while each action is
legal there, and in the real game plays each one only while the observation is the one it expected
and the action is offered.

## Options

| `IsMctsAgentOptions` | Default | Meaning |
|---|---|---|
| `or_budget` | 200 | Nodes for the win search |
| `search` | [`IsMctsOptions`](is_mcts.md#the-search) with 48 iterations, r = 2 and matched worlds | The search: iterations, C, r, the and-tree budgets, matched worlds, choosing by mean |
| `weights` | `default_weights()` | θ for e (Greedy's setup rule) |

Step 8.5 started at 64 iterations, to match Flat MC's 64 playouts. Step 8.6 tuned against Flat MC instead
(see the roadmap's 8.6 table): matched worlds and r = 2 won. By search time a game, 42 iterations at r = 2
match Flat MC's time (1.02 times it) and 48 take about 1.16 times it. The search's own `IsMctsOptions` defaults stay as they were
(200 iterations, r = 1, fresh worlds).

## Counters

The agent's (plans followed, wins found, plan actions, Greedy decisions, or-tree nodes and seconds) and
its search's: searches, iterations, tree nodes, playout actions, moves unplayable in a world, outcome nodes, matched iterations skipped, the searcher's and-trees and cache
hits, the opponent's and-trees, single-action nodes, and the seconds in the search, its playouts and
each side's and-trees. `search s` over `iterations` is what an iteration costs.

## Tests

[`tests/test_is_mcts_agent.cpp`](../../../../tests/test_is_mcts_agent.cpp), tag `[is_mcts]`, on the
Planner's sample cards with a one-archetype library and small budgets:

- a win two actions away taken without searching;
- with no win, one search and its most visited plan played to the end, the analysis ranked by visits;
- whole sample-card games, the same for the same seed;
- two whole real-deck games against random, with invariant checks (skipped without generated card
  data);
- its counters and timings.
