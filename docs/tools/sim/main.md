# `tools/sim/main.cpp` — the `pokebot-sim` command

**File:** [`tools/sim/main.cpp`](../../../tools/sim/main.cpp) · **Executable:** `pokebot-sim`

Plays many games between random agents on the real decks, either to stress-test the engine and
measure its speed, or to narrate one game move by move.

## Usage

Run from the repository root. It reads `data/cards/generated`, `data/cards/effects` and every
`data/decks/*.txt`, so run `pokebot-import` first.

```bash
./build/debug/tools/sim/pokebot-sim.exe [--games N] [--agent random|biased] [--seed S] [--no-check] [--print]
```

| Option | Default | Meaning |
|---|---|---|
| `--games N` | 1000 | Games to play |
| `--agent A` | `biased` | `random` or `biased` (see [random agents](../../src/agents/random_agent.md)) |
| `--seed S` | 1 | Game `i` uses seed `S + i`; agents are seeded from it too |
| `--no-check` | | Skip the invariant check after every action (about 2.5× faster) |
| `--print` | | Play **one** game with seed `S` and print every action |

**Pairings:** decks are sorted by file name. Game `i` plays deck `i mod n` against deck
`(i / n) mod n`, so every pairing, including mirrors, comes up equally often.

## Stress test output

```
Played 100000 games (biased vs biased) across 4 decks in 129.9 s
  769.7 games/s, 0.1 million actions/s
  average 65.5 turns, 101.6 actions per game
Ended by:
  Prize cards        38714  (38.7%)
  no Pokemon left    19640  (19.6%)
  deck-out           41646  (41.6%)
  draw                   0  (0.0%)
  action limit           0  (0.0%)
Player who went first won 31.6% of decided games
Invariants checked after every action: no problems found
```

If an invariant breaks, it stops with the seed, the broken rules and the game state (see
[`play_game`](../../src/harness/match.md)).

**Speed:** use the release build for real numbers. On the reference machine, `--games 20000
--agent biased --seed 7`:

| Version | `--no-check` | With checks |
|---|---|---|
| Milestone 2 (no effects) | about 10,000 games/s, 1.0 million actions/s | about 4,100 games/s |
| Step 3.4 (attack and Trainer effects, Special Conditions) | about 5,100 games/s, 0.7 million actions/s | about 1,700 games/s |
| Step 3.5a (damage formulas) | about 4,250 games/s, 0.6 million actions/s | about 1,650 games/s |
| Step 3.7a (lasting and always-on effects) | about 4,340 games/s, 0.6 million actions/s | |
| Step 3.7b (Abilities you use) | about 3,920 games/s, 0.6 million actions/s (games are 44.5 turns now: draw Abilities empty decks faster) | |
| Step 3.8a (Special Energy, exact cost matching) | about 3,720 games/s, 0.6 million actions/s | |

Games are longer now (about 145 actions each, up from about 100), so compare actions/s between
versions. The release preset uses link-time optimization; without it the
[rules](../../src/engine/rules/rules.md) questions cost about 15%. Profiling the engine is part of
the Milestone 3 sign-off, before search needs the speed.

```bash
cmake --preset release
cmake --build --preset release
./build/release/tools/sim/pokebot-sim.exe --games 100000 --no-check
```

## Narrating a game

```bash
./build/debug/tools/sim/pokebot-sim.exe --print --seed 3
```

This plays the first two decks against each other and prints the board at the start of every turn,
then each action, using [`describe`](../../src/engine/describe/describe.md):

```
Turn 1, player 1's turn - waiting on player 1: main phase
...
  > player 1: attach Psychic Energy to Active Dreepy
  > player 1: retreat, bringing in Bench 1 Meowth ex
  > player 1: end the turn
```

It ends with the final board and how the game ended. Narration includes hidden cards: it's a
debugging view, not a player's view.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Every game finished |
| 1 | A game hit the action limit, an invariant broke, or loading data failed |
| 2 | Bad arguments (usage is printed) |
