# `src/engine/game_state` — the game in memory

**Files:** [`src/engine/game_state.hpp`](../../../src/engine/game_state.hpp), [`src/engine/game_state.cpp`](../../../src/engine/game_state.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

The data structures of the formal model's world state. See the [engine overview](README.md) for
how they're used.

| Formal model | C++ |
|---|---|
| card instances `Inst = {1..120}` | `CardIndex` 0–119 + `CardTable` |
| a Pokémon in play `Mon` | `PokemonInPlay` |
| a player's zones `Z_i` | `PlayerState` |
| once-per-turn flags | `TurnFlags` |
| decisions and the pending stack | `Decision`, `GameState::pending` |
| world state `w` | `GameState` |

## Card instances

```cpp
using CardIndex = std::uint8_t;   // 0-59: player 0's cards, 60-119: player 1's, in decklist order
constexpr CardIndex kNoCard = 255;
```

| Constant / function | Value |
|---|---|
| `kPlayers` | 2 |
| `kDeckSize` | 60 |
| `kMaxCards` | 120 |
| `kBenchSize` | 5 |
| `kPrizeCount` | 6 |
| `kMaxEvolutionStack` | 3 (Basic, Stage 1, Stage 2) |
| `owner_of(card)` | 0 for cards 0–59, 1 for 60–119 |
| `opponent_of(player)` | `1 - player` |

## `CardTable`

Maps every card instance to its `CardDef`. It's built once per game, and every copy of the game
points to it.

```cpp
CardTable(const CardDB& db, const Decklist& deck0, const Decklist& deck1);
const CardDef& def(CardIndex card) const;
```

The constructor runs the [deck validator](../cards/deck_validator.md) on both decks and throws
`std::invalid_argument`, listing every problem, if either is illegal, so a game can never start
with an illegal deck.

**Lifetime:** it stores pointers into the `CardDB`, so the database must outlive the table, and
the table must outlive every `GameState` that uses it.

## `PokemonInPlay`

| Field | Meaning |
|---|---|
| `stack` | Evolution stack, bottom (Basic) to top (current card); `top()` returns the top |
| `energy` | Attached Energy cards |
| `tool` | Attached Tool, or `kNoCard` |
| `damage_counters` | Each is 10 damage |
| `turn_played` | Turn it came into play or last evolved (0 = setup); used by the evolution rules |

Special Conditions will be added in Milestone 3.

## `PlayerState`

| Field | Meaning |
|---|---|
| `deck` | `back()` is the **top** card, so drawing is a cheap `pop_back()` |
| `hand`, `prizes`, `discard`, `lost_zone` | The other zones |
| `active` | `std::optional<PokemonInPlay>`; empty only while choosing a new Active, or after the game is decided |
| `bench` | Up to 5 Pokémon |
| `mulligans` | Mulligans taken during setup |

## `TurnFlags`

`energy_attached`, `supporter_played`, `stadium_played`, `retreated`: the once-per-turn limits.
They're reset at the start of every turn.

## `TurnPhase`

`Setup`, `Main`, `AfterMain`, `Checkup`. Where the game is within a turn, so the engine knows what
happens once every pending decision is answered. See [`continue_game`](turn.md#continue_game).

## `Decision`

| Field | Meaning |
|---|---|
| `kind` | One of the `DecisionKind`s listed in the [engine overview](README.md#every-choice-is-a-decision) |
| `player` | Who decides |
| `amount` | Extra number the decision needs: bonus cards allowed, Energy still to discard |
| `target` | A `Slot`, e.g. the Benched Pokémon a retreat will bring in |

## `GameResult`

`Ongoing`, `Player0Wins`, `Player1Wins`, `Draw`. A draw is when both players win at once with the
same number of win conditions. The rulebook's tiebreaker game isn't modeled.

## `GameState`

| Field | Meaning |
|---|---|
| `cards` | Pointer to the shared `CardTable` |
| `players` | Both players' `PlayerState` |
| `current_player`, `first_player` | Whose turn it is; who went first |
| `turn` | 0 during setup; 1 is the first player's first turn, 2 the second player's |
| `phase` | `TurnPhase` |
| `flags` | `TurnFlags` for the current turn |
| `pending` | Stack of up to 8 open decisions; `back()` is decided first |
| `rng` | The game's random number generator |
| `result` | `GameResult` |

Helpers: `player(p)`, `def(card)`, `decision()` (the top of `pending`), `is_over()`.

A `static_assert` requires `GameState` to be **trivially copyable**. Adding a `std::vector` or
`std::string` to it stops the build, because copying the state is how search explores the game.
A whole game is **1,488 bytes**.

## Tests

[`tests/test_engine_state.cpp`](../../../tests/test_engine_state.cpp), tag `[engine]`: copies are
independent and equal, card ownership, `CardTable` numbering, and illegal decks are refused.
