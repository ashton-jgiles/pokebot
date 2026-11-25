# `src/agents/planner/and_tree` — the turn planner's and-tree mode (branch-and-bound)

**Files:** [`src/agents/planner/and_tree.hpp`](../../../../src/agents/planner/and_tree.hpp), [`src/agents/planner/and_tree.cpp`](../../../../src/agents/planner/and_tree.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Model 1's and-tree mode answers *"what are the best ways to play this turn?"*. It returns the top-K
plans by value. Until IS-MCTS (M8), the Planner plays its best plan when the
[or-tree](or_tree.md) finds no win.

```cpp
PlanScorer scorer(player, seed);
const AndTreeResult result = and_tree_search(game, player, scorer, AndTreeOptions{});
result.plans.front().plan;  // the best plan
result.plans.front().value; // val(w, τ)
```

## The model (from the proposal)

The and-tree is the optimization form of and-tree search. The problem "the best completion of the
turn from w" divides into "the best completion after a_j" for each [Det](action_class.md) action a_j,
and the answers combine by comparison:

  best(w) = max( max over b in Bnd(w) of val(w, (b)),  max over j of best(δ(w, a_j)) )

- Every action that ends a plan (`Final` or `Uncertain`) is scored where it's found, with
  [`PlanScorer`](plan.md), and kept if it's among the best K so far.
- A node with no Det action is solved by scoring its plan-ending actions.
- A node whose state is already in the [closed set](state_hash.md) is solved too: every plan through
  it ends in a state already scored.
- **The output** is the top-K plans, best first, **one per distinct end state**: two plans that end in
  the same state (attach then bench, bench then attach) count once. A `Final` plan's end state is the
  state after it; an `Uncertain` plan's is the state before its last action together with that action.
  `keep = 0` keeps every distinct end state, which Model 3 will use.
- The node budget B_tp is 2,000 by default.

## Search control

- **f_leaf:** depth-first, and among a node's children the one whose state e scores best first, so a
  strong incumbent plan is found early, which tightens the bound.
- **f_trans:** ties between children go by the same action-class order as the or-tree
  (`action_order`).
- **f_bound:** v* is the value of the K-th best plan so far; a child is labelled solved without being
  expanded if ub(child) ≤ v*. It's off when `keep = 0`, as the proposal says.

## The upper bound

`upper_bound(game, player, weights)` is an optimistic e for the best plan from a state, from a relaxed
problem: each feature is raised to the most it could reach this turn, never lowered, and e is taken of
the result.

| Feature | Raised to |
|---|---|
| Prizes | + the most Prize cards one Knock Out could take, by the most damage any of the player's Pokémon in play, or an evolution in hand (at double its printed damage), could do with any Energy. If that takes the last Prize cards, the bound is 1 |
| ActiveHp | the healthiest of the player's Pokémon against the opponent's Pokémon with the least HP left after that damage |
| TurnsToKnockout | the turns the opponent needs to Knock Out the player's sturdiest Pokémon, minus the player's fewest turns to Knock Out any of theirs |
| ReadyAttackers | + one more ready attacker |
| Energy | + 2 Energy |
| Bench | + every free Bench space |
| Hand | the player's hand counted as 7 |
| Deck | + the hand shuffled back in |
| Conditions | the opponent's Active with 2 more Special Conditions, the player's with none |
| PrizesAtRisk | the opponent's most valuable Pokémon in front of the player's least valuable |

**It isn't proven to be a true upper bound** for every card. An effect could raise a feature further
than this allows. It needs non-negative weights (with a negative weight it returns 1, so nothing is
pruned), and returns 1 for a weight on a [candidate feature](../evaluation/evaluation.md#candidates), which
it has no term for. The tests check, on positions built for it and on real-deck positions, that f_bound never
changes the best plan found. If it's too loose to prune well, the proposal's fallback is beam search.

## What it reports

`AndTreeResult` has the plans and five counts for the proposal's measurements: nodes `expanded`, Det
children `generated`, nodes skipped as `closed`, children `pruned` by f_bound, and plan endings
`scored`. `closed_set` and `bound` can be turned off to measure what each saves.

## Tests

[`tests/test_and_tree.cpp`](../../../../tests/test_and_tree.cpp), tag `[planner]`:

- the best plan evolves, attaches and attacks for the Knock Out;
- the best plan is worth at least any one action that ends a plan at the root, and a fresh scorer
  gives it the same value;
- the top five plans come best first, and keeping every end state finds more, prunes nothing and has
  the same best;
- the closed set and f_bound each expand fewer nodes, and all four combinations find the same best
  value;
- the upper bound is at least e, and 1 when a Knock Out would take the last Prize card;
- a weight on a candidate feature makes it 1;
- on the real decks, the bound is at least e at every decision, the best plan is legal and worth what
  the search says, and f_bound doesn't change the best value when neither search ran out of budget.
