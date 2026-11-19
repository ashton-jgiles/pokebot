# `src/engine/describe/describe` — states and actions in words

**Files:** [`src/engine/describe/describe.hpp`](../../../../src/engine/describe/describe.hpp), [`src/engine/describe/describe.cpp`](../../../../src/engine/describe/describe.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Human-readable text. Describing a `GameState` shows **everything**, including hidden cards, for
debugging; describing an [`Observation`](../observation/observation.md) shows only what one player can see.

```cpp
std::string describe(const GameState& game);                        // the whole board
std::string describe(const GameState& game, const Action& action);  // one action
std::string describe(const Observation& observation);               // the board as one player sees it
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

Pokémon show remaining/maximum HP, attached Energy in brackets, and any Special Conditions, such
as `(Asleep) (Poisoned)`. When the game is over, the first
line ends with the result instead of the pending decision.

## An observation

The viewer's side comes first, as "You". The opponent's hand is a count, with any cards in it the
viewer was shown. From a real game:

```
Turn 6, player 1's turn - waiting on player 1: main phase
You (player 0, went first): 6 Prize cards, 40 in deck, 3 in hand, 2 in discard
  Active: Iron Leaves ex 220/220 HP
  Bench:  Lillie's Clefairy ex 190/190 HP [Lightning Energy]; Teal Mask Ogerpon ex 210/210 HP [Fighting Energy]; Mega Kangaskhan ex 300/300 HP [Grass Energy]; Enamorus 120/120 HP
  Hand:   Special Red Card, Cyrano, Lightning Energy
Opponent (player 1): 6 Prize cards, 43 in deck, 4 in hand, 4 in discard
  Active: Fezandipiti ex 210/210 HP [Darkness Energy, Fire Energy]
  Bench:  (empty)
  Hand:   4 cards: Dudunsparce and 3 unknown
Stadium: Area Zero Underdepths
```

A Tool shows in braces after the Energy, `{Hero's Cape}`. Setup Pokémon the viewer can't see yet
show as `Face down: 2 Pokemon`, and deck cards whose place the viewer knows as
`Known in deck: Dreepy (top), Ultra Ball (2 from the top)`.

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

## An action from an observation

```cpp
std::string describe(const Observation& observation, const Action& action);
```

The same text, made only from what `observation.viewer` can see, for showing a player their
choices (see [`HumanAgent`](../../agents/human_agent.md)). For the deciding player it reads the same as
`describe(game, action)`; a test checks this on every action of 30 sample games and 50 real-deck
games.

A choice of one of the opponent's Pokémon says so, from [`choice_owner`](../engine.md):
`choose the opponent's Bench 2 Dreepy`. This applies to `describe(game, action)` too, which
used to say only `choose Bench 2 Dreepy`.

## A decision

```cpp
std::string describe_decision(const Observation& observation);
```

What the viewer is being asked, with the card whose effect asks it: `main phase`,
`choose cards for an effect (Ultra Ball)`, `choose a Pokemon for an effect (Boss's Orders)`.

## An action, as one player saw it

```cpp
std::string describe(const GameState& before, const Action& action, const GameState& after, int viewer);
```

The same text, but a card `viewer` knew the place of neither before nor after the action is
"an unseen card", or "a face-down Pokemon" when it's put down during setup. So the opponent's
search that doesn't reveal reads `choose an unseen card`, while one that reveals names the card.
[Replays](../../harness/replay.md) use it to show a game as one player saw it.

## Tests

[`tests/test_stress.cpp`](../../../../tests/test_stress.cpp), tag `[describe]`: across 20 full games
on the real decks, every state and every legal action is described, with no "unknown". A new
action kind without a description fails this test. [`tests/test_observation.cpp`](../../../../tests/test_observation.cpp)
describes every observation in its random games, and checks the opponent's hand shows as a count.
[`tests/test_replay.cpp`](../../../../tests/test_replay.cpp) checks, across 30 random games, that each player's
description of an action hides exactly the cards they don't know.
