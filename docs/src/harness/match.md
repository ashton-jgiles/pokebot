# `src/harness/match` — playing games between agents

**Files:** [`src/harness/match.hpp`](../../../src/harness/match.hpp), [`src/harness/match.cpp`](../../../src/harness/match.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

## `play_game`

```cpp
GameRecord play_game(const CardTable& cards, std::uint64_t seed, Agent& player0, Agent& player1,
                     const MatchOptions& options = {});
```

Plays one game from `seed`. At every decision it asks the agent whose decision it is, which may not
be the current player (e.g. choosing a new Active after a Knock Out).

With `check_invariants` on, it runs [`check_invariants`](../engine/invariants/invariants.md) before the first
action and after every action. If anything is inconsistent, it throws `std::logic_error` with:
- the **seed**, so the game can be replayed;
- how many actions in it broke;
- every broken rule;
- the full [description](../engine/describe/describe.md) of the game state.

### `MatchOptions`

| Field | Default | Meaning |
|---|---|---|
| `max_actions` | 20000 | Safety net: a game this long stops and counts as `ActionLimit`, which is treated as a bug |
| `check_invariants` | `true` | Check after every action (about 2.5× slower) |

### `GameRecord`

| Field | Meaning |
|---|---|
| `result` | `GameResult` |
| `end` | `GameEnd`: how it ended |
| `first_player` | Who went first |
| `turns`, `actions` | Length of the game |

## How a game ended

```cpp
enum class GameEnd { PrizeCards, NoPokemon, DeckOut, Draw, ActionLimit };
GameEnd how_game_ended(const GameState& game);
const char* to_string(GameEnd end);   // "Prize cards", "no Pokemon left", ...
```

`how_game_ended` works it out from the final state:
- the winner has no Prize cards left → `PrizeCards`;
- otherwise, the loser has no Pokémon in play → `NoPokemon`;
- otherwise → `DeckOut`.

A game that isn't over is `ActionLimit`.

## Used by

- [`pokebot-sim`](../../tools/sim/main.md), for the 100,000-game stress test and speed numbers.
- [`tests/test_stress.cpp`](../../../tests/test_stress.cpp): 2,000 real-deck games on every `ctest` run.
