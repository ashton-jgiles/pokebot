# `src/agents/planner/or_tree` — the turn planner's or-tree mode

**Files:** [`src/agents/planner/or_tree.hpp`](../../../../src/agents/planner/or_tree.hpp), [`src/agents/planner/or_tree.cpp`](../../../../src/agents/planner/or_tree.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Model 1's or-tree mode answers *"is there a line this turn that achieves goal γ?"*. It's the first
thing the decision procedure asks in the player's main phase, with γ_win: if a winning line exists,
play it.

```cpp
const OrTreeResult result = or_tree_search(game, player, Goal{GoalKind::KnockOut});
if (result.found) {
    result.plan; // Det actions, then the Final action that reaches the goal
}
```

## The model (from the proposal)

A node is (w, γ) with a solution of yes, no or unknown.

- **Goals:**
  - `Win`, γ_win: the game is over and the player won;
  - `KnockOut`, γ_KO: the opponent's Active at the start of the search has been Knocked Out (it's no
    longer in play, and the opponent has had a Pokémon Knocked Out this turn since the search began);
  - `Prizes`, γ_prize^k: the player has taken at least k Prize cards this turn.
- **Alternatives:** a node's children are the states after each of its [Det](action_class.md) actions.
- **Yes** if γ holds at the node, or some `Final` action from it reaches γ. **No** if no `Final` action
  does and there's no Det action, or the node's state is already in the [closed set](state_hash.md).
- **Uncertain actions aren't searched:** a line through chance or hidden cards can't be sure to reach γ.
- **The search stops** at a yes, when every leaf is no, or when the node budget B_or (2,000 by default)
  is used up.
- **The output** is the actions from the root to the yes node, then the `Final` action that reached γ.

## Search control

- **f_leaf, greedy best-first:** the open leaf with the smallest

    h_KO(w) = max(0, HP left on the opponent's Active − maxDmg(w))

  goes first, where maxDmg is the most damage any of the player's Pokémon in play could do to it with
  one attack, ignoring Energy (a relaxed problem; Weakness, Resistance and other effects still apply,
  through [`rules::attack_damage`](../../engine/rules/rules.md)). Ties go to the deepest leaf, then the
  first made. `knock_out_distance` computes h_KO.
- **f_trans:** a node's children are made in the order of their action class: actions that make an
  attack possible (none was legal before, one is after) first, then evolving, attaching Energy,
  Items, Supporters, any other action (Abilities, Tools, Stadiums, the choices an action leads to),
  benching, and retreating. Among leaves with the same h and depth, that order decides.
  `action_order` gives it, and the [and-tree](and_tree.md) uses it too.

**Open leaves aren't kept as states.** A leaf is its parent's index and the action, and is applied only
when it's chosen; only expanded nodes keep a state. A budget of 2,000 nodes holds at most 2,000 states
(about 8 MB), however many children they have.

## What it reports

`OrTreeResult` has the plan, whether it was found, whether the budget ran out, and three counts for
the proposal's measurements: nodes `expanded`, children `generated`, and nodes skipped because their
state was already `closed`. `OrTreeOptions::closed_set = false` turns the closed set off to measure what
it saves.

## Tests

[`tests/test_or_tree.cpp`](../../../../tests/test_or_tree.cpp), tag `[planner]`:

- attaching Energy and attacking for a Knock Out;
- evolve, attach, attack, the proposal's demonstration: Tackle can't Knock Out, Big Tackle can;
- retreating a Pokémon with no Energy, then attacking with the one switched in;
- a win by the last Prize card, one Prize card, and no plan for two when only one Knock Out is possible;
- no plan when none exists, with the closed set skipping nodes, fewer nodes expanded with it than
  without, and a budget of one node stopping the search;
- on the real decks, every plan found for a Knock Out is legal action by action and takes a Prize card.
