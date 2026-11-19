# pokebot

A search-based AI for the Pokémon Trading Card Game, running in its own simulator.

The plan has a turn planner (or-tree and and-tree search) for decisions within a turn,
Information-Set MCTS for looking several turns ahead, and an expectiminimax solver for endgames.

## Layout

| Folder | Contents |
|---|---|
| `src/core` | Decklist parser |
| `src/cards` | Card definitions, card database, deck validator |
| `src/engine` | Rules engine: game state, legal actions, applying actions |
| `src/agents` | AI agents (random so far; planner, MCTS… later) and the human player |
| `src/harness` | Playing games between agents, replays |
| `src/client` | `pokebot`: play against an agent, watch two agents, play back a replay |
| `tools/import` | `pokebot-import`: downloads and converts card data |
| `tools/sim` | `pokebot-sim`: plays many games, or narrates one |
| `data/cards` | Card definitions (JSON) |
| `data/decks` | Decklists |
| `tests` | Catch2 test suite |
| `docs` | Documentation for every file, mirroring this layout; start at `docs/README.md` |

## Building

Requires CMake 3.24+ and a C++20 compiler. The default presets use MinGW g++.
The test framework (Catch2) is downloaded automatically on the first configure.

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Other presets: `release` (optimized MinGW build) and `msvc` (Visual Studio 2026).

## Running

From the repository root. Import the cards for every deck first (needs network once, then cached):

```bash
./build/debug/tools/import/pokebot-import.exe data/decks/*.txt
```

Play 1,000 games between random agents and report how they went, or narrate one game:

```bash
./build/debug/tools/sim/pokebot-sim.exe --games 1000
./build/debug/tools/sim/pokebot-sim.exe --print --seed 3
```

Play against the biased agent, seeing only what your player can see, then watch the game again from
either side:

```bash
./build/debug/src/pokebot.exe play --deck dragapult --against basic-box --save-replay replays/mine.json
./build/debug/src/pokebot.exe replay replays/mine.json --as 1
./build/debug/src/pokebot.exe watch --seed 7 --step
```

Agents never see the game itself, only an observation of what their player could see at the table;
see [observation](docs/src/engine/observation/observation.md).
