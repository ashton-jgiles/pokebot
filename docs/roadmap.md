# Roadmap

The milestone plan from the project proposal, with the current status of each.
**Last reviewed:** 2026-11-16.

| # | Milestone | Status |
|---|---|---|
| M0 | Project setup | ✅ Done |
| M1 | Card data, decklist parser, deck validator | ✅ Done |
| M2 | Basic engine | ✅ Done |
| M3 | Effects system and every card in the four decks | ⬜ Next |
| M3b | Standard coverage: card-text translator and coverage report | ⬜ Not started |
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
| Card definition format (`CardDef`, JSON) | ✅ | [`src/cards/card`](src/cards/card/card.md) |
| Card database + effects-file merging | ✅ | [`src/cards/card_db`](src/cards/db/card_db.md) |
| Card importer (TCGdex download, cache, convert) | ✅ | [`tools/import`](tools/import/main.md) |
| All 80 cards (4 decks) imported and loading | ✅ | `pokebot-import` on all decks |
| Tera review of every Pokémon ex | ✅ | [`data/cards/effects/`](data/cards/README.md#recording-tera) |
| Deck validator, run by `pokebot-import` on every deck | ✅ | [`src/cards/deck_validator`](src/cards/validator/deck_validator.md) |

All four decks validate as legal, and deliberately illegal decks are rejected with every problem listed.

## M2 — Basic engine ✅

**Done when:** 100,000 random-vs-random games run with no crashes or broken invariants.

The core rules engine (`src/engine`): setup and mulligans, the turn loop, benching, evolving,
attaching Energy, retreating, attacks with Weakness and Resistance, Knock Outs, Prize cards (1 / 2 for ex /
3 for Mega ex), promoting a new Active, and every way to win, including both players winning at once.
Card *effects* are not included: attacks deal their printed damage only, and Trainers can't be played yet.

**Sign-off run (2026-11-16):** `pokebot-sim --games 100000 --agent biased` on all four decks, with the
invariant checker after every action (about 10 million actions): **no problems found, every game finished.**

| Measure | Result |
|---|---|
| How games ended (biased random agents) | Prize cards 38.7%, no Pokémon left 19.6%, deck-out 41.6%, draws 0 |
| Speed, release build, no checks | about 10,000 games/s, 1 million actions/s |
| Speed, release build, checking every action | about 4,100 games/s |
| Player who went first won | 31.6% of decided games (they can't attack on turn 1) |

Tools added: `pokebot-sim` (run many games, or `--print` one game move by move), random and biased random
agents, the invariant checker, and text descriptions of any state or action.

## M3 — Effects system ⬜ (next)

**Done when:** `pokebot-import` reports 0 attacks, abilities and cards still needing effects (88 today,
across all four decks), every card has a test, and the 100,000-game stress test still passes.

**Format (decided 2026-11-16): data + C++ hybrid.** Effects are written as data in
`data/cards/effects/<id>.json`, compiled into a flat list of steps, and run by an interpreter that can
pause for a player's choice and resume. Where a running effect is up to is stored in `GameState`, so
search can copy a half-finished Ultra Ball like anything else. A `custom` step calls registered C++
for the few one-of-a-kind cards. A translator from card text to effects is deferred until the format
has been proven on these 88.

The 88 texts come in five shapes:

| Shape | Examples | About |
|---|---|---|
| One-shot step programs | Ultra Ball, Boss's Orders, Lillie's Determination, Phantom Dive | 50 |
| Damage formulas | "30 per damage counter", "+120 if Knocked Out last turn", flip until tails | 14 |
| Lasting effects | "can't attack next turn", "can't retreat", Budew's Item lock | 9 |
| Passive modifiers | Air Balloon, N's Castle, Hero's Cape, Fairy Zone, Area Zero, Tera | 10 |
| Abilities and triggers | Run Errand, Teal Dance, Meowth ex on Bench, Risky Ruins, Ignition Energy | 16 |

| Step | Builds | Status |
|---|---|---|
| 3.1 | Effect format, loader and compiler, the step interpreter, new decisions (choose cards / Pokémon / yes-no), first building blocks run through attack effects. **Plus the foundations from the [Standard scan](research/standard-scan.md):** every rule cards can bend becomes a question with the rulebook's default answer, an event dispatcher, and game-history fields. *3.1a done: the [rules questions](src/engine/rules/rules.md) with rulebook defaults* | 🟨 |
| 3.2 | Trainer rules (Item, Supporter, Stadium, Tool, play conditions) and the simple Trainers | ⬜ |
| 3.3 | Special Conditions and the full Pokémon Checkup | ⬜ |
| 3.4 | Attack effects: damage formulas, damage to Benched Pokémon, damage counters, Tera bench protection | ⬜ |
| 3.5 | Lasting effects and game history (Knocked Out last turn) | ⬜ |
| 3.6 | Abilities (once per turn, triggers, passive modifiers), Tools and Stadiums in play | ⬜ |
| 3.7 | Special Energy and full Energy cost matching | ⬜ |
| 3.8 | One-of-a-kind cards in custom C++ (Night Joker, Transformation Tome, Rare Candy, Area Zero, …) | ⬜ |
| 3.9 | Sign-off: 0 missing, a test per card, test-only cards for unused mechanics, 100k-game stress test | ⬜ |

**Designed for all of Standard (2026-11-16).** A [scan of every Standard-legal card](research/standard-scan.md)
(TEF–DLR: 2,213 distinct cards, 1,455 distinct effect texts) found that 97.6% of texts use known, templated
mechanics. It also listed the 25 rules that cards change (HP, Retreat Cost, attack cost, Energy provided,
damage, prevention, Abilities, evolution, Prize cards, Bench size…), the events cards react to, and the game
history they check. M3 builds that whole vocabulary, so any Standard card can be expressed, but writes
effects only for our four decks. Covering the rest of Standard is M3b.

## M3b — Standard coverage ⬜

A **translator from card text to effects files**, using the vocabulary M3 builds, plus a coverage report:
how many Standard cards it converts, and exactly which texts still need new vocabulary or custom code.
Hand-writing all 1,455 texts isn't realistic; the scan shows the wording is patterned enough for this to
cover most of them. **Done when:** a target share of Standard (to be set once M3 is done) imports with
effects and passes generated tests, and the rest is listed by what it needs. Can run alongside M4–M5,
since the search work only needs our four decks.

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
