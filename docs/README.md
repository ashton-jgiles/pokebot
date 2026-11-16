# pokebot documentation

These docs mirror the repository: the documentation for `src/cards/card.cpp` lives at
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
                                                                (src/cards)      (used by the engine)
```

Only the left half (`tools/import`) talks to TCGdex. The engine, coming in Milestone 2, only
uses `CardDB` and `CardDef`.

## Index

| Area | Doc | Covers |
|---|---|---|
| Project | [roadmap.md](roadmap.md) | Milestones and current status |
| `src/core` | [decklist.md](src/core/decklist.md) | Decklist parser |
| `src/cards` | [card.md](src/cards/card.md) | `CardDef`, enums, card JSON |
| | [card_db.md](src/cards/card_db.md) | Card database, effects-file merging |
| `src/client` | [main.md](src/client/main.md) | Command-line client (placeholder) |
| `tools/import` | [main.md](tools/import/main.md) | The `pokebot-import` command |
| | [tcgdex.md](tools/import/tcgdex.md) | TCGdex lookup, cache, retries |
| | [convert.md](tools/import/convert.md) | TCGdex → our card format |
| | [http_client.md](tools/import/http_client.md) | HTTP requests via curl |
| `tests` | [README.md](tests/README.md) | Test suite and helpers |
| `data/cards` | [README.md](data/cards/README.md) | `sets.json`, card JSON format, effects files |
| `data/decks` | [README.md](data/decks/README.md) | Decklists |

`src/engine`, `src/agents` and `src/harness` are still empty. Their docs will appear here as
those milestones start.

## Libraries and executables

| Target | Kind | Contents |
|---|---|---|
| `pokebot_lib` | static library | `src/core`, `src/cards` (later: engine, agents, harness) |
| `pokebot_import_lib` | static library | `tools/import` except `main.cpp`, so tests can use it |
| `pokebot` | executable | `src/client/main.cpp` |
| `pokebot-import` | executable | `tools/import/main.cpp` |
| `pokebot_tests` | executable | `tests/*.cpp` |

## Building, testing, formatting

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The code style is defined in [`.clang-format`](../.clang-format): 4-space indents, indented
namespaces, braces on every `if` and loop. Format a file in VS Code with **Shift+Alt+F**.
