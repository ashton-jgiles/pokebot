# Roadmap

The milestone plan from the project proposal, with the current status of each.
**Last reviewed:** 2026-11-18.

| # | Milestone | Status |
|---|---|---|
| M0 | Project setup | ✅ Done |
| M1 | Card data, decklist parser, deck validator | ✅ Done |
| M2 | Basic engine | ✅ Done |
| M3 | Effects system and every card in the four decks | ✅ Done |
| M3b | Standard coverage: card-text translator and coverage report | ⬜ Not started |
| M4 | Observation layer, command-line client, replays | ✅ Done |
| M5 | Greedy agent, match harness, profiling | 🔄 In progress |
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

## M3 — Effects system ✅

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
| 3.1 | **Engine foundations** from the [Standard scan](research/standard-scan.md): every rule cards can bend is a [question](src/engine/rules/rules.md) with the rulebook's default answer; Pokémon ids, `AttackRef` and `EffectContext` (effects run for "this Pokémon", which makes copy attacks work); attacks per turn with an `ExtraAttack` decision; [events](src/engine/events/events.md) at every moment cards react to; three turns of game history | ✅ |
| 3.2 | **Effect language:** the effects-file format, loader and compiler, the resumable step interpreter (its state stored in `GameState`), new decisions (choose cards / Pokémon / attack / yes-no), and the first building blocks, proven on simple attack effects. *Done: [effect language](src/cards/effects/program.md), [interpreter](src/engine/effects/interpreter.md), 7 attacks from our decks* | ✅ |
| 3.3 | Trainer rules (Item, Supporter, Stadium, Tool, play conditions) and the simple Trainers. *Done: the rules, `"requires"`, `if`, new building blocks and filters, 16 Trainers from our decks* | ✅ |
| 3.4 | Special Conditions and the full Pokémon Checkup. *Done: all five [conditions](src/engine/actions/conditions.md), the Checkup, Absolute Snow and Mind Bend* | ✅ |
| 3.5 | Attack effects: damage formulas, damage to Benched Pokémon, damage counters, Tera bench protection, copy attacks. *Done: damage formulas, flips and counts (3.5a); targets, Bench damage, damage counters, Tera protection, Tuck Tail, copy attacks and Crushing Hammer (3.5b)* | ✅ |
| 3.6 | Lasting effects ("during your opponent's next turn") and the rest of the game history. *Done: five [lasting effects](src/engine/actions/lasting.md), Knock Outs by attacks in history, 8 cards* | ✅ |
| 3.7 | Abilities (once per turn, triggers on events, passive modifiers that change the rule questions), Tools and Stadiums in play, Festival Lead. *3.7a done: always-on effects (Tools, N's Castle, Skyliner, Fairy Zone) and Shred. 3.7b done: Abilities you use (9) and Surfing Beach. 3.7c done: triggered effects (3 Abilities, Risky Ruins). Festival Lead isn't in our decks; the extra-attack flow is ready for it. Area Zero Underdepths moves to 3.9; Psyduck's Damp waits until a Standard card needs it* | ✅ |
| 3.8 | Special Energy and full Energy cost matching. *Done: Energy units, exact cost matching, Retreat paid in Energy, Neo Upper and Ignition Energy (3.8a); attaching and moving Energy, 6 Trainers (3.8b)* | ✅ |
| 3.9 | One-of-a-kind cards in custom C++ (Night Joker, Transformation Tome, Rare Candy, Area Zero, …). *Done: [custom effects](src/engine/effects/custom.md) for Rare Candy and Transformation Tome, Area Zero Underdepths as a `bench_size` passive, the 8-space Bench and forced discards down to the limit. Night Joker was done in 3.5b. Only Psyduck's Damp is left unwritten* | ✅ |
| 3.10 | Sign-off: 0 missing, a test per card, test-only cards for unused mechanics, 100k-game stress test, and profiling the engine (speed fell from 1.0 to 0.5 million actions/s with effects; a game is now 2,624 bytes, up from 1,920). *Done: 0 missing (Damp marked `no_effect`), a [coverage test](tests/README.md#every-card-is-tested) that uses every chosen effect in random games, the unused mechanics listed with their tests, and `apply_legal` plus fewer allocations for speed (0.5 → 0.8 million actions/s). 100,000 games with checks: no problems* | ✅ |

**Sign-off run (2026-11-18):** `pokebot-sim --games 100000 --agent biased` on all four decks, with the
invariant checker after every action (about 15 million actions): **no problems found, every game finished.**
`pokebot-import` reports 0 attacks, abilities and cards still needing effects, and 218 tests pass.

| Measure | Result |
|---|---|
| How games ended (biased random agents) | Prize cards 31.1%, no Pokémon left 18.3%, deck-out 50.6%, draws 43 (0.04%) |
| Speed, release build, no checks | about 5,480 games/s, 0.8 million actions/s (151.7 actions a game, up from about 100 in M2) |
| Speed, release build, checking every action | about 1,920 games/s |
| Player who went first won | 51.0% of decided games (Supporters and Abilities make up for not attacking on turn 1) |

Known approximations, each documented where it lives: Adrena-Brain always moves as many damage counters
as it can; Crispin doesn't check that its two Energy are different types; damage formulas count Energy
cards, not units; Psyduck's Damp does nothing with our cards; Chien-Pao's Icicle Loop can't be paid for
in `basic-box`, so it's only tested with a made-up card.

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

## M4 — Observation layer, client, replays ✅

**Done when:** agents only see what their player could see at the table (tests prove two games that
differ only in hidden cards look identical), 10,000 simulated games round-trip through replays
exactly, and a full game against the random agent can be played in the terminal.

**Opponent decks (decided 2026-11-18):** agents never get the opponent's decklist. They see the cards
the opponent has shown and, from M7, guess the list from a library of known archetype lists, the
way a player recognises Dragapult from a Dreepy and then wonders whether it's the Crushing Hammer or
the Blaziken ex version. M4 makes sure the list can't leak and records what each player has seen.

| Step | Builds | Status |
|---|---|---|
| 4.1 | **Who knows what:** each player's `located` and `seen` cards, revealed searches (`"reveal": true`), looking at the top of the deck, mulligans showing the hand, face-down setup, shuffles forgetting, and card numbers shuffled per game. See [knowledge](src/engine/knowledge/knowledge.md) | ✅ |
| 4.2 | **`Observation`:** what one player can see, with card definitions only for cards they've seen; their own deck and Prize cards as one unordered list; the opponent's seen cards; `describe(observation)`; identical observations for games that differ only in hidden cards. See [observation](src/engine/observation/observation.md) | ✅ |
| 4.3 | **Agents see observations:** `Agent::choose(const Observation&, actions)`, with no `GameState` or `CardTable`; `ask` builds the deciding player's view; every action offered names only cards that player knows (a deck search now shows its owner the whole deck). See [agent](src/agents/agent.md) | ✅ |
| 4.4 | **Replays:** decks, seed and actions as JSON, replayed exactly; viewed as either player or with everything shown; `--save-replay`, and a saved replay whenever an invariant breaks. See [replay](src/harness/replay.md) | ✅ |
| 4.5 | **Command-line client:** `pokebot play` (you against an agent, seeing only your view), `watch` and `replay`; `HumanAgent`; actions described from an observation; the opponent's Pokémon named as theirs. See [client](src/client/main.md) | ✅ |
| 4.6 | Sign-off: 100,000 games with checks, 10,000 games through replays (`pokebot-sim --check-replays`), and the known approximations listed. *Done: no problems in either run, 263 tests pass* | ✅ |

**Sign-off run (2026-11-18):** `pokebot-sim --games 100000 --agent biased --seed 1` on all four decks,
with the invariant checker after every action (about 15 million actions): **no problems found, every
game finished.** Then `--games 10000 --check-replays`: **every game was saved, loaded and played back
to exactly the same final state.** 263 tests pass, and a full game against an agent was played by hand
with `pokebot play`.

| Measure | Result |
|---|---|
| How games ended (biased random agents) | Prize cards 31.1%, no Pokémon left 18.2%, deck-out 50.7%, draws 41 (0.04%) |
| Speed, release build, no checks | about 2,860 games/s, 0.4 million actions/s (151.7 actions a game) |
| Speed, release build, checking every action | about 1,420 games/s |
| Speed, release build, checking every replay | about 1,090 games/s |
| Player who went first won | 50.9% of decided games |

The games end the same way as at the M3 sign-off: hiding cards from the agents changes nothing for
random agents, as it shouldn't. The cost is speed, from building an observation at every decision.

Known approximations, each documented where it lives:
- **A revealed card is named only once it's revealed.** When the opponent picks cards in a search that
  reveals them (Cyrano), the picks show as "an unseen card" until the search ends and the cards are
  shown. The opponent's hand then names them, so nothing is hidden that should be shown for long.
- **Building an observation costs about as much as an action** (about 1.2 µs), and recording who knows
  what after every action is a sweep of the visible cards. With the random agents the simulator runs at
  about 2,860 games/s, down from 5,480 at the end of M3. Search agents will spend far longer per
  decision, so this was left as it is; recording cards as they move would be faster.
- **Replays depend on the card data and the engine.** A replay made before either changes may no longer
  play back; it then stops with the action that failed rather than showing a different game.
- **The client has had only light play-testing.** Its prompts and descriptions are tested with typed
  input, and a full game has been played by hand.

## M5 — Greedy agent, match harness, profiling 🔄

**Done when:** Greedy clearly beats Random over at least 1,000 seeded games per deck pairing in both
seat orders, and the simulator's throughput is recorded.

**How Greedy judges actions (decided 2026-11-18):** agents get only an `Observation`, so Greedy builds
a game state consistent with it, tries each action there and scores the result with the evaluation
function `e`. Its own deck and Prize cards are dealt from `deck_or_prizes` (their contents are known
exactly); the opponent's unseen cards are an inert placeholder until M7 deals them from a guessed
decklist. Actions that use no chance and reveal nothing come out the same whatever the hidden cards
are; the rest are averaged over a few sampled states. `e` reads only what both players can see, so
it can't use hidden cards either, and gives the same value in a world as in the real game. The turn
planner (M6) needs the same states.

| Step | Builds | Status |
|---|---|---|
| 5.1 | **The engine's stack in `Observation`:** every pending decision, and the effects resolving with their chosen cards hidden from a viewer who can't place them. See [observation](src/engine/observation/observation.md) | ✅ |
| 5.2 | **Consistent states:** a [`World`](src/agents/world.md), a `GameState` built from an observation that observes back to the same observation, with the viewer's own hidden cards dealt from their known contents and the opponent's unseen cards a placeholder | ✅ |
| 5.3 | **The evaluation function** `e` (features φ1–φ8 as 10 features, hand-set weights), zero-sum and reading only what both players can see. See [evaluation](src/agents/evaluation.md) | 🔄 |
| 5.4 | **Greedy agent:** one action ahead, averaged over a few sampled states | ⬜ |
| 5.5 | **Match harness:** seeded games in parallel, seats swapped, win rates with 95% confidence intervals per deck pairing, replays kept | ⬜ |
| 5.6 | Sign-off: Greedy against Random on every deck pairing, and profiling | ⬜ |

## M5 onward

The full definitions of M5–M11 (what each builds and its "done when" test) are in the project
proposal. The table at the top lists them in order. Two changes since:

- **M7 starts with an opponent deck model:** a library of archetype lists (`data/archetypes/`), a
  belief over them updated from the cards seen (Bayes' rule, with a small chance for cards no list
  explains), and dealing the opponent's unseen cards from a list drawn from that belief. Lists can
  only use cards the engine has effects for, so M3b widens what they can hold.
- **M11's opponent inference** becomes the learned, stronger version of that model.

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
