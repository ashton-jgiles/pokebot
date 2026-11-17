# pokebot documentation

These docs mirror the repository: the documentation for `src/cards/card/card.cpp` lives at
`docs/src/cards/card.md`, and so on. Start with the [roadmap](roadmap.md) for where the project stands.

## How the pieces fit today

```
data/decks/*.txt ──► decklist parser ──► TcgdexClient ──► convert_card ──► data/cards/generated/*.json
                     (src/core)          (tools/import)   (tools/import)            │
                                              │                                      ▼
                                         data/cards/raw/            data/cards/effects/*.json
                                         (download cache)                    │
                                                                             ▼
                                                                CardDB::load ──► CardDef
                                                                (src/cards)         │
                                                                                    ▼
                          deck validator ──► CardTable ──► new_game / legal_actions / apply
                          (src/cards)        (src/engine)              │
                                                                       ▼
                                             Agent::choose ◄──── play_game ──► pokebot-sim
                                             (src/agents)        (src/harness)  (tools/sim)
```

Only `tools/import` talks to TCGdex. The engine only uses `CardDB` and `CardDef`, and agents only
see the engine through `legal_actions` and `apply`.

## Index

| Area | Doc | Covers |
|---|---|---|
| Project | [roadmap.md](roadmap.md) | Milestones and current status |
| `src/core` | [decklist.md](src/core/decklist.md) | Decklist parser |
| `src/cards` | [card.md](src/cards/card/card.md) | `CardDef`, enums, card JSON |
| | [card_db.md](src/cards/db/card_db.md) | Card database, effects-file merging |
| | [deck_validator.md](src/cards/validator/deck_validator.md) | Deck-building rules |
| `src/engine` | [README.md](src/engine/README.md) | **Start here for the engine:** how it works, decisions, turn phases |
| | [game_state.md](src/engine/game/game_state.md) | `GameState`, `CardTable`, Pokémon in play, decisions |
| | [action.md](src/engine/actions/action.md) | `Action` and every action kind |
| | [engine.md](src/engine/engine.md) | `new_game`, `legal_actions`, `apply`, shared helpers |
| | [setup.md](src/engine/setup/setup.md) | Opening hands, mulligans, setup decisions |
| | [turn.md](src/engine/actions/turn.md) | Turn flow, `continue_game`, Pokémon Checkup |
| | [main_actions.md](src/engine/actions/main_actions.md) | Bench, evolve, attach, retreat |
| | [combat.md](src/engine/actions/combat.md) | Attacks, damage, Knock Outs, Prize cards, winning |
| | [invariants.md](src/engine/invariants/invariants.md) | Consistency checker |
| | [describe.md](src/engine/describe/describe.md) | States and actions as text |
| | [fixed_vector.md](src/engine/shared/fixed_vector.md), [rng.md](src/engine/shared/rng.md) | Fixed-capacity list, seeded RNG |
| `src/agents` | [agent.md](src/agents/agent.md) | The `Agent` interface |
| | [random_agent.md](src/agents/random_agent.md) | Random and biased random agents |
| `src/harness` | [match.md](src/harness/match.md) | `play_game`, how games end |
| `src/client` | [main.md](src/client/main.md) | Command-line client (placeholder) |
| `tools/import` | [main.md](tools/import/main.md) | The `pokebot-import` command |
| | [tcgdex.md](tools/import/tcgdex.md) | TCGdex lookup, cache, retries |
| | [convert.md](tools/import/convert.md) | TCGdex → our card format |
| | [http_client.md](tools/import/http_client.md) | HTTP requests via curl |
| `tools/sim` | [main.md](tools/sim/main.md) | The `pokebot-sim` command: stress tests, speed, narrated games |
| `tests` | [README.md](tests/README.md) | Test suite and helpers |
| `data/cards` | [README.md](data/cards/README.md) | `sets.json`, card JSON format, effects files |
| `data/decks` | [README.md](data/decks/README.md) | Decklists |

## Libraries and executables

| Target | Kind | Contents |
|---|---|---|
| `pokebot_lib` | static library | `src/core`, `src/cards`, `src/engine`, `src/agents`, `src/harness` |
| `pokebot_import_lib` | static library | `tools/import` except `main.cpp`, so tests can use it |
| `pokebot` | executable | `src/client/main.cpp` |
| `pokebot-import` | executable | `tools/import/main.cpp` |
| `pokebot-sim` | executable | `tools/sim/main.cpp` |
| `pokebot_tests` | executable | `tests/*.cpp` |

## Building, testing, formatting

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The code style is defined in [`.clang-format`](../.clang-format): 4-space indents, indented
namespaces, braces on every `if` and loop. Format a file in VS Code with **Shift+Alt+F**.
