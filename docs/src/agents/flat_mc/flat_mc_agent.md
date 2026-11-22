# `src/agents/flat_mc/flat_mc_agent` — flat Monte Carlo over the turn planner's plans

**Files:** [`src/agents/flat_mc/flat_mc_agent.hpp`](../../../../src/agents/flat_mc/flat_mc_agent.hpp), [`src/agents/flat_mc/flat_mc_agent.cpp`](../../../../src/agents/flat_mc/flat_mc_agent.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Milestone 7's agent, and the proposal's stepping stone to IS-MCTS: give each of the turn planner's
best plans m determinized playouts and play the one with the best average. Where the
[Planner](../planner/planner_agent.md) scores a plan by e at its end, flat MC scores it by what
happens next: the opponent's reply, played out in worlds dealt from the [deck model](deck_model.md).

```cpp
const ArchetypeLibrary library = ArchetypeLibrary::load("data/archetypes", db);
const DeckModel model(library);
FlatMcAgent agent(seed, model);   // defaults: K = 4 plans, N = 32 playouts, r = 1, Greedy with 1 world
agent.analysis();                 // the last decision's plans, best first, with their means
```

`pokebot-sim --agent flatmc` or `--agents flatmc,planner` plays it ([sim](../../../tools/sim/main.md)).

## How it decides

In its main phase (or when offered an extra attack), after setup:

1. **A world:** a [`World`](../world.md#with-a-deck-model) built from the observation and dealt from
   the deck model.
2. **A win:** the [or-tree](../planner/or_tree.md) looks for one, as in the Planner. If it finds
   one, that plan is played, with no playouts.
3. **Plans:** otherwise the [and-tree](../planner/and_tree.md) keeps its top K plans (one per end
   state). With only one, it's played without playouts.
4. **Playouts:** with two or more, flat MC deals m = N / K worlds (at least 1), each from its own
   seed, and draws a playout seed for each. In every world, each plan is applied and
   [played out](playout.md) with that world's playout seed. A plan's score is the mean of its m
   values; the best mean is played, and ties go to the and-tree's order.
5. **Following:** the plan is followed while the observation is the one it expected, and planned
   again when not, as in the Planner.

Every other decision (setup, a new Active, choices inside the opponent's effects) goes to
[Greedy](../greedy/greedy_agent.md) with 3 worlds, as in the Planner. M7's step 7.6 measures searching
the ones with several choices too.

**The same worlds for every plan.** All K plans are played out in the same m worlds with the same
playout seeds (common random numbers), so the difference between two plans' means comes from the
plans, not from luck in the draws. This is the playout version of Greedy using the same luck for
every action.

**Why a plan is legal in every world.** Each world observes back to the same observation, so the
deciding player is offered the same actions in each. A plan's Det actions change that observation
the same way in every world, and its last action is chosen from it. The same reasoning lets the
Planner follow its plan in the real game.

**Budgets are counts.** N playouts a decision, not a time, so a game plays the same for the same
seed on any machine and the arena's results don't depend on the thread count (decided 2026-11-21).
One playout is comparable to one IS-MCTS iteration (M8).

## Options

| `FlatMcOptions` | Default | Meaning |
|---|---|---|
| `or_budget` | 200 | Nodes for the win search |
| `and_budget` | 300 | Nodes for the best-plans search |
| `samples` | 1 | The and-tree's sampled worlds for a plan that ends in chance |
| `plans` | 4 | K: the and-tree's top plans played out |
| `playouts` | 32 | N: playouts each decision, split evenly over the plans |
| `playout` | depth 1, Greedy with 1 world | The [playout options](playout.md#where-it-stops): r, the policy, its worlds |
| `weights` | `default_weights()` | θ for e |

The `DeckModel` (and its library and `CardDB`) must outlive the agent. It's only read, so one model
can serve every agent in an arena at once.

## The analysis

`analysis()` returns the last decision's plans, best first, as `ScoredPlan`s: the plan, the
and-tree's value for it, its mean playout value (the and-tree's value when it wasn't played out) and
its number of playouts. A win found by the or-tree is a single entry worth 1. This is what the
analysis mode in the client will show (see the roadmap's long-term goals).

## Counters

`stats()` and the arena's counters: plans played, wins found, decisions played out, playouts,
playout actions, plan actions, Greedy decisions, or-tree and and-tree nodes, and the seconds spent in
the or-tree, the and-tree, playouts and Greedy. `playout s` over `playouts` is what one playout costs.

## Tests

[`tests/test_flat_mc.cpp`](../../../../tests/test_flat_mc.cpp), tag `[flat_mc]`, on the Planner's
sample cards with a one-archetype library:

- a win two actions away taken without playouts;
- with several plans, each played out the same number of times, ranked by mean, the best followed;
- with one plan, the and-tree's best plan played without playouts (the same turn as the Planner's);
- whole sample-card games, the same for the same seed;
- whole real-deck games against random (skipped without generated card data);
- on the real decks, games that differ only in cards the player can't see getting the same choice
  and the same means;
- its counters and timings.
