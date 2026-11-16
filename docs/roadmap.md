# Roadmap

The milestone plan from the project proposal, with the current status of each.
**Last reviewed:** 2026-11-16.

| # | Milestone | Status |
|---|---|---|
| M0 | Project setup | ✅ Done |
| M1 | Card data, decklist parser, deck validator | ✅ Done |
| M2 | Basic engine | ⬜ Next |
| M3 | Effects system and all cards in both decks | ⬜ Not started |
| M4 | Observation layer, command-line client, replays | ⬜ Not started |
| M5 | Greedy agent, match harness, profiling | ⬜ Not started |
| M6 | Model 1: turn planner (or-tree and and-tree modes) | ⬜ Not started |
| M7 | Determinization and flat Monte Carlo | ⬜ Not started |
| M8 | Model 2: IS-MCTS | ⬜ Not started |
| M9 | Model 3: endgame solver (expectiminimax) | ⬜ Not started |
| M10 | Learned evaluation function | ⬜ Not started |
| M11+ | Stretch: policy prior, deck-building GA, more decks, opponent inference | ⬜ Not started |

Each milestone from M5 on produces a new agent that must beat the previous one:
Random → Greedy → Planner → Flat MC → IS-MCTS → IS-MCTS + endgame solver → learned evaluation.

## M0 — Project setup ✅

**Done when:** the build and an empty test suite pass.

- CMake + C++20, presets for MinGW g++ (`debug`, `release`) and Visual Studio (`msvc`).
- Catch2 test suite; nlohmann/json for JSON.
- MinGW builds link the C++ runtime statically, so executables don't pick up a wrong `libstdc++` DLL from PATH.
- `.clang-format` encodes the project style. Both decklists chosen.

## M1 — Card data and decks ✅

**Done when:** both decks load and validate, and an illegal deck is rejected.

| Piece | Status | Where |
|---|---|---|
| Decklist parser | ✅ | [`src/core/decklist`](src/core/decklist.md) |
| Card definition format (`CardDef`, JSON) | ✅ | [`src/cards/card`](src/cards/card.md) |
| Card database + effects-file merging | ✅ | [`src/cards/card_db`](src/cards/card_db.md) |
| Card importer (TCGdex download, cache, convert) | ✅ | [`tools/import`](tools/import/main.md) |
| All 80 cards (4 decks) imported and loading | ✅ | `pokebot-import` on all decks |
| Tera review of every Pokémon ex | ✅ | [`data/cards/effects/`](data/cards/README.md#recording-tera) |
| Deck validator, run by `pokebot-import` on every deck | ✅ | [`src/cards/deck_validator`](src/cards/deck_validator.md) |

All four decks validate as legal, and deliberately illegal decks are rejected with every problem listed.

## M2 — Basic engine ⬜

Setup and mulligans, turn structure, benching, Energy attachment, evolution, retreat, attacks
that only deal damage, Knock Outs and Prize cards, and win conditions.
**Done when:** 100,000 random-vs-random games run with no crashes or broken invariants.

## M3 — Effects system ⬜

The pending-decision stack, the effect format and its building blocks, Trainers, Abilities,
Special Conditions and Pokémon Checkup. Every card in both decks gets an effects file, plus test
cards for mechanics the decks don't use (each Special Condition, Ability lock, healing, Special
Energy). **Done when:** per-card tests and the 100k-game stress test pass. The current to-do
list is printed by `pokebot-import` (42 attacks, abilities and cards).

## M4 onward

The full definitions of M4–M11 (what each builds and its "done when" test) are in the project
proposal. The table at the top lists them in order.

## Known gaps and notes

- **TCGdex has no rule-box data.** Tera can't be detected automatically. Each non-Mega Pokémon ex
  is checked by hand and recorded in its effects file; the importer reminds you about any that
  aren't recorded yet (see [Recording Tera](data/cards/README.md#recording-tera)).
- **Ancient / Future labels aren't tracked yet** (Raging Bolt ex is Ancient, Iron Leaves ex is
  Future). Some cards care about them, so they'll need a field once those cards' effects are written.
- **No test for `CurlProcessClient`**, by design: it needs the network. Everything that uses it is
  tested through a fake client.
- **`CardDB::load` fails if `data/cards/generated/` doesn't exist.** Run `pokebot-import` first
  after cloning.
