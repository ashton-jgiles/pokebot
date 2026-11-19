# `src/harness/arena` — the match harness

**Files:** [`src/harness/arena.hpp`](../../../src/harness/arena.hpp), [`src/harness/arena.cpp`](../../../src/harness/arena.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Plays one agent against another on every deck pairing, in parallel, and reports the win rate with
95% confidence intervals: the proposal's main measure, and how each rung of the agent ladder is
shown to beat the one below. `pokebot-sim --agents A,B` ([sim](../../tools/sim/main.md)) is its
command line.

```cpp
const Contestant greedy{"greedy", [](std::uint64_t seed) { return std::make_unique<GreedyAgent>(seed); }};
const Contestant random{"random", [](std::uint64_t seed) { return std::make_unique<RandomAgent>(seed); }};
ArenaOptions options;
options.games = 1000; // per deck pairing
const ArenaResult result = run_arena(db, decks, greedy, random, options);
score_interval(result.overall); // A's (greedy's) win rate with its 95% interval
```

## What gets played

- **Every deck pairing:** A's deck against B's deck for every pair of decks, mirrors included. With
  our 4 decks that's 16 pairings.
- **Each seed in both seats:** `games` per pairing (rounded up to even) means `games / 2` seeds, each
  played once with A as player 0 and once with A as player 1. Each side keeps its deck when the
  seats swap. Which seat wins the coin flip depends on the seed, so the coin flip, and the choice of
  who goes first, falls to each side equally often.
- **The same seeds for every pairing:** `seed`, `seed + 1`, …
- **A fresh agent for every game,** seeded from the game's seed and its side, through the
  `Contestant`'s `make`.

Games are handed to `threads` worker threads (one per hardware thread by default) as they become
free. Each game's record goes in its own slot and the totals are added up in game order afterwards,
so **the results are the same for any number of threads.** The `CardDB` and the card tables are
shared read-only; every game copies its own state.

## What it reports

`ArenaResult` has, all from A's side:

| Field | Holds |
|---|---|
| `overall` | A's wins, losses and draws over every game |
| `a_first`, `b_first` | The same, split by who went first |
| `pairings` | One `Tally` for each (A's deck, B's deck), A's deck first |
| `ends` | Games by how they ended ([`GameEnd`](match.md)) |
| `a_wins_by`, `a_losses_by` | A's wins and losses by how the game ended. Prize cards and no Pokémon left are the winner's doing and deck-out the loser's, so a win by `DeckOut` means B's deck ran out and a loss by `DeckOut` means A's did |
| `games`, `turns`, `actions`, `seconds`, `threads` | For speed |
| `replays` | Every `replay_every`-th game as a [replay](replay.md), with the decks' and agents' names |

A `Tally`'s `score()` is A's points per game: 1 for a win, ½ for a draw. A game stopped by the
action limit counts as a draw.

**Confidence intervals** use the Wilson score interval, which stays inside [0, 1] and works near 0
and 1, where Greedy against Random sits:

  centre = (p + z²/2n) / (1 + z²/n),  half-width = z·√(p(1−p)/n + z²/4n²) / (1 + z²/n),  z = 1.96

`score_interval(tally)` is that interval for A's score, with draws counting half a win.

**A fairness check:** with Random on both sides, 16,000 games give A 49.6% (95% CI 48.8% to 50.3%),
and every mirror pairing is within 48.5% to 51.4%. The seat swap leaves no side an advantage the
interval can see.

## When a game fails

If an invariant breaks (with `check_invariants`), the arena stops the other threads and throws
`ArenaError`, which carries the game as a replay: its decks, agents, seed and moves up to the
broken invariant. Any other exception in a game, such as an agent's, stops the arena the same way
and is passed on as it was.

## Tests

[`tests/test_arena.cpp`](../../../tests/test_arena.cpp), tag `[arena]`:

- the Wilson interval for 50 of 100, 31 of 32, 0 of 10 and no games;
- a tally counting results for A from either seat, with a draw as half;
- every pairing played the same number of times and the totals adding up (wins and losses by how
  they ended too), with each seed played with A in each seat and each side keeping its deck;
- kept games playing back to the same result;
- the same results with 1 thread and with 4;
- an agent's failure stopping the arena and passed on.
