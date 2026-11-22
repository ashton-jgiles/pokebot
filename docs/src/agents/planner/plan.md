# `src/agents/planner/plan` — plans and plan values

**Files:** [`src/agents/planner/plan.hpp`](../../../../src/agents/planner/plan.hpp), [`src/agents/planner/plan.cpp`](../../../../src/agents/planner/plan.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

The turn planner's (Model 1) unit of play is a **plan**, the proposal's macro-action τ = (a₁, …, a_k):
[Det](action_class.md) actions a₁ … a_(k−1), then one action a_k that ends it, `Final` or `Uncertain`.

```cpp
struct Plan {
    std::vector<Action> actions;
    ActionClass last; // the class of the last action
};

GameState apply_plan(const GameState& game, const Plan& plan); // δ*(w, τ)
bool apply_plan_checked(GameState& game, const Plan& plan);    // the same, only while each action is legal

PlanScorer scorer(player, seed, 3); // m = 3 samples
scorer.value(game, plan);           // val(w, τ)
scorer.value(game, action, tried);  // the same for a plan ending with `action` at `game`
```

`apply_plan` trusts the plan: it's for a plan applied in the state it was found in. A plan found in
one world and applied in another goes through `apply_plan_checked`, which stops and returns false at
the first action that isn't legal there (and true if the game ends part-way). A plan's Det actions
are classed by applying them once, in the world the plan was found in, and a few depend on how the
player's own hidden cards are split between the deck and the Prize cards, which the player can't
see: a trigger that searches the deck does nothing where every copy of the card is a Prize card, and
asks for a choice where one is in the deck. Two worlds with the same observation can then disagree
on whether the plan's next action is still a main-phase action. Flat MC and IS-MCTS use the checked
version (found in step 8.5, when IS-MCTS's first arena run applied such a plan).

## Plan value

The proposal's

  val(w, τ) = 1/m · Σ_j e_i(δ*(w, τ)^(j))

averaged over m sampled outcomes if the last action involves chance or hidden cards, and with m = 1
otherwise; a finished game is scored with u_i instead of e_i ([evaluation](../evaluation/evaluation.md)).

- **A `Final` plan** is scored once, on the state after it: by its class, its result is the same
  whatever the hidden cards are.
- **An `Uncertain` plan** is scored in `samples` [worlds](../world.md) dealt from what the player sees
  just before its last action, each with its own luck (its own hidden cards and random number
  generator), and the scores are averaged. A draw is scored over different draws, a coin-flip attack
  over heads and tails.
- **The worlds dealt for one state are kept** until a different state is scored. A planner scores
  every way of ending a plan at one state together, so they're all tried with the same luck: common
  random numbers, as in the [greedy agent](../greedy/greedy_agent.md).
- **The worlds for a state depend only on the scorer's seed and the state** (its
  [hash](state_hash.md)), not on what was scored before. So a plan has the same value however a search
  reaches it, and two searches (with f_bound on and off, say) can be compared plan for plan.
- **It times itself:** `seconds()` is the time spent in `value`, and `uncertain()` the number of
  uncertain plan endings scored, for the Planner's counters.
- **Follow-up choices come first.** After the last action, the player's own choices that follow from
  it (the Pokémon Seek takes, the cards Ultra Ball discards) are made with `finish_choices`, as Greedy
  does, and only then is the state scored. Otherwise a search would be scored partway through.

## Follow-up choices

`follows_up(game, player)` is true while the decision is `player`'s and isn't a new main-phase one
(`MainPhase` or `ExtraAttack`). `finish_choices` makes those choices, each the one that scores best
right after it (the first of any tied), up to 64 of them. They were written for the greedy agent in
step 5.6c and moved here so the planner uses the same rule.

## Tests

[`tests/test_plan.cpp`](../../../../tests/test_plan.cpp), tag `[planner]`:

- `apply_plan` plays a plan's actions in order;
- `apply_plan_checked` plays the same plan, refuses one with an action that isn't legal when it comes,
  and leaves the state as it was when the first action already isn't;
- a final plan's value is e after it, with one sample or eight;
- a plan ending in a draw is worth e after the draw, since every sampled world agrees on what e can
  see; a deck search is scored after taking a Pokémon, not partway through;
- a plan ending in a coin-flip attack is averaged over heads and tails: with one sample both values
  come up over different seeds, with sixteen the value is strictly between them, and the same state
  and the same seed give the same value.
