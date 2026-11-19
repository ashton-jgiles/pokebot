# `src/client/main.cpp` — the `pokebot` command

**File:** [`src/client/main.cpp`](../../../src/client/main.cpp) · **Executable:** `pokebot`

Play against an agent, watch two agents play, or play back a saved game. Run it from the repository
root, after `pokebot-import`: it reads `data/cards/generated`, `data/cards/effects` and `data/decks`.

```
pokebot play   [--deck D] [--against D] [--agent random|biased] [--seat 0|1] [--seed S] [--save-replay FILE]
pokebot watch  [--deck D] [--against D] [--agent random|biased] [--seed S] [--as 0|1|all] [--step] [--save-replay FILE]
pokebot replay FILE [--as 0|1|all] [--step]
```

| Option | Default | Meaning |
|---|---|---|
| `--deck D` | the first deck in `data/decks` | Your deck (player 0's when watching): a file, or a name there such as `dragapult` |
| `--against D` | the second deck in `data/decks` | The other deck |
| `--agent A` | `biased` | The agent to play: `random` or `biased` ([random agents](../agents/random_agent.md)) |
| `--seat P` | 0 | Play as player 0 or 1 |
| `--seed S` | a random one | The game's seed, printed at the start |
| `--as P` | `all` | Show what player 0 or 1 saw, or everything |
| `--step` | | Wait for Enter before each turn (`q` stops) |
| `--save-replay F` | | Save the game as a [replay](../harness/replay.md) |

## `play`

You against an agent, through [`play_game`](../harness/match.md). You're a
[`HumanAgent`](../agents/human_agent.md), so you see only what your player could see at the table:
the board as your [observation](../engine/observation/observation.md), and the opponent's actions as
you saw them, with their hidden cards as "an unseen card".

```
Game 7: you are player 0 with basic-box.txt, against the biased agent with dragapult.txt.
Type the number of your choice, b to see the board again, or q to quit.

Turn 2, player 0's turn - waiting on player 0: main phase
You (player 0): 6 Prize cards, 46 in deck, 6 in hand, 0 in discard
  Active: Lillie's Clefairy ex 190/190 HP
  ...
Opponent (player 1, went first): 6 Prize cards, 46 in deck, 6 in hand, 0 in discard
  ...
  Hand:   6 cards
main phase:
   1. attach Fighting Energy to Active Lillie's Clefairy ex
   2. play Crispin
   ...
  12. end the turn
> 2
  > you: play Crispin
```

Quitting (`q`, or the end of the input) stops the game; `--save-replay` still saves what was played.
At the end it shows the final board, how the game ended and whether you won.

## `watch`

Two agents play a whole game, then it's shown turn by turn with
[`print_replay`](../harness/replay.md), as one player saw it or with everything shown. `--step`
waits for Enter before each turn.

## `replay`

Plays back a saved replay, like `pokebot-sim --replay`, with `--as` and `--step`.

## Errors

A bad option or deck prints the error and the usage, and exits with 2. Anything else, such as a
replay that no longer plays back, prints the error and exits with 1.

The client is a thin layer: everything it uses is in `pokebot_lib`, so the
[tests](../../../tests/test_client.cpp) cover it without running the executable.
