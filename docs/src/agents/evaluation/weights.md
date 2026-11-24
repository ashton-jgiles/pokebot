# `src/agents/evaluation/weights` and `src/agents/set_weights` — θ in files, and in every agent

**Files:** [`src/agents/evaluation/weights.hpp`](../../../../src/agents/evaluation/weights.hpp), [`src/agents/evaluation/weights.cpp`](../../../../src/agents/evaluation/weights.cpp), [`src/agents/set_weights.hpp`](../../../../src/agents/set_weights.hpp), [`src/agents/set_weights.cpp`](../../../../src/agents/set_weights.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

M10 compares the [evaluation](evaluation.md)'s hand-set weights with learned ones, so weights are
read from files and one function gives an agent the same weights everywhere it uses them.

```cpp
const Weights theta = load_weights("data/weights/hand.txt");
IsMctsAgentOptions options;
set_weights(options, theta);   // setup, playouts and macro-actions
```

## Files

One `feature weight` per line, the features named as `to_string(Feature)` names them, in any order;
blank lines and Windows line endings are fine. `load_weights` throws, naming the file and line, for
a feature that's missing, repeated or unknown, or a weight that isn't one finite number.
`save_weights` writes every digit, so a file loads back exactly.
[`data/weights/hand.txt`](../../../../data/weights/hand.txt) holds `default_weights()`; a test keeps
the two equal. [`data/weights/prizes.txt`](../../../../data/weights/prizes.txt) has only the prizes
weight (2, the rest 0), a baseline: against it, IS-MCTS with the hand weights scores 69.1% (63.8% to
73.9%, 320 games).

## Setting them

| `set_weights` for | Sets |
|---|---|
| `PlannerOptions` | its own |
| `FlatMcOptions` | its own and its playouts' |
| `IsMctsAgentOptions` | its own (the setup rule), its playouts' and its macro-actions' |
| `EndgameAgentOptions` | all of its IS-MCTS's, and its solver's search's |

Greedy takes weights in its constructor. An agent's search models its opponent with its own weights
(in playouts and macro-actions), since it can't know the opponent's.

A negative weight makes the and-tree's [`upper_bound`](../planner/and_tree.md) 1 everywhere, so it
prunes nothing with it and searches differently on the same budget.

In the [sim](../../../tools/sim/main.md): `--weights FILE` for every agent, and with `--agents A,B`,
`--weights-a FILE` and `--weights-b FILE` for one side.

## Tests

[`tests/test_weights.cpp`](../../../../tests/test_weights.cpp), tag `[weights]`:

- weights survive saving and loading exactly; `hand.txt` holds the default weights; line endings,
  blank lines and order don't matter; wrong files and a missing one are rejected;
- `set_weights` sets every copy in the table above (each is named in the test, so a field added to
  another options struct later isn't caught);
- on the real decks, Greedy plays the same games with the loaded hand weights as by default, and
  other weights change at least one.
