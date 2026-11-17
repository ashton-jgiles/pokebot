# `src/engine/invariants/invariants` — consistency checker

**Files:** [`src/engine/invariants/invariants.hpp`](../../../../src/engine/invariants/invariants.hpp), [`src/engine/invariants/invariants.cpp`](../../../../src/engine/invariants/invariants.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

```cpp
std::vector<std::string> check_invariants(const GameState& game);
```

Checks rules that must hold at **every** decision point. It returns a description of each broken
one; an empty list means the state is consistent. [`play_game`](../../harness/match.md) runs it after
every action by default, which is how the 100,000-game stress test finds bugs nobody wrote a test
for.

## What it checks

| Rule | Example message |
|---|---|
| Every card (0–119) is in exactly one place | `card 37 appears 2 times` |
| Cards are only in their owner's zones | `card 70 is in player 0's hand but belongs to player 1` |
| Every Pokémon in play has at least one card | `player 0's Bench 2 has no cards` |
| Its bottom card is a Basic Pokémon | `player 1's Active Pokemon doesn't start with a Basic Pokemon: …` |
| Each evolution matches the card below it by name | `…: Testmon doesn't evolve from Testmon` |
| Only Energy cards are attached as Energy | `… has a non-Energy card attached as Energy: …` |
| No Pokémon has damage ≥ its HP (it should have been Knocked Out), except while an effect is still running | `… has 990 damage on 60 HP and should have been Knocked Out` |
| A game in progress has a decision pending, for player 0 or 1 | `the game isn't over but no decision is pending` |
| A finished game has no pending decisions or effects | `the game is over but a decision or effect is still pending` |
| An effect's decision has an effect running | `an effect's decision is pending but no effect is running` |
| A card being played has an effect running | `… card is still being played but no effect is running` |
| A Stadium in play is a Stadium card, and at most one is in play | `… Stadium in play isn't a stadium card`, `two Stadiums are in play` |
| Only the Active has Special Conditions, and at most one of Asleep, Confused and Paralyzed | `… Bench 2 has a Special Condition, but only the Active Pokemon can`, `… more than one of Asleep, Confused and Paralyzed` |
| A main-phase decision is the current player's, during the main phase | |
| After setup, each player has an Active, unless they're choosing a new one | `player 0 has no Active Pokemon and isn't choosing one` |

## Tests

[`tests/test_invariants.cpp`](../../../../tests/test_invariants.cpp), tag `[invariants]`. Each test
corrupts a real game state on purpose and checks that the problem is reported: a duplicated card,
a missing card, a card in the wrong player's zone, a missed Knock Out, a non-Energy attached as
Energy, a wrong evolution, and a missing Active.
