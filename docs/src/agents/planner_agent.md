# `src/agents/planner_agent` — the turn planner as an agent

**Files:** [`src/agents/planner_agent.hpp`](../../../src/agents/planner_agent.hpp), [`src/agents/planner_agent.cpp`](../../../src/agents/planner_agent.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

The third rung of the agent ladder (M6): Random → Greedy → **Planner** → flat Monte Carlo (M7). It
plays Model 1: it plans the certain part of its turn and plays the plan.

```cpp
PlannerAgent planner(seed);          // budgets 200 (or-tree) and 300 (and-tree), 3 samples
PlannerAgent quick(seed, options);   // other budgets, samples or weights
planner.stats();                     // plans made, wins found, actions followed, greedy decisions
```

## How it decides

This is the proposal's decision procedure with Model 2 (IS-MCTS, M8) not built yet:

1. **In its main phase** (a `MainPhase` or `ExtraAttack` decision, after setup), it builds a
   [world](world.md) from its observation and searches it:
   1. the [or-tree](or_tree.md) with γ_win: if there's a winning line this turn, that's the plan;
   2. otherwise the [and-tree](and_tree.md)'s best plan (K = 1), standing in for IS-MCTS.

   It then plays the plan's first action.
2. **While the game goes as planned,** it plays the plan's next action. "As planned" means its
   observation is exactly what the plan expected at that point and the next action is legal. Every
   action before the last is Det, so in any world that matches what it sees, the same actions lead
   to the same observations, and the real game is one of those worlds.
3. **When it doesn't** (a different observation, as after the last action's draw or search, or the
   opponent's turn), it drops the plan, and plans again at its next main-phase decision. This is
   the proposal's "Model 1 is called again from the resulting state, still in the same turn".
4. **Every other decision** (setup, choosing a new Active, choices inside effects that aren't part
   of a plan) goes to a [greedy agent](greedy_agent.md), with its follow-up choices, as the proposal's
   small forced decisions. That includes the choices after a plan's last action, such as which Pokémon
   a search takes, which is also how the plan was scored.
5. A single legal action is taken without searching.

## Options

| Option | Default | Meaning |
|---|---|---|
| `or_budget` | 200 | B_or: nodes for the or-tree's win search |
| `and_budget` | 300 | B_tp: nodes for the and-tree |
| `samples` | 3 | m: sampled worlds for a plan that ends in chance or hidden cards |
| `weights` | `default_weights()` | θ for e, used by the planner and the greedy fallback |

The budgets are a starting point; step 6.8 measures the Planner's strength and speed against them.

## Where it's used

`pokebot-sim --agent planner` and `--agents planner,greedy` ([sim](../../tools/sim/main.md)), and
`pokebot play|watch --agent planner` ([client](../client/main.md)).

## Tests

[`tests/test_planner.cpp`](../../../tests/test_planner.cpp), tag `[planner]`:

- it wins with a free retreat then the other Pokémon's attack, and counts the win the or-tree found;
- with no win, it plays the and-tree's evolve, attach, attack to the end, from one plan;
- it plans again when its observation isn't what the plan expected;
- it plays whole sample-card games against random, the same moves for the same seed;
- on the real decks, it plays whole games against random, with the invariant checks on.
