# `src/engine/describe/describe` — states and actions in words

**Files:** [`src/engine/describe/describe.hpp`](../../../../src/engine/describe/describe.hpp), [`src/engine/describe/describe.cpp`](../../../../src/engine/describe/describe.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Human-readable text for debugging. It shows **everything**, including hidden cards.

```cpp
std::string describe(const GameState& game);                        // the whole board
std::string describe(const GameState& game, const Action& action);  // one action
```

## A state

```
Turn 1, player 1's turn - waiting on player 1: main phase
Player 0: 6 Prize cards, 47 in deck, 6 in hand, 0 in discard
  Active: Koraidon ex 230/230 HP
  Bench:  (empty)
  Hand:   Teal Mask Ogerpon ex, Cyrano, Chien-Pao, Ultra Ball, Hero's Cape, Energy Switch
Player 1 (went first): 6 Prize cards, 46 in deck, 6 in hand, 0 in discard
  Active: Dreepy 70/70 HP
  Bench:  Meowth ex 170/170 HP
  Hand:   Crispin, Fire Energy, Psychic Energy, Risky Ruins, Drakloak, Ultra Ball
```

Pokémon show remaining/maximum HP and attached Energy in brackets. When the game is over, the first
line ends with the result instead of the pending decision.

## An action

Described from the point of view of the player making the pending decision:

| Action | Example |
|---|---|
| `ChooseTurnOrder` | `go first` / `go second` |
| `SetupPlaceActive` | `make Dreepy the Active Pokemon` |
| `SetupPlaceBench`, `PlayBasic` | `put Meowth ex on the Bench` |
| `SetupDone` | `finish setting up` |
| `DrawMulliganCards` | `draw 2 extra card(s)` |
| `Evolve` | `evolve Active Dreepy into Drakloak` |
| `AttachEnergy` | `attach Psychic Energy to Bench 2 Dreepy` |
| `Retreat` | `retreat, bringing in Bench 1 Meowth ex` |
| `Attack` | `attack with Phantom Dive (200)` |
| `EndTurn` | `end the turn` |
| `DiscardEnergy` | `discard Fire Energy` |
| `Promote` | `promote Bench 1 Drakloak` |

`pokebot-sim --print` uses both functions to narrate a whole game; see [`tools/sim`](../../../tools/sim/main.md).

## Tests

[`tests/test_stress.cpp`](../../../../tests/test_stress.cpp), tag `[describe]`: across 20 full games
on the real decks, every state and every legal action is described, with no "unknown". A new
action kind without a description fails this test.
