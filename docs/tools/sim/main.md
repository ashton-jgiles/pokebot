# `tools/sim/main.cpp` — the `pokebot-sim` command

**File:** [`tools/sim/main.cpp`](../../../tools/sim/main.cpp) · **Executable:** `pokebot-sim`

Plays many games between random agents on the real decks, either to stress-test the engine and
measure its speed, or to narrate one game move by move. It also saves and prints
[replays](../../src/harness/replay.md).

## Usage

Run from the repository root. It reads `data/cards/generated`, `data/cards/effects` and every
`data/decks/*.txt`, so run `pokebot-import` first.

```bash
./build/debug/tools/sim/pokebot-sim.exe [--games N] [--agent random|biased|greedy] [--seed S] [--no-check]
                                        [--print [--save-replay FILE]] [--replay FILE] [--as 0|1|all]
```

| Option | Default | Meaning |
|---|---|---|
| `--games N` | 1000 | Games to play |
| `--agent A` | `biased` | `random`, `biased` (see [random agents](../../src/agents/random_agent.md)) or `greedy` ([greedy agent](../../src/agents/greedy_agent.md)); both players get the same kind |
| `--seed S` | 1 | Game `i` uses seed `S + i`; agents are seeded from it too |
| `--no-check` | | Skip the invariant check after every action (about 2.5× faster) |
| `--print` | | Play **one** game with seed `S` and print every action |
| `--save-replay FILE` | | With `--print`: also save that game as a replay |
| `--check-replays` | | Save every game as replay text, load it and play it back, checking it ends in exactly the same state. A game that doesn't is saved to `replays/mismatch-seed-S.json`, and the run exits with 1 |
| `--replay FILE` | | Print a saved replay instead of playing |
| `--as 0\|1\|all` | `all` | With `--print` or `--replay`: show only what that player saw |

**Pairings:** decks are sorted by file name. Game `i` plays deck `i mod n` against deck
`(i / n) mod n`, so every pairing, including mirrors, comes up equally often.

## Stress test output

```
Played 100000 games (biased vs biased) across 4 decks in 52.0 s
  1924.2 games/s, 0.3 million actions/s
  average 40.1 turns, 151.7 actions per game
Ended by:
  Prize cards        31067  (31.1%)
  no Pokemon left    18325  (18.3%)
  deck-out           50565  (50.6%)
  draw                  43  (0.0%)
  action limit           0  (0.0%)
Player who went first won 51.0% of decided games
Invariants checked after every action: no problems found
```

If an invariant breaks, it stops with the seed, the broken rules and the game state (see
[`play_game`](../../src/harness/match.md)), and saves the game to `replays/invariant-seed-S.json`. Play it
back with `--replay` to see every action up to the one that broke it.

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
| Step 3.9 (one-of-a-kind cards, 8-space Bench) | about 3,130 games/s, 0.5 million actions/s | |
| Step 3.10 (`apply_legal`, no heap use when listing main-phase actions) | about 5,480 games/s, 0.8 million actions/s | about 1,920 games/s |
| Step 4.1 (each player's knowledge recorded after every action) | about 4,630 games/s, 0.7 million actions/s | |
| Step 4.3 (each agent is shown an observation, built at every decision) | about 2,860 games/s, 0.4 million actions/s | |
| Step 4.4 (every action recorded for replays) | about 2,860 games/s: no measurable cost | |
| Milestone 4 sign-off | about 2,860 games/s, 0.4 million actions/s | about 1,420 games/s; about 1,090 with `--check-replays` and no invariant checks |

Games are longer now (about 145 actions each, up from about 100), so compare actions/s between
versions. The release preset uses link-time optimization; without it the
[rules](../../src/engine/rules/rules.md) questions cost about 15%.

At the Milestone 3 sign-off, the main costs were:
- **`apply` built the legal action list a second time** to check the action. The harness now calls
  `apply_legal` with the list it already has.
- **Merging duplicate choices** used a heap-allocated list four times per main phase. It now looks
  through the actions already offered instead.
- **Trainer requirements were checked once per copy** in hand. Three Rare Candy meant three searches
  of every Stage 2 against every card in the game. Each card name is checked once now, and a
  requirement stops at the first pair it finds.

Together these took the simulator from about 3,130 to 5,480 games/s. Seeded runs end exactly as
before, so no game plays differently.

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

It plays the game with [`play_game`](../../src/harness/match.md) and prints it with
[`print_replay`](../../src/harness/replay.md), so it's the same as printing a saved replay.

## Replays

```bash
./build/debug/tools/sim/pokebot-sim.exe --print --seed 7 --save-replay replays/game7.json
./build/debug/tools/sim/pokebot-sim.exe --replay replays/game7.json --as 1
```

The first saves the game it prints. The second prints it again as player 1 saw it: the opponent's
hand is a count, their setup is face down, and what a search that doesn't reveal found is hidden.

```
Game 7: basic-box.txt (player 0, biased) vs dragapult.txt (player 1, biased)
As player 1 saw it

Turn 0, player 0's turn - waiting on player 0: choose who goes first
...
  > player 0: go second
  > player 0: make a face-down Pokemon the Active Pokemon
  > player 0: put a face-down Pokemon on the Bench
  > player 0: finish setting up
  > player 1: make Fezandipiti ex the Active Pokemon
  > player 1: finish setting up
```

A replay that no longer plays back, because the cards or the engine changed, stops with the action
that failed.

`--check-replays` tests the whole round trip at scale: the Milestone 4 sign-off runs 10,000 games with it.

It ends with the final board and how the game ended. Narration includes hidden cards: it's a
debugging view, not a player's view.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Every game finished |
| 1 | A game hit the action limit, an invariant broke, or loading data failed |
| 2 | Bad arguments (usage is printed) |
