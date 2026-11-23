# `src/agents/is_mcts/tree` — the IS-MCTS tree

**Files:** [`src/agents/is_mcts/tree.hpp`](../../../../src/agents/is_mcts/tree.hpp), [`src/agents/is_mcts/tree.cpp`](../../../../src/agents/is_mcts/tree.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

The tree IS-MCTS (M8) grows over many determinizations, and the arithmetic on it: UCB1 with
availability counts, backpropagation and the final choice. It knows nothing about the engine; the
search (step 8.4) decides what's legal in each world and drives it.

```cpp
SearchTree tree(hash_observation(root_view), me);
// each iteration: mark the children legal in this world available, then add an untried move
// or select among them, down the tree; evaluate; then
tree.backpropagate(path, value_for_player_0);
const std::size_t best = tree.most_visited(tree.root());
tree.node(best).move;   // the plan to play
```

## Nodes

A `TreeNode` is one information set of the searching player, the proposal's n = (η, actor, N, Q, A):

| Field | Holds |
|---|---|
| `key` | The [information-set key](info_set.md) of the searcher's observation there (η) |
| `actor` | Who decides there |
| `move` | The move (a plan, or a single action) that first led here from the parent |
| `visits` | N |
| `availability` | A: how often this node's move was legal when its parent was visited |
| `reward` | Q for each player: `reward[0]` adds up the values for player 0, `reward[1]` their negation (the game is zero-sum) |
| `children` | Its children, as indices into the tree |

`mean(player)` is Q_player / N, or 0 before the first visit.

Nodes are kept in one vector and refer to each other by index, so the tree grows cheaply and has no
pointers to keep valid. The root is index 0.

## Children by information set

`find_child(parent, key)` and `add_child(parent, key, actor, move)` work on a parent's children by
key, so two moves that leave the searcher seeing the same thing lead to the same child: two orders of
the same plan, or the opponent playing the same card under different numbers in different worlds. A
child keeps the first move that reached it. For the searcher's own nodes that's the plan to play,
since the searcher sees the same thing in every world. `add_child` throws `std::logic_error` if the
parent already has a child with that key. Children are found by a linear scan; a node has a few
dozen at most.

## Selection, availability and backpropagation

- **`mark_available(children)`** adds 1 to each one's availability. On each visit to a node the search
  marks every existing child that's legal in that iteration's world. A new child starts at 1, since it
  was legal on the visit that added it.
- **`select(parent, available, C)`** applies UCB1 with availability counts to the children legal in
  ŵ, with the rewards of the parent's actor j:
  argmax_c Q_j(c)/N(c) + C·√(ln A(c) / N(c)). An unvisited child is taken first; ties go to the
  first in `available`. An empty list throws `std::invalid_argument`. Using A(c) rather than the
  parent's visits keeps a move that's rarely legal (it needs a card the opponent seldom holds) from
  being under-explored when it is.
- **`backpropagate(path, value)`** adds a visit to every node on the path from the root, `value` to
  `reward[0]` and `−value` to `reward[1]`. `value` is always for player 0, so the caller turns e for
  the searcher into player 0's value once.
- **`most_visited(parent)`** is the output: the child with the most visits, ties to the higher mean
  for the parent's actor, then the first; `kNone` without children. Visits are a steadier choice than
  the highest mean (proposal Section 8.4).

## Tests

[`tests/test_tree.cpp`](../../../../tests/test_tree.cpp), tag `[is_mcts]`:

- the root alone, children found by key, a repeated key rejected;
- backpropagation adding visits and the value for player 0, negated for player 1;
- UCB1 taking an unvisited child first, then the best mean for the node's actor, for either player;
- the exploration term growing with availability;
- on a made-up two-level game driven the way the search will drive it (mark, expand or select,
  evaluate with noise, back up), the visits concentrating on the minimax moves for both players;
- a move legal on only every third iteration: its availability counts only those, it's never visited
  more than it was available, and its mean is right.
