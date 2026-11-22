# `tools/sim/main.cpp` — the `pokebot-sim` command

**File:** [`tools/sim/main.cpp`](../../../tools/sim/main.cpp) · **Executable:** `pokebot-sim`

Plays many games on the real decks: one kind of agent against itself, to stress-test the engine
and measure its speed; one agent against another (`--agents A,B`), to measure win rates through
the [match harness](../../src/harness/arena.md); or one game narrated move by move. It also saves
and prints [replays](../../src/harness/replay.md).

## Usage

Run from the repository root. It reads `data/cards/generated`, `data/cards/effects` and every
`data/decks/*.txt`, so run `pokebot-import` first.

```bash
./build/debug/tools/sim/pokebot-sim.exe [--games N] [--agent random|biased|greedy|planner] [--seed S] [--no-check]
                                        [--print [--save-replay FILE]] [--replay FILE] [--as 0|1|all]
                                        [--agents A,B [--threads N] [--save-replays K]]
                                        [--planner-samples M] [--planner-or-budget N] [--planner-and-budget N]
                                        [--planner-lab N [--every K]]
```

| Option | Default | Meaning |
|---|---|---|
| `--games N` | 1000 | Games to play |
| `--agent A` | `biased` | `random`, `biased` (see [random agents](../../src/agents/random/random_agent.md)) `greedy` ([greedy agent](../../src/agents/greedy/greedy_agent.md)) or `planner` ([planner](../../src/agents/planner/planner_agent.md)); both players get the same kind |
| `--seed S` | 1 | Game `i` uses seed `S + i`; agents are seeded from it too |
| `--no-check` | | Skip the invariant check after every action (about 2.5× faster) |
| `--print` | | Play **one** game with seed `S` and print every action |
| `--save-replay FILE` | | With `--print`: also save that game as a replay |
| `--check-replays` | | Save every game as replay text, load it and play it back, checking it ends in exactly the same state. A game that doesn't is saved to `replays/mismatch-seed-S.json`, and the run exits with 1 |
| `--replay FILE` | | Print a saved replay instead of playing |
| `--as 0\|1\|all` | `all` | With `--print` or `--replay`: show only what that player saw |
| `--agents A,B` | | Play agent `A` against agent `B` instead (see [below](#one-agent-against-another)). `--games` is then the number of games **per deck pairing** |
| `--threads N` | one per hardware thread | With `--agents`: games played at once |
| `--save-replays K` | | With `--agents`: save every `K`th game to `replays/arena/` |
| `--planner-samples M` | 1 | The [planner](../../src/agents/planner/planner_agent.md)'s sampled worlds for a plan that ends in chance or hidden cards |
| `--planner-or-budget N` | 200 | The planner's node budget for its win search (the or-tree) |
| `--planner-and-budget N` | 300 | The planner's node budget for its best-plan search (the and-tree) |
| `--planner-lab N` | | Play `N` Greedy-against-Greedy games and measure the planner's searches at one in every `K` main-phase decisions ([planner lab](../../src/harness/planner_lab.md)). Uses `--seed`, `--planner-samples` and `--planner-or-budget`, and prints its progress after each game to stderr |
| `--every K` | 5 | With `--planner-lab`: which main-phase decisions to measure |

**Pairings:** decks are sorted by file name. Game `i` plays deck `i mod n` against deck
`(i / n) mod n`, so every pairing, including mirrors, comes up equally often.

## One agent against another

`--agents A,B` runs [`run_arena`](../../src/harness/arena.md): `A` against `B` on all 16 deck
pairings (A's deck, B's deck), each seed played once with `A` in each seat, on several threads.
`--games N` is per pairing, so `--games 1000` plays 16,000 games. `--no-check` makes it faster, as
for the stress test. It can't be combined with `--print` or `--check-replays`.

```
./build/release/tools/sim/pokebot-sim.exe --agents greedy,random --games 1000 --no-check
```

It prints the speed, A's score (wins, plus half the draws, per game) with its 95% confidence
interval, the same split by who went first and by deck pairing, how the games ended, and how A won
and lost: by Prize cards, by the loser having no Pokémon left, or by the loser's deck running out
(a loss by deck-out is A's own deck running out). With a planner on either side, the header also gives
its settings. For an agent that measures itself (the
[planner](../../src/agents/planner/planner_agent.md)) it then prints its counters per game. While it plays, it
prints a progress line about every 5% of the games to the error stream, with the time so far and an
estimate of the time left. Results
don't depend on `--threads`. With the same agent on both sides (`--agents random,random`) they're
labelled `random-A` and `random-B`. Replays saved with `--save-replays` are named
`NNNNN-seed-S-<player 0's agent>-<deck>-vs-<player 1's agent>-<deck>.json`; print one with `--replay`.

**Match speed** (Milestone 5 sign-off, release build, `--agents A,B --games 1000 --no-check`, 16,000
games; the reference machine has 16 hardware threads):

| Match | 16 threads | 1 thread (`--threads 1`, `--games 100`) |
|---|---|---|
| `random,random` | about 21,700 games/s, 4.5 million actions/s | |
| `greedy,random` after step 5.6b (Greedy scores each action as it stands) | about 2,810 games/s | |
| `greedy,random` at sign-off (Greedy also plays out its follow-up choices) | about 856 games/s; 840 with invariant checks | about 92 games/s |
| `greedy,biased` at sign-off | about 892 games/s | |
| `planner,greedy` at the Milestone 6 sign-off (1 sampled world, budgets 200 and 300) | about 33.9 games/s; 32.5 with invariant checks | about 3.5 games/s (`--games 10`) |

Greedy builds 3 worlds per decision and tries every action in each, so it costs about 9 ms per game
on one thread against Random. Playing out follow-up choices (step 5.6c) made it about 3.3 times
slower.

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
| Milestone 5 sign-off (`Observation` carries the engine's stack) | about 2,880 games/s, 0.4 million actions/s: no measurable cost | |

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
