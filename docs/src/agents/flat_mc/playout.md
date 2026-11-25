# `src/agents/flat_mc/playout` — playing a determinization on

**Files:** [`src/agents/flat_mc/playout.hpp`](../../../../src/agents/flat_mc/playout.hpp), [`src/agents/flat_mc/playout.cpp`](../../../../src/agents/flat_mc/playout.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Flat Monte Carlo (M7) scores a plan by playing on from it: in a determinization ŵ (a
[`World`](../world.md#with-a-deck-model) dealt from the [deck model](deck_model.md)), apply the plan,
let a policy play both sides for a while, and score where the game got to with e. A playout is that
playing on. IS-MCTS (M8) uses the same thing for its evaluate step, with r turns of Greedy.

```cpp
PlayoutOptions options;          // depth 1, Greedy with 1 world
const PlayoutResult result = play_out(world_after_plan, me, seed, options);
result.value;                    // e for `me` where it stopped, or u if the game ended
```

## Where it stops

A playout stops at the start of the searching player's **r-th next main phase** (`depth`), when the
game ends, or after `max_actions` actions (2,000), and is scored there with `evaluate`, which is u
for a finished game.

Only main phases in later turns count. Started part-way through the searcher's own turn (after a
plan), r = 1 plays the rest of that turn and the opponent's reply, and stops as the searcher's next
turn begins: the plan is scored after the opponent has answered it. Started during the opponent's
turn (choosing a new Active after a Knock Out, say), r = 1 stops at the searcher's next turn. r = 0
scores the state as it is.

## Who decides

Both players are played by the same policy, each decision going to that seat's agent with
`observe(game, who)`, exactly as `ask` does in the [match harness](../../harness/match.md). So in ŵ
the opponent decides knowing its own sampled hand and seeing the searcher's hand only as a count, and
the searcher decides knowing what it knows. Nothing in a playout hands an agent the game state, so
nothing that decides for the searcher can see more than they could at the table.

| Policy | Agents | Cost (one turn, M7 profiling) |
|---|---|---|
| `Greedy` | [`GreedyAgent`](../greedy/greedy_agent.md) with `greedy_worlds` worlds (1 by default) | 0.28 ms with 1 world, 0.79 ms with 3 |
| `Biased` | [`BiasedRandomAgent`](../random/random_agent.md) | 0.018 ms |

Greedy inside a playout builds its own worlds from its own observation, with the other side's unseen
cards as the placeholder, as it always has; e reads only what both players can see, so it doesn't
need more.

Greedy plays with `weights`, the ones the end is scored with, unless `policy_weights` is set, which
only changes who plays: the end is still scored with `weights`. Both sides play with the same ones.

`to_string(policy)` and `playout_policy(name)` convert to and from `"greedy"` and `"biased"`;
`playout_policy` throws `std::invalid_argument` for any other name.

## Seeds

`play_out(game, player, seed, options)` seeds the two agents from `seed`; chance (draws, shuffles,
flips) comes from the state's own random number generator, which a `World` deals. The same state and
seed give the same playout, so Flat MC can give every plan the same seeds and compare plans on the
same luck.

`play_out(game, player, mine, theirs, options)` takes the two agents instead (`mine` decides for
`player`); `policy` and `greedy_worlds` are then unused. The tests use it to record what each side is
shown.

## The result

| Field | Holds |
|---|---|
| `value` | e for `player` where the playout stopped, or u if the game ended |
| `actions` | Actions played |
| `turns` | Turns started |
| `over` | Whether the game ended |
| `end` | The state it stopped in |

A negative depth throws `std::invalid_argument`.

## Tests

[`tests/test_playout.cpp`](../../../../tests/test_playout.cpp), tag `[playout]`:

- depth 0 scores the state as it is;
- a playout stopping at the start of the player's r-th next main phase, from their own main phase
  (2r turns on) and from the opponent's (2r − 1);
- a playout to the end of the game scored with u;
- the same seed playing the same playout, under both policies, and other seeds others;
- `max_actions` stopping a playout, and a negative depth rejected;
- each player deciding from its own observation, the opponent never shown the searcher's hand in
  a world;
- the policies by name;
- on the real decks, with the deck model and both policies: games that differ only in cards the
  searcher can't see (the opponent's hand and the searcher's Prize cards swapped with their decks)
  giving exactly the same playouts.
