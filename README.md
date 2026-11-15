# pokebot

A search-based AI for the Pokémon Trading Card Game, running in its own simulator.

The plan has a turn planner (or-tree and and-tree search) for decisions within a turn,
Information-Set MCTS for looking several turns ahead, and an expectiminimax solver for endgames.

## Layout

| Folder | Contents |
|---|---|
| `src/core` | Shared types and utilities |
| `src/engine` | Rules engine: game state, legal actions, effects |
| `src/cards` | Card-specific effect code |
| `src/agents` | AI agents (random, greedy, planner, MCTS…) |
| `src/harness` | Match runner and statistics |
| `src/client` | Command-line client |
| `tools` | Decklist parser, deck validator, data tools |
| `data/cards` | Card definitions (JSON) |
| `data/decks` | Decklists |
| `tests` | Catch2 test suite |
| `docs` | Design documents |

## Building

Requires CMake 3.24+ and a C++20 compiler. The default presets use MinGW g++.
The test framework (Catch2) is downloaded automatically on the first configure.

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Other presets: `release` (optimized MinGW build) and `msvc` (Visual Studio 2026).
