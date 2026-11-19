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
                       Agent::choose ◄── observe ◄──── play_game ──► replays, pokebot-sim, pokebot
                       (src/agents)    (src/engine)    (src/harness)  (src/harness, tools/sim, src/client)
```

Only `tools/import` talks to TCGdex. The engine only uses `CardDB` and `CardDef`. Agents never see
the game state: each decision they get an `Observation` of what their player could see, and the
legal actions.

## Index

| Area | Doc | Covers |
|---|---|---|
| Project | [roadmap.md](roadmap.md) | Milestones and current status |
| `src/core` | [decklist.md](src/core/decklist.md) | Decklist parser |
| `src/cards` | [card.md](src/cards/card/card.md) | `CardDef`, enums, card JSON |
| | [card_db.md](src/cards/db/card_db.md) | Card database, effects-file merging |
| | [program.md](src/cards/effects/program.md) | The effect language: building blocks, filters, steps |
| | [deck_validator.md](src/cards/validator/deck_validator.md) | Deck-building rules |
| `src/engine` | [README.md](src/engine/README.md) | **Start here for the engine:** how it works, decisions, turn phases |
| | [game_state.md](src/engine/game/game_state.md) | `GameState`, `CardTable`, Pokémon in play, decisions |
| | [action.md](src/engine/actions/action.md) | `Action` and every action kind |
| | [engine.md](src/engine/engine.md) | `new_game`, `legal_actions`, `apply`, shared helpers |
| | [setup.md](src/engine/setup/setup.md) | Opening hands, mulligans, setup decisions |
| | [turn.md](src/engine/actions/turn.md) | Turn flow, `continue_game`, Pokémon Checkup |
| | [main_actions.md](src/engine/actions/main_actions.md) | Bench, evolve, attach, retreat |
| | [combat.md](src/engine/actions/combat.md) | Attacks, damage, Knock Outs, Prize cards, winning |
| | [conditions.md](src/engine/actions/conditions.md) | Special Conditions and Pokémon Checkup |
| | [lasting.md](src/engine/actions/lasting.md) | Lasting effects ("during your next turn") |
| | [abilities.md](src/engine/actions/abilities.md) | Abilities you choose to use, Stadium actions |
| | [rules.md](src/engine/rules/rules.md) | The questions cards can change: HP, costs, damage, turn limits |
| | [events.md](src/engine/events/events.md) | Moments cards react to, the event hook |
| | [interpreter.md](src/engine/effects/interpreter.md) | Running effects and their choices |
| | [custom.md](src/engine/effects/custom.md) | One-of-a-kind cards (Rare Candy, Transformation Tome) and the Bench limit |
| | [knowledge.md](src/engine/knowledge/knowledge.md) | What each player knows: seen cards, revealed cards, shuffled card numbers |
| | [observation.md](src/engine/observation/observation.md) | `Observation`: the game as one player sees it |
| | [invariants.md](src/engine/invariants/invariants.md) | Consistency checker |
| | [describe.md](src/engine/describe/describe.md) | States, observations and actions as text |
| | [fixed_vector.md](src/engine/shared/fixed_vector.md), [rng.md](src/engine/shared/rng.md) | Fixed-capacity list, seeded RNG |
| `src/agents` | [agent.md](src/agents/agent.md) | The `Agent` interface |
| | [random_agent.md](src/agents/random_agent.md) | Random and biased random agents |
| | [human_agent.md](src/agents/human_agent.md) | A person typing their choices |
| | [world.md](src/agents/world.md) | `World`: a game state consistent with an observation, to try actions on |
| | [evaluation.md](src/agents/evaluation.md) | The evaluation function e: features, weights, utility |
| | [greedy_agent.md](src/agents/greedy_agent.md) | The greedy agent: one action ahead in a few worlds |
| `src/harness` | [match.md](src/harness/match.md) | `play_game`, how games end |
| | [replay.md](src/harness/replay.md) | Saving games and playing them back, as either player |
| | [arena.md](src/harness/arena.md) | The match harness: one agent against another, in parallel, with confidence intervals |
| `src/client` | [main.md](src/client/main.md) | The `pokebot` command: `play`, `watch`, `replay` |
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
