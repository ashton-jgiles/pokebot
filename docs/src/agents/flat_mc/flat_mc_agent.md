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
FlatMcAgent agent(seed, model);   // defaults: K = 16 plans, N = 64 playouts, r = 2, Greedy with 1 world
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
[Greedy](../greedy/greedy_agent.md) with 3 worlds, as in the Planner, unless `search_choices` is on
(`--mc-choices`). Then every decision after setup that flat MC isn't following a plan through is
scored like the plans: each action as a one-action plan, played out in the same m = N / (number of
actions) worlds with the same seeds, the best mean taken. Setup still goes to Greedy's setup rule,
since a `World` can't be built before both players have set up. It's off by default until step 7.6
has measured it: it adds about 64 playouts to each of about 11 decisions a game.

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
| `plans` | 16 | K: the and-tree's top plans played out |
| `playouts` | 64 | N: playouts each decision, split evenly over the plans |
| `playout` | depth 2, Greedy with 1 world | The [playout options](playout.md#where-it-stops): r, the policy, its worlds |
| `weights` | `default_weights()` | θ for e |
| `search_choices` | off | Play out the decisions outside the main phase too, instead of asking Greedy |

The defaults are step 7.6's choice: of the settings measured against the Planner, r = 2 with N = 64
and K = 16 scored best for its cost, 67.9% (95% CI 65.6% to 70.2%) at 6.6 games/s on 16 threads
(see the [roadmap](../../../roadmap.md)). Seeing the opponent's reply mattered most (r = 1 against
0), then more plans (K = 8 against 4); deeper than r = 2, more playouts and better playout play all
paid less for their time.

The `DeckModel` (and its library and `CardDB`) must outlive the agent. It's only read, so one model
can serve every agent in an arena at once.

## The analysis

`analysis()` returns the last decision's plans, best first, as `ScoredPlan`s: the plan, the
and-tree's value for it, its mean playout value (the and-tree's value when it wasn't played out) and
its number of playouts. A win found by the or-tree is a single entry worth 1. This is what the
analysis mode in the client will show (see the roadmap's long-term goals).

## Counters

`stats()` and the arena's counters: plans played, wins found, decisions played out, playouts,
playout actions, plan actions, Greedy decisions, choices played out, or-tree and and-tree nodes, and
the seconds spent in the or-tree, the and-tree, playouts, Greedy and choices. `playout s` over
`playouts` is what one playout costs. Choices played out are counted in the decisions played out,
playouts and playout seconds too; `choices` and `choices s` are their share.

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
- its counters and timings;
- the defaults chosen in step 7.6;
- on the real decks, with `search_choices`, decisions outside the main phase played out (fewer left
  to Greedy than without), games still the same for the same seed. The sample cards have no
  searches or discards and offer each new Active only once, so only real decks have such choices.
