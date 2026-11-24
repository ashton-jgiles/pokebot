# `src/agents/endgame/endgame_agent` — IS-MCTS with the endgame solver

**Files:** [`src/agents/endgame/endgame_agent.hpp`](../../../../src/agents/endgame/endgame_agent.hpp), [`src/agents/endgame/endgame_agent.cpp`](../../../../src/agents/endgame/endgame_agent.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

The proposal's decision procedure (Section 7): the [endgame solver](endgame_solver.md) when the game is
close to its end, and the [IS-MCTS agent](../is_mcts/is_mcts_agent.md) otherwise. It decides from its
player's observation only, like every agent.

```cpp
EndgameAgent agent(seed, model);   // the solver's depth-2 plans at 2 or fewer Prize cards left, else IS-MCTS
```

`pokebot-sim --agent endgame` or `--agents endgame,ismcts` plays it ([sim](../../../tools/sim/main.md)).

## A decision

1. **Partway through a solver plan,** while the observation is the one it expected and the next
   action is legal, it plays the plan's next action.
2. **In the endgame,** it solves: after setup, at a main-phase or extra-attack decision with more
   than one action, when either player has at most `prizes` (2) Prize cards left, and unless IS-MCTS
   is partway through a plan of its own. The best plan is followed as IS-MCTS follows its own: walked
   in one world dealt from the observation, cut at the first action that isn't legal there, with
   the observation expected before each action. If the solver has no plan (its budget didn't finish
   depth 1) or the plan's first action isn't legal, that's a fallback.
3. **Otherwise** IS-MCTS decides: its or-tree win, its search, its plan.

Outside the main phase (a new Active, an effect's choice) IS-MCTS decides even in the endgame: the
solver's root would have to deal worlds while the player is shown cards mid-choice, as in a search
of the deck.

The proposal checks the endgame before the or-tree's win; so does this agent, since the solver finds
a one-turn win itself (a Knock Out of the last Pokémon is worth 1). Outside the endgame, IS-MCTS's
or-tree still looks for a win first.

| `EndgameAgentOptions` | Default | Meaning |
|---|---|---|
| `prizes` | 2 | The endgame: either player at most this many Prize cards left (P_end) |
| `min_depth` | 2 | The solver's plan is played only if its solve reached this depth (or had one plan); otherwise IS-MCTS decides, counted as a fallback. 2 since 9.6: a depth-1 solve scores a plan by e with no reply from the opponent, and playing those lost to IS-MCTS (45.9%, against 50.1% without them) |
| `is_mcts` | `IsMctsAgentOptions` | The IS-MCTS agent outside the endgame and on fallbacks |
| `solver` | `EndgameOptions` | The solver: 4 worlds, depth 2, 10,000 nodes |

## Counters

Its own first (endgame decisions, solver plans played, fallbacks, solver plan actions, solves
reaching depth 1 and 2, and the solver's nodes, chance nodes, and-trees, alpha-beta and Star1 cuts,
plans dropped and seconds), then IS-MCTS's.

## Tests

[`tests/test_endgame_agent.cpp`](../../../../tests/test_endgame_agent.cpp), tag `[endgame]`, on the
IS-MCTS agent tests' sample cards with a one-archetype library and small budgets:

- in the endgame, the solver's depth-2 best plan played to the end, IS-MCTS never searching;
- outside the endgame, IS-MCTS's search and plan;
- in the endgame, a win taken (a free retreat, then a Knock Out of the last Pokémon);
- on a 1-node budget, a fallback on IS-MCTS, and the same when the solve stops short of `min_depth`;
- whole sample-card games, the same for the same seed; whole real-deck games against random, with
  endgame decisions solved;
- its counters first, then IS-MCTS's.
