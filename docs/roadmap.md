# Roadmap

The milestone plan from the project proposal, with the current status of each.
**Last reviewed:** 2026-11-22.

| # | Milestone | Status |
|---|---|---|
| M0 | Project setup | ✅ Done |
| M1 | Card data, decklist parser, deck validator | ✅ Done |
| M2 | Basic engine | ✅ Done |
| M3 | Effects system and every card in the four decks | ✅ Done |
| M3b | Standard coverage: card-text translator and coverage report | 🔄 In progress |
| M4 | Observation layer, command-line client, replays | ✅ Done |
| M5 | Greedy agent, match harness, profiling | ✅ Done |
| M6 | Model 1: turn planner (or-tree and and-tree modes) | ✅ Done |
| M7 | Determinization and flat Monte Carlo | ✅ Done |
| M8 | Model 2: IS-MCTS | ✅ Done |
| M9 | Model 3: endgame solver (expectiminimax) | ✅ Closed: the solver ties IS-MCTS (see the sign-off) |
| M10 | Learned evaluation function | ✅ Done |
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

## M3b — Standard coverage 🔄

A **translator from card text to effects**, in the importer, plus a coverage report: how many Standard cards have all
their effects, and exactly what the rest need. Hand-written effects files override what it produces.

**Done when (2026-11-25, before any of it was built):**

- at least 60% of the distinct cards that TCGdex marks Standard-legal, as [the coverage report](tools/import/standard.md) counts them (2,097 on
  2026-11-25, so 1,259), have all their effects. The 60% was set against the prototype's 2,109;
- four new decks, chosen by the end of 3b.2, are fully playable, with hand-written effects for what the translator misses;
- a person checks a random 200 translated texts: at most 4 wrong. Each wrong one's rule is fixed, and a fresh sample must pass;
- 100,000 random games with the invariant checks find no problems, and the old decks are at most 10% slower.

**What the measurements showed (2026-11-25):** every card was downloaded again (TEF to DLR and both promo sets), and a
prototype phrase grammar in Python, kept out of the repo, sorted every text:

| | Cards, of 2,109 | |
|---|---|---|
| No text, or everything expressible with today's vocabulary | 846 | 40.1% |
| Every text parsed, if all the vocabulary it needs is built | 1,540 | 73.0% |

- **Both are estimates.** In a random 60 of the texts marked expressible about 1 in 10 wasn't, and the rules for passives match
  loosely, so 73% is an upper estimate for this grammar. Against that, it parses only 57 of our own decks' 85 texts, nearly
  all of which have working effects, so a better grammar finds more.
- **The 97.6% of the 2026-11-16 scan isn't a share of translatable texts.** It counts texts that mention a known mechanic. The
  1,455 texts have 1,374 different sentence templates, 230 of them used more than once, so whole sentences can't be matched.
- **Trainers lag:** 129 of 244 parse (all 2,213 cards counted).
- **Delta Reign is left out of the 2,109.** TCGdex doesn't mark it legal and its data is incomplete: every Pokémon ex there is a
  Basic (Mega Golisopod ex, for one, evolves from Wimpod), and 16 of its Trainers and Energy have no text. It's reported apart.
- **TCGdex has wrong data, and the importer copied it.** Six Pokémon ex of 30th Celebration are stored as Basic but are evolutions
  (checked on Limitless). **One is in mega-greninja: Greninja ex (30C 21), which is a Stage 2,** so every earlier run let that
  deck play it as a 300-HP Basic. It's one card of 60. What it changed isn't measured, except that both sides of a
  matchup had it. Other importer problems: 10 cards of Delta Reign write `EX` and were read as not ex, 53 write Weakness as `x2`, 8 Energy are named
  "Basic …", and 53 evolutions don't say what they evolve from.
- **Labels TCGdex doesn't have:** Tera (159 non-Mega ex to check), Ancient and Future.

**The design:** the translator must use every word of a text or it produces nothing, so a misread fails loudly. Its effects go into
`generated/`, marked as translated, which git ignores. It must compile to the same program as our hand-written effects wherever it
translates one. After the Greninja fix, the old decks mustn't change: each step repeats one 320-game IS-MCTS run exactly.

| Step | Builds | Status |
|---|---|---|
| 3b.1 | **Data:** corrections to TCGdex's data, the importer's fixes, whole sets imported, reprints matched, the coverage report, the scan document restored and corrected. *[Corrections](data/cards/README.md#correctionsjson) to TCGdex's data and the importer's fixes are built. All 460 tests pass, the re-import changes only Greninja ex (30C 21), and M10's result stands after the fix: 57.8% (56.6% to 59.0%). The bulk import and the [coverage report](tools/import/standard.md) are built: all 466 tests pass, and 373 of the 2,097 distinct cards (17.8%) have all their effects. The [scan document](research/standard-scan.md) is restored and corrected. Reprints in decklists move to 3b.2* | ✅ |
| 3b.2a | **Any deck export imports:** set codes found through TCGdex's series, reprints using their card's effects (`same_as`), and a report for each deck. Decks that aren't playable yet wait in `data/decks/incoming/`, which the sim and the tests don't read. *Done: all 470 tests pass, the four new decks import with 0 failures, 6 printings use a reprint's effects, and the reference run repeats. Results below* | ✅ |
| 3b.2 | **Translator** for today's vocabulary, checked against the hand-written effects. *Done: the four decks are chosen, and the translator reads 81 of the 88 hand-written texts the same, completes 471 Standard cards with no effects file (844 complete in all, 40.2%), and passed its audit of 50. The design and the substeps are below* | ✅ |
| 3b.3 | Damage, healing, counts and conditions. *Done: 1,047 of the 2,097 cards complete (49.9%, from 40.2%), the focused audit of 50 found none wrong, and the reference run repeats. The design and the substeps are below* | ✅ |
| 3b.4 | Lasting effects and damage passives, built in one step: the engine's lasting kinds, damage passives and prevention, the translator's sentences, and the runs. *Built: all 502 tests pass, 81 hand-written texts the same with the same 7 not read, 1,132 of the 2,097 cards complete (54.0%), and the reference run repeats in what's played. The audit with the right words found none of 50 wrong. Timed back to back against 3b.3, the same games took about 6 to 8% longer, and after a speed fix they're level. Results below* | ✅ |
| 3b.5 | **Ranked vocabulary**, 3b.5 and 3b.6 merged (2026-11-26): the sentences worth the most complete cards first, whatever their kind, until 60% or the list flattens. *The plan is below* | 🔄 |
| 3b.6 | Merged into 3b.5 | — |
| 3b.7 | The four decks: hand-written and custom cards | ⬜ |
| 3b.8 | Audit, stress runs, sign-off | ⬜ |

**Rules for the re-measurement (2026-11-25, before any run):** after the fix, IS-MCTS with the final θ plays hand θ, 6,400 games on seeds from
3,200,001, no invariant checks. M10's result stands if the whole interval is above 50%; mega-greninja's matchups are reported. The
weights aren't refitted. Then 320 games of the defaults (`--games 20 --seed 2000001`) are the run that later steps must repeat.

**What the fix measured (2026-11-25):**

- **The re-import** (`--offline data/decks data/archetypes`): 80 cards, 0 failures, 0 missing effects, all 16 lists legal. Checked
  against the 80 raw TCGdex files, nothing else the new importer does differently touches any of them, so only Greninja ex changed.
  The old cards, rebuilt from today's with that one put back, are in `replays/cards-m10/`, which git ignores, for replaying M10's
  training games. That copy is a reconstruction; it hasn't been checked by playing one back.
- **All 460 tests pass** on the new cards.
- **M10's result stands:** the final θ against hand θ, 6,400 games from 3,200,001, scores **57.8% (56.6% to 59.0%)**: 3,697 won,
  2,703 lost. The sign-off had 57.6%.
- **Mega-greninja isn't measurably weaker:** its score against the other decks, both sides together, is 62.2% (± 1.9) against
  64.1% (± 1.2) at the sign-off, on different seeds. Its matchups, both seatings averaged (± about 3.5, the sign-off's ± about
  2.1): basic-box 54.9% (53.3%), dragapult 60.4% (63.7%), ns-zoroark 53.1% (55.1%), its mirror 55.2% (54.6%).
- **By the deck A plays,** A less B (± about 3.8): dragapult +23.6, ns-zoroark +14.7, basic-box +12.0, mega-greninja +11.8. Each
  is within its error of the sign-off's.
- **The reference run** (IS-MCTS's defaults on both sides, `--agents ismcts,ismcts --games 20 --no-check --threads 16 --seed 2000001`): **158 to 162**, 18.4 turns and 140.8 actions a game,
  ended by Prize cards 266, no Pokémon left 37, deck-out 17. Later steps must repeat it in everything but the times.

**The first coverage report (2026-11-25):** `pokebot-import --standard`, 23 requests, then the same with `--offline` and 0 requests.

| | Complete | Missing effects | Problems | Total |
|---|---|---|---|---|
| Pokémon | 329 | 1,512 | 0 | 1,841 |
| Trainers | 34 | 196 | 1 | 231 |
| Energy | 10 | 15 | 0 | 25 |
| **All** | **373 (17.8%)** | 1,723 | 1 | 2,097 |

- **21 sets, 3,594 printings.** Left out as not legal: Delta Reign's 141, 121 old SVP promos, 30 classic reprints of 30th
  Celebration, 29 of Prismatic Evolutions and 21 others; and 30th Celebration's three Mew with letters for numbers.
- **2,097, not the prototype's 2,109:** 8 Basic Energy named "Basic Grass Energy" and the like are now the same cards as "Grass
  Energy", and 4 corrected 30th Celebration ex (Greninja, Espeon, Sylveon, Umbreon) now match their MEP promos exactly, which
  the promos' stage and HP confirm. A Python check repeats 2,097.
- **Complete today:** the cards with no text, our decks' cards and their reprints. The 40.1% expressible with today's vocabulary
  is what the translator would add; none of it is written yet.
- **One problem:** 30th Celebration's Ultra Ball has no text in TCGdex. The six corrected 30th Celebration ex no longer clash with
  their other printings.
- **Tera not checked: 163** Pokémon ex.
- **The reference run repeats** 158 to 162 in everything but the times, so nothing played changed.

**The four new decks (2026-11-25, chosen by the user):** `alakazam` and `festival-lead`, two popular single-Prize decks, and
`mega-lucario` and `mega-venasaur`, two evolution decks, in [`data/decks/incoming/`](data/decks/README.md) until they're playable. Sorted by the prototype
grammar, against the downloaded sets (distinct cards in each list):

| Deck | Cards | Already complete | Need nothing new | Need new vocabulary | Not parsed |
|---|---|---|---|---|---|
| `alakazam` | 24 | 12 | 3 | 5 | 4 |
| `festival-lead` | 27 | 11 | 3 | 4 | 9 |
| `mega-lucario` | 27 | 14 | 1 | 6 | 6 |
| `mega-venasaur` | 25 | 13 | 3 | 5 | 3, and Ultra Ball (PAF 91) |

- **What they need most:** damage prevention (three decks), Abilities on evolving or when played, Ability locks, self-damage,
  Energy counts, HP passives and next-turn damage effects (two each).
- **The hardest:** `festival-lead`. Festival Grounds, Dipplin and Seaking attacking twice (the extra-attack flow is built, but
  no card uses it yet), Forest of Vitality (the rule question exists), Kieran's "choose 1", and Gladion's Final Battle's "only
  when it is the last card in your hand".
- **Special Energy with triggers or prevention:** Telepathic, Enriching and Mist Energy in `alakazam`, Growing and Rocky
  Energy in the others. Meganium's "each Basic {G} Energy provides {G}{G}" is a rule question (Energy provided) with no passive
  for it yet.
- **Printings from rotated sets:** Rare Candy (SVI 191), Switch (SVI 194), Boss's Orders (PAL 172) and Ultra Ball (PAF 91).
  Their sets aren't in `sets.json`, so the importer can't find them yet.
- **Variants for later:** other `basic-box` and `dragapult` lists, and `basic-box` lists with Mew ex or Mega Rayquaza ex,
  as [archetype lists](data/archetypes/README.md). Mega Rayquaza ex is a Delta Reign card, whose data isn't trusted yet.

**Why 3b.5 and 3b.6 were merged (2026-11-26):** 3b.5's own vocabulary (triggers, Abilities with costs, locks, HP and
Retreat Cost) is a long tail. Of Standard's 300 Ability texts, the commonest unread shapes are evolving from the hand (10),
being damaged by an attack (11, and 2 on Tools), Knocking Out itself (3) and not being Knocked Out (4); the rest are mostly
one-offs. Read, they might complete 15 to 30 cards, while 127 more are needed for 60%. 3b.6's attack shapes are commoner. But
the counts so far are of the first sentence each text can't read, not of cards completed, since a card with two unread texts
gains nothing from one.

| Step | Builds | Status |
|---|---|---|
| 3b.5.1 | **The measurement:** `--check-translations` also counts, over cards without an effects file and with exactly one text not read, that text's first unread sentence, so shapes rank by the cards they'd complete. *Done: 883 cards without an effects file have exactly one text not read, and the list is flat: the top shape stops 13 cards, the 40th 2* | ✅ |
| 3b.5.2 | **The first batch**, built in one step: the gust the opponent chooses and damage to the new Active, drawing up to a hand size, discarding the opponent's Tool before damage, random cards from their hand and the top of a deck, a condition on damaged Benched Pokémon, "this Pokémon can't retreat" next turn, healing the Active, and the triggers on evolving and on being damaged by an attack. At most about 80 cards by the list, perhaps 50 to 70. *Built: all 511 tests pass, 81 hand-written texts the same with the same 7 not read, 1,214 of the 2,097 cards complete (57.9%, 82 more), and the reference run repeats. The audit, with the right words, found 1 of 50 wrong, and its rule is fixed. The speed check wasn't run* | ✅ |
| 3b.5.3 | **The second batch**, in one step: typed Energy counts and coins for each Energy, Pokémon with a named attack, "−" damage per counter, "you may do N more damage", "discard a card from your hand, if you do", no damage counters, Pokémon on the Bench, extra Energy beyond the cost, for each heads an Energy, Special Energy and Tera as nouns, Energy attached in any way you like, and the first turn. At most about 55 cards by the list. *Built, not yet tested or run* | 🔄 |
| 3b.5.4 on | Batches from the top of that list, each as 3b.3 was: engine, translator, then audit and runs. The evolving and damaged-by-attack triggers go in the first batch whatever their rank, as they're cheap (the events exist) and open a recurring Ability shape. The one-off Abilities are left to 3b.7, which writes the four decks' cards | ⬜ |

**Its rules (2026-11-26, before any of it was built):** each batch: every hand-written text read loads the same; an audit of 50
texts using that batch's vocabulary, at most 1 wrong; the reference run repeats, 158 to 162; and timed back to back against
3b.3's commit (`..\pokebot-base`), within noise. M3b's 60% stays the target; no batch has its own.

**What 3b.5.2 measured (2026-11-26):**

- **On all of Standard:** **1,214 of 2,097 cards complete (57.9%)**, against 1,132, so **82 more**, at the top of the guess of 50 to
  70 and near the list's 80. 841 complete by translation. Texts read: attacks 1,205 of 1,757 (from 1,128), Abilities 73 of 300
  (from 51), Trainers 64 of 164 (from 59). **45 more are needed for 60% (1,259).**
- **The incoming decks:** alakazam 15 of 24 cards ready (from 14), mega-lucario 18 of 27 (from 17), mega-venasaur 20 of 25 (from 19).
- **The audit's words were wrong again:** `until` is also the key of every lasting effect, so 35 of the 50 drawn were lasting
  effects from 3b.2 and 3b.4. All 50 were checked by hand and none was wrong, but only 15 used this batch's vocabulary (the
  top of a deck 7, drawing until 3, the gust, a random card, the Tools, and the damage trigger twice), and none the evolve
  trigger or the Bench condition. It's run again with `until:5` to `until:8` in place of `until`. **The lesson for `--using`:** a
  word must be an op, or a key or `key:value` that only the new vocabulary has.
- **The reference run repeats** 158 to 162, 18.4 turns and 140.8 actions a game, in 100.9 s.
- **The audit again** (`until:5` to `until:8` in place of `until`): 50 of the 95 texts, every one this batch's vocabulary: the top
  of a deck 19, the gust 9, drawing until 5, the Tools 4, the evolve trigger 5, the damage trigger 3, a random card 4, the Bench
  condition 2 (some texts have two). **One was wrong:** Samurott's Torrential Whirlpool ("you may switch your Active Pokémon … If
  you do, switch out your opponent's Active Pokémon") required the opponent to have a Benched Pokémon, so it couldn't be used to
  switch your own when they had none. **The fix:** effects after "If you do" add no requirements. A test pins it.
- **The speed check wasn't run:** the 3b.3 copy for timing back to back was removed. Speed is taken up as a goal of its own
  ([long-term goals](#long-term-goals-back-burner)); the 10% check of M3b's sign-off stays.
- **The ranked list now:** 823 cards have exactly one text not read; the top shape ("your opponent reveals their hand") stops
  11, the next 7, and then 6, 4 and 3s.

**3b.4's design (2026-11-25, before any of it was built):** in the cached Standard sets, lasting damage changes and prevention
(about 80 texts: less damage next turn 28, prevention next turn about 35, the Defending Pokémon doing less 10, taking more 3, this
Pokémon doing more next turn 4) and passives (about 20: less damage 9, prevention from Pokémon ex 2, on the Bench 3, less damage
while Active 3, effects prevented 3), plus "the Defending Pokémon can't use attacks" (6) and the attack coin flip (4). Left for
later: triggers when damaged (11, 3b.5), "effects of attacks and Abilities" (4), and discarding the Defending Pokémon (2).
**A guess, to be measured:** about 110 texts, perhaps 40 to 90 more cards complete. Two rules questions are taken as
approximations until checked against a ruling: being switched by an attack isn't counted as an effect that prevention stops,
and "takes less damage from attacks" doesn't reduce damage a Pokémon does to itself.

**Its rules (2026-11-25, before any of it was built):** every hand-written text the translator reads must load the same, and a
text leaves the pinned list only if it's then read and loads the same; an audit of 50 texts using the new vocabulary (`--seed 3`,
with `--using`): at most 1 wrong, each wrong one's rule fixed; the reference run repeats, 158 to 162, 18.4 turns and 140.8
actions a game; no coverage threshold.

**What 3b.4 measured (2026-11-26):**

- **Against the hand-written cards:** 88 texts, 81 the same, 0 different, the same 7 not read.
- **On all of Standard:** **1,132 of 2,097 cards complete (54.0%)**, against 1,047, so **85 more**, inside the guess of 40 to 90.
  759 complete by translation. Texts read: attacks 1,128 of 1,757 (from 1,043), Abilities 51 of 300 (from 35).
- **The incoming decks:** mega-lucario 17 of 27 cards ready (from 16), mega-venasaur 19 of 25 (from 18).
- **The audit's words were wrong.** `--using` matches ops, keys and `key:value`, and the new lasting kinds and passives are values
  (`effect:takes_less`, `modify:damage_taken`), so the words `takes_less`, `deals_less`, `takes_more`, `attack_flip`,
  `damage_taken` and `prevent_damage` matched nothing. The 50 drawn (of 78) were prevention with effects (16), the Bench barrier
  (2), less damage while Active (2), this Pokémon doing more next turn (3), and 27 "can't attack" texts, mostly 3b.2's vocabulary.
  All 50 were checked by hand and none was wrong, but the sample has no texts for less damage, more damage taken, the attack flip
  or damage passives, so it doesn't meet the rule. It's run again with `effect:` and `modify:` words.
- **The audit again** (`--audit 50 --seed 3 --using effect:takes_less,effect:prevent_damage,effect:deals_less,effect:takes_more,`
  `effect:attack_flip,effect:damage_bonus,modify:damage_taken,modify:prevent_damage,modify:prevent_effects,if_active,if_benched`):
  50 of the 97 texts that use 3b.4's vocabulary. Each was checked by hand. **None was wrong**, within the rule of at most 1. The
  sample: less damage next turn 14, prevention with effects 12, prevention of damage only 2, up to an amount 1, from Basic Pokémon
  3, the Defending Pokémon doing less 5 or taking more 2, the attack flip 2, this Pokémon doing more next turn 2, and passives:
  less damage 4, prevention from Pokémon ex 1, less damage while Active 2.
- **The protection tests' deck had 8 copies of one card**, so every one of their games was refused; it has 4 now.
- **One approximation the audit showed:** "attacks used by your opponent's Active Pokémon do N less damage" uses `damage_dealt`,
  which applies only to damage to the Active Pokémon, so a spread attack's damage to the Bench isn't reduced.
- **The reference run repeats** 158 to 162, 18.4 turns and 140.8 actions a game, the same endings, but took **229.3 s**, against 84 to
  100 s. The first 112 games went about five times slower than before (16 games in 22 to 31 s), and the rest at the old pace (16
  in 3 to 5 s). A slowdown from the code would show in every game, so this looks like the machine being busy at the start, but it
  isn't known; the run is repeated.
- **Repeated, it took 134.5 s,** again with a slow stretch (games 80 to 112), and search time per game went from about 2.4 s to
  3.3 s. Repeated in the same place, that points at the code, though it isn't proven: the damage calculation, which the agents
  call very often, now walked the Tools and Abilities in play twice, and choosing targets for an attack walked them once per
  Pokémon. **The fix:** one walk per damage calculation for all three passives (`damage_dealt`, `damage_taken`, `prevent_damage`),
  and choosing targets walks once to see whether any effects prevention is in play at all before checking each Pokémon.
- **After the fix, all 502 tests pass, and the run repeats again but took 341.6 s,** slow from the first game to the last, with every
  part of the search about three times slower, the or-tree too (0.06 s a game against 0.02), though the fix only made the code do
  less. Single runs on this machine have now taken 84, 88, 100, 229, 134 and 342 s, so they can't show a slowdown. **Next:** the
  same games from 3b.3's commit and from today's code, built in two folders and run one after the other.
- **Back to back** (one run each, 3b.3 first, then 3b.4): IS-MCTS, 96 games (`--games 5`), **25.1 s against 27.1 s** (+8%), search
  1.97 and 1.86 s a game against 2.11 and 2.00 (+7%); the plain engine, 20,000 biased games, **2,981.6 against 2,814.1 games a
  second** (5.6% slower). Both played exactly the same games, so 3b.4 changes nothing the old decks play. The long single runs
  were the machine, not the code. The cost is real but small: under M3b's limit of 10%, which counts all of M3b against M10, so
  3b.5 and 3b.6 have little room left. A way to win it back: decide once, when a game's cards are loaded, whether any of them
  has a damage passive, and skip the walk when none does.
- **The speed fix (2026-11-26):** `CardTable::has_damage_passives()`, set when the table is built. Without a damage passive, the damage
  calculation looks through the passives only when the defender is Active, as before 3b.4, and effects prevention doesn't look
  at all. *Done: all 503 tests pass. Back to back again, 3b.3 first: IS-MCTS 28.0 s against 26.8 s, search 2.19 and 2.07 s a
  game against 2.09 and 1.98; the plain engine 2,889.3 against 2,858.0 games a second (1.1% slower). 3b.3's own times moved by
  up to 12% between the two pairs (25.1 to 28.0 s), so the remaining difference is inside the noise: the cost is won back.*

**How 3b.4 is built:** prevention and reductions are effects on the defender, so `ignore_defender_effects` skips them for an Active
defender. Effects prevention is checked as each effect happens: a shielded Pokémon can't be chosen for anything but damage, and
gets no Special Condition or lasting effect. The new lasting fields are hashed so that an effect without them hashes as before.

**3b.3's design (2026-11-25, before any of it was built):** the vocabulary most often missing, counted in the cached Standard sets
(2,108 cards, 2,316 texts): healing (about 100 texts), damage to the attacker itself (83), "if tails, this attack does nothing" (44),
conditions on the opponent's Active Pokémon (60) and on the attacker (67, some already read), counts (about 40 not yet expressible),
Weakness or Resistance ignored (25), damage to each of several Pokémon (15), and discarding an Energy from the opponent's Active
Pokémon (28). Left for later: healing "the same amount of damage you did", counts of one Energy type, "extra Energy beyond this
attack's cost", and "if you can't discard …, this attack does nothing". **A guess, to be measured:** these shapes are in about
300 texts, but many of them have another sentence 3b.3 doesn't read, so perhaps 100 to 250 more cards become complete.

| Step | Builds | Status |
|---|---|---|
| 3b.3.1 | The engine's vocabulary: `heal_self` and `heal_to`, `damage_self`, `"each": true` targets, `"on": "active"`, `ignore_weakness` and `ignore_resistance`, five conditions and five counts. *Done: all 492 tests pass, and the reference run repeats 158 to 162, 18.4 turns and 140.8 actions a game, with the same endings (88.2 s against 84.4 s, one run each, so not a measured slowdown; speed is checked at sign-off)* | ✅ |
| 3b.3.2 | The translator's sentences for them, and "if tails, this attack does nothing". *[Done](tools/import/translate.md#how-it-reads): all 493 tests pass, the 81 hand-written texts read still load the same with the same 7 not read, and 1,047 of the 2,097 cards are complete (49.9%), 674 of them by translation* | ✅ |
| 3b.3.3 | `--audit --using`, the runs, the audit and the record. *[Done](tools/import/main.md#--audit-a-sample-of-translations): the audit of 50 found none wrong, and the reference run repeats* | ✅ |

**Its rules (2026-11-25, before any of it was built):**

- every hand-written text the translator reads must load the same; a text leaves the pinned list of texts not read only if it's
  then read and loads the same;
- an audit of 50 texts whose effects use the new vocabulary (`--seed 2`, with `--using`): at most 1 wrong, and each wrong one's rule
  is fixed;
- the reference run repeats, 158 to 162, 18.4 turns and 140.8 actions a game;
- no coverage threshold; the gain is reported against the guess above.

**What 3b.3.3 measured (2026-11-25):**

- **The audit** (`--audit 50 --seed 2 --using` the new ops, conditions, counts, `each`, `on:active` and `damage_inside_if`): 50 of the
  276 texts that use 3b.3's vocabulary, 49 attacks and 1 Ability. Each was checked by hand against its effects. **None was wrong**,
  within the rule of at most 1. The sample had damage to itself (11), healing (11, one of them Potion), "if tails, this attack does
  nothing" (5), discarding an Energy from the opponent's Active (6), conditions on the opponent's Active (8), counts (4), damage to
  each Benched Pokémon (3) and Weakness or Resistance ignored (2).
- **Two choices the audit leans on, as designed:** an Item or Ability that heals 1 of your Pokémon needs a damaged one, as our
  hand-written cards leave out a card that would do nothing; and "Pokémon ex or Pokémon V" reads as a Pokémon ex, since no Pokémon V
  is in Standard.
- **The reference run repeats** 158 to 162, 18.4 turns and 140.8 actions a game, the same endings. It took 100.4 s, after 84.4 s and
  88.2 s. 3b.3.2 and 3b.3.3 changed only the importer, which the sim doesn't run, so these times vary by at least 14% from run to
  run on this machine; speed is measured at sign-off, not from single runs.

**What 3b.3.2 measured (2026-11-25):**

- **Against the hand-written cards:** 88 texts, 81 the same, 0 different, the same 7 not read.
- **On all of Standard:** **1,047 of 2,097 cards complete (49.9%)**, against 844 after 3b.2, so **203 more**, inside the guess of
  100 to 250. 674 of them are complete by translation. Texts read: attacks 1,043 of 1,757 (from 774), Abilities 35 of 300 (from
  30), Trainers 59 of 164 (from 57); Tools, Stadiums and Special Energy unchanged.
- **The incoming decks:** festival-lead 14 of 27 cards ready (from 13), mega-venasaur 18 of 25 (from 17); alakazam and
  mega-lucario unchanged. The old decks and archetype lists are still ready with none by translation.
- **The sentences now most often not read** are 3b.4's and 3b.6's: damage reduction and prevention next turn (28, 22, 10, 9, 5),
  gusting by the opponent's choice (13), the opponent's hand (12, 9) and deck (9, 7), Abilities that trigger on evolving (10).

**How 3b.3.1 is built:** damage to itself is damage counters on the attacker, which is exact while nothing reduces damage; 3b.4's
damage passives revisit it. `heal_to` offers only damaged Pokémon. `"each"` and `"on": "active"` choose every matching Pokémon with no
decision, so a run's targets now hold up to 9 (a Bench of 8 and the Active). The new flags are hashed so that a run without them
hashes as before.

**3b.2's design (2026-11-25, before any of it was built):** a [translator](tools/import/translate.md) in the importer, strict: it
reads all of a text or none of it. Its effects go into `generated/`, marked `translated: true`, only for cards with no effects file
of their own and no reprint link, so a hand-written file owns its whole card and the old decks can't change.

| Step | Builds | Status |
|---|---|---|
| 3b.2.1 | Cleaning and splitting text, the parts of sentences, `==` for programs, attack sentences, and `--check-translations`. *Done: all 477 tests pass, all 36 hand-written attacks translate to the same program, and 561 of Standard's 1,448 attack texts are read* | ✅ |
| 3b.2.2 | Trainers and Abilities: play conditions, once per turn, "you may"; Tools and Stadiums with today's passives; Special Energy that only provides a type. *[Done](tools/import/translate.md#cards-other-than-attacks): all 480 tests pass, 80 of the 88 hand-written texts are read and all 80 load the same, and 836 of the 2,097 cards have every text read* | ✅ |
| 3b.2.3 | The importer writes translations, the coverage report counts them, and an audit command; the runs, the audit and the record. *[Built](tools/import/standard.md#translations): all 482 tests pass, 81 of the 88 hand-written texts are read and all 81 load the same, 471 cards are complete by translation, the audit of 50 found none wrong, and the reference run repeats* | ✅ |

**Its rules (2026-11-25, before any of it was built):**

- **Against the hand-written effects:** every hand-written text it reads must compile to the same program, unless the hand effect is
  a known approximation listed here.
- **A person checks a sample:** 50 random translated texts from all of Standard (seed 1), each checked and recorded; at most 1 wrong,
  and every wrong one's rule fixed.
- **The old decks repeat** the reference run, 158 to 162.
- **No threshold for coverage** in this step; the prototype suggests 650 to 850 of the 2,097 cards complete, up from 373. On its own
  it makes about 10 of the four new decks' cards ready.

**What 3b.2.2 measured (2026-11-25):** `pokebot-import --offline --check-translations`.

- **Against the hand-written cards:** 88 texts with an effects file. **80 are read, and all 80 load the same** (programs, requirements,
  passives, Ability flags, triggers, what Energy provides); none is different. The rule is met.
- **The 8 not read:** Rare Candy and Transformation Tome (custom code), Crispin ("of different types"), Janine's Secret Art (a
  choice in one sentence used in the next), Area Zero Underdepths (its Bench rule), Adrena-Brain (moving up to 3 damage counters),
  Damp (no effect), and **Crushing Hammer, which should have been read**: "If heads, discard an Energy from 1 of your opponent's
  Pokémon" is today's vocabulary, but no sentence rule was written for it. It's added in 3b.2.3.
- **On all of Standard:** 2,097 distinct cards, **836 with every text read (39.9%)**, cards with no text included, against 373 complete
  today. The prototype's estimate was 846. Texts read: attacks 774 of 1,757 (44.1%), Trainers 55 of 164 (33.5%), Stadiums 4 of 31,
  Special Energy 3 of 17, Tools 3 of 34 (plus one counted apart, under its name "Fluorite", by a bug in the report's sorting: Technical
  Machine: Fluorite has ": " in its name), Abilities 30 of 300 (10.0%).
- **The test against the real cards didn't catch three bugs that the made-up cards did** ("only if you discard…", "+50 HP" written as one
  word, and a noun followed by "with"), because it only fails on texts read differently, not on texts that stop being read. Ultra Ball,
  Secret Box, Hero's Cape and Surfing Beach were quietly skipped until then. In 3b.2.3 it gets the list of texts not read, so one that
  stops being read fails it.
- **The sentences most often not read** need new vocabulary: damage to the attacker (76), healing (43, and 9 and 6 more), a lasting
  damage change on the attacker (28) or the defender (10), "If tails, this attack does nothing" (22), lasting prevention (20 and 5),
  gusting the opponent's choice (13), the opponent's Active Pokémon's Energy (12 and 8) and type (12), the opponent's hand (12 and 9),
  and Abilities that trigger on evolving (10).
- **The reference run repeats** 158 to 162, so Fairy Zone's tidied filter changed nothing played.

**What 3b.2.3 measured (2026-11-25):**

- **Against the hand-written cards** (`--check-translations`): 88 texts, **81 read and all 81 load the same**, none different.
  Crushing Hammer now reads; the 7 not read are exactly the ones the test pins.
- **On all of Standard** (`--standard`): 2,097 distinct cards, **844 complete (40.2%)**, against 373 before translations were
  stored; **471 of them complete by translation**, with no effects file. One problem: 30C-128 Ultra Ball, which TCGdex gives no
  text. `--check-translations` counts 838 cards with every text read; the 6 more complete are hand-written cards whose texts it
  doesn't read, such as Rare Candy. Texts read: attacks 774 of 1,757, Trainers 57 of 164, Tools 3 of 35 (Fluorite now counted
  among them), Stadiums 4 of 31, Special Energy 3 of 17, Abilities 30 of 300.
- **The audit** (`--audit 50 --seed 1`): 50 of the 791 translated texts, 49 attacks and 1 Ability. Each was checked by hand
  against its effects and what the compiler adds (an attack's printed damage is placed first unless a `damage` step is
  written; "can't use" records the attack that set it). **None was wrong**, within the rule of at most 1. The sample is all
  short attack texts, since those are most of what's read; Trainers and Abilities get their share of the 200 in M3b's audit.
- **The four incoming decks** stay incoming: alakazam 14 of 24 cards ready (2 by translation), festival-lead 13 of 27 (2),
  mega-lucario 16 of 27 (2), mega-venasaur 17 of 25 (3). The rest need 3b.3 to 3b.6's vocabulary and Special Energy written by
  hand. The four old decks and every archetype list are ready with none by translation, as designed.
- **The reference run repeats** 158 to 162, 18.4 turns and 140.8 actions a game, ended by Prize cards 266, no Pokémon left 37,
  deck-out 17, so the stored translations changed nothing the old decks play.
- **3b.2's rules are met:** every hand-written text read loads the same (81 of 81), the audit of 50 found none wrong, and the
  reference run repeats.

**How 3b.2.3 was built:** the importer stores each translation in its card's `generated/` file under a `translation` key,
rather than marking merged effects `translated: true` as the design said, so the card's own data stays as TCGdex gave it
and a stale translation is easy to take away. `CardDB::load` merges it only when the card has no effects file and no
`same_as`, as designed. `--standard` counts translated cards as complete, and `pokebot-import --audit N --seed S` prints a
seeded sample of translated texts. With it: a rule for Crushing Hammer's sentence, the report's sorting of Technical
Machine: Fluorite, and the real-card test pinned to the 7 texts it's known not to read.

**3b.2.2's one data change:** Lillie's Clefairy ex's Fairy Zone (`JTG-56`) filtered its passive with `"kind": "pokemon"`, which N's
Castle and Skyliner leave out. A passive only looks at Pokémon in play, so it changes nothing played; it's dropped, as the translator
writes it.

**3b.2.1's one data change:** Raging Bolt ex's Bellowing Thunder (`TEF-123`) filtered "Basic Energy from your Pokémon" with
`"kind": "energy"`, which Mega Greninja ex's file leaves out for attached Energy. Everything attached there is Energy, so it changes
nothing played; it's dropped so both files say it the same way, the way the translator does.

**What 3b.2.1 measured (2026-11-25):** `pokebot-import --offline --check-translations`.

- **Against the hand-written effects:** all 36 attacks with text are read, and all 36 compile to the same program as their
  effects files. None is different and none is unread. That's every hand-written attack, so the rule is met.
- **On all of Standard:** 561 of the 1,448 distinct attack texts are read (38.7%). The prototype estimated about a third of
  attack texts with today's vocabulary.
- **The reference run repeats** 158 to 162, so Bellowing Thunder's tidied filter changed nothing played. All 477 tests pass. Two
  test names first failed with "No tests ran": CTest on Windows passes a test's name as a filter in another encoding, so names
  with "é" match nothing. They're spelled "Pokemon", as the other tests are.
- **The sentences most often not read** need vocabulary the engine doesn't have yet:

| Texts | Sentence | Needs |
|---|---|---|
| 50 | This Pokémon also does # damage to itself | damage to the attacker |
| 30 | Heal # damage from this Pokémon | healing |
| 25 | During your opponent's next turn, this Pokémon takes # less damage from attacks | a lasting damage change on the attacker |
| 12 | If heads, during your opponent's next turn, prevent all damage from and effects of attacks done to this Pokémon | lasting prevention |
| 12 | If your opponent's Active Pokémon is a Pokémon ex, this attack does # more damage | a condition on the defender |
| 11 | Your opponent reveals their hand | the opponent's hand |
| 9 + 7 | Discard the top # card(s) of your opponent's deck | milling |
| 9 | During your opponent's next turn, attacks used by the Defending Pokémon do # less damage | a lasting damage change on the defender |
| 8 | If tails, this attack does nothing | ending the attack |
| 8 | If your opponent's Active Pokémon already has any damage counters on it, this attack does # more damage | a condition on the defender |

  Two of the commoner ones need only grammar, the vocabulary being there: "Discard a Stadium in play" (5) and "During your opponent's
  next turn, that Pokémon can't retreat" (5). They go into 3b.2.2.

**What 3b.2a measured (2026-11-25):** `pokebot-import data/decks/incoming`, 75 requests, 82 new cards, 0 failures, 0 name mismatches,
all four decks legal.

| Deck | Cards ready | Still missing effects |
|---|---|---|
| `alakazam` | 12 of 24 | 12 cards, 15 effects |
| `festival-lead` | 11 of 27 | 16 cards, 20 effects |
| `mega-lucario` | 14 of 27 | 13 cards, 16 effects |
| `mega-venasaur` | 14 of 25 | 11 cards, 13 effects |

- **Set codes found through TCGdex:** SVI, PAL and PAF.
- **6 printings use a reprint's effects:** the older Rare Candy (SVI 191), Switch (SVI 194), Boss's Orders (PAL 172) and Ultra Ball
  (PAF 91), and Ultra Ball (ASC 213) and Night Stretcher (SFA 61). The old decks get no links: every one of their cards has its own
  effects file.
- **The ready counts match the prototype's,** and mega-venasaur's PAF Ultra Ball, which the prototype couldn't place, is ready.
- **To check by hand:** Cornerstone Mask Ogerpon ex (TWM 112) for Tera, and what five Special Energy provide (Growing Grass,
  Rocky Fighting, Telepathic Psychic, Enriching, Mist).
- **The reference run repeats** 158 to 162, so nothing played changed, and all 470 tests pass with the new cards in `generated/`.

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

## M5 — Greedy agent, match harness, profiling ✅

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
| 5.3 | **The evaluation function** `e` (features φ1–φ8 as 10 features, hand-set weights), zero-sum and reading only what both players can see. See [evaluation](src/agents/evaluation/evaluation.md) | ✅ |
| 5.4 | **Greedy agent:** one action ahead, averaged over 3 worlds with the same luck for every action, and a fixed rule for setup; `--agent greedy` in `pokebot-sim` and `pokebot`. See [greedy agent](src/agents/greedy/greedy_agent.md) | ✅ |
| 5.5 | **Match harness:** one agent against another on every deck pairing, each seed in both seats, on several threads with results that don't depend on how many; win rates with 95% Wilson intervals overall, by who went first and per pairing; replays kept; `pokebot-sim --agents A,B`. See [arena](src/harness/arena.md) | ✅ |
| 5.6 | Sign-off: Greedy against Random on every deck pairing, and profiling. *First full run (16,000 games): Greedy 77.7% (95% CI 77.1% to 78.4%), but only 39.3% to 51.2% with ns-zoroark against three of the decks: it draws its deck away (Trade with an empty deck) and scores effects half-way through. So 5.6 is done in steps, each measured:* | ✅ |
| 5.6a | How A won and lost (Prize cards, no Pokémon left, deck-out), so a loss by A's own deck-out shows. *Baseline: Greedy lost 3,566 games, 3,457 (97%) by its own deck running out; of its 12,434 wins, 7,223 were Random decking out and 4,206 by Prize cards* | ✅ |
| 5.6b | Rebalance deck against hand in e: the deck counts up to 30 cards, the hand only up to 7. See [evaluation](src/agents/evaluation/evaluation.md#features). *Greedy 82.5% (95% CI 81.9% to 83.1%), up from 77.7%; losses by its own deck-out 2,667, down from 3,457; 13 of 16 pairings better, but ns-zoroark against basic-box, dragapult and mega-greninja still 50.4%, 52.6% and 46.2%: starting Trade is scored before its discard and draw happen* | ✅ |
| 5.6c | Greedy finishes its own choices inside an effect before scoring. See [greedy agent](src/agents/greedy/greedy_agent.md#how-it-chooses). *Greedy 91.3% (95% CI 90.8% to 91.7%); losses by its own deck-out 1,321, down from 2,667; the ns-zoroark rows 79.8%, 86.8% and 76.8%; about 3.3 times slower* | ✅ |
| 5.6d | Sign-off: the full run with invariant checks, Random against Random as a fairness check, Greedy against biased, speed recorded; same-agent matches labelled A and B | ✅ |

**Sign-off run (2026-11-19):** `pokebot-sim --agents greedy,random --games 1000` on all 16 deck
pairings (16,000 games, every seed once with Greedy in each seat), with the invariant checker after
every action: **no problems found**, and exactly the same results as without the checks.

| Measure | Result |
|---|---|
| Greedy's score against Random | **91.3%** (95% CI 90.8% to 91.7%): 14,605 won, 1,395 lost, 0 drawn |
| By who went first | Greedy first 91.1% (90.6% to 91.6%), Random first 91.9% (91.0% to 92.7%) |
| Every deck pairing | 76.8% to 99.9%; the lowest lower end is 74.1% (ns-zoroark against mega-greninja) |
| How Greedy won | Random's deck ran out 8,201, Prize cards 5,154, no Pokémon left 1,250 |
| How Greedy lost | Its own deck ran out 1,321, Prize cards 60, no Pokémon left 14 |
| Greedy against biased random | 75.8% (75.2% to 76.5%); every pairing's lower end above 56% |
| Random against Random (fairness check) | 49.6% (48.8% to 50.3%); mirror pairings 48.5% to 51.4% |
| Speed, Greedy against Random | about 856 games/s on 16 threads, 92 on one |
| Speed, engine (`--agent biased`, as at M4) | about 2,880 games/s: the observation's new fields cost nothing measurable |

How each change in 5.6 moved Greedy, on the same 16,000 seeded games against Random:

| Greedy | Score (95% CI) | Lost by own deck-out | Lowest pairing | Games/s (16 threads) |
|---|---|---|---|---|
| As built in 5.4 | 77.7% (77.1% to 78.4%) | 3,457 | 39.3% | about 2,890 |
| 5.6b: deck counted up to 30, hand up to 7 | 82.5% (81.9% to 83.1%) | 2,667 | 46.2% | about 2,810 |
| 5.6c: follow-up choices played out | 91.3% (90.8% to 91.7%) | 1,321 | 76.8% | about 856 |

Known limits, each documented where it lives:
- **Greedy sees one action ahead.** It misses a win two actions away: in a saved game it switched
  in the N's Zoroark ex with no Energy, when switching in the one with Energy and attacking would
  have taken its last 2 Prize cards. Finding such lines is the turn planner's job (M6).
- **It still decks itself out sometimes** (1,321 of its 1,395 losses to Random): e values each
  card in the deck the same, however many turns the game has left.
- **The opponent's unseen cards are a placeholder** in Greedy's worlds until M7 deals them from a
  guessed decklist.
- **Greedy is slow** for what it does (about 9 ms a game on one thread), which matters once M7
  uses it for playouts: profiling it is a good first task there.
- **The weights θ are set by hand,** two of them adjusted in 5.6b after measuring; M10 learns them.

## M6 — Model 1: turn planner ✅

**Done when:** the or-tree mode finds the win in every position of a hand-built test suite, and the
and-tree mode's Planner beats Greedy. The suite also reports the nodes expanded with and without the
closed set, and with and without f_bound.

The planner searches the certain part of the current turn: Det actions, ending in one Bnd action.
The or-tree mode looks for the first plan that reaches a goal (a win, a Knock Out, k Prize cards); the
and-tree mode finds the best plans by branch-and-bound. Until IS-MCTS (M8), the Planner plays the
or-tree's winning plan if there is one, and the and-tree's best plan otherwise.

**How actions are classed (decided 2026-11-19):** by applying the action once: Det if it uses no
chance (the random number generator didn't move), reveals nothing (the player knows the place of no
new card) and leaves the player in their main phase; Fin if only the last fails. A Prize card taken
for a Knock Out doesn't count as revealing for Fin, and the engine now takes Prize cards from the end
of the pile rather than at random, so a Knock Out needs no chance.

| Step | Builds | Status |
|---|---|---|
| 6.1 | **Action classes** Det, Fin and the rest of Bnd, by applying the action once. See [action classes](src/agents/planner/action_class.md) | ✅ |
| 6.2 | **State hash and closed set:** a 64-bit hash of everything but the random number generator and the hand's order, so equivalent orders of the same actions hash the same. See [state hash](src/agents/planner/state_hash.md) | ✅ |
| 6.3 | **Plans and plan value:** δ*, and e (or u) at the end after the player's follow-up choices, averaged over m sampled worlds when the last action is uncertain, with the same worlds for every action at one state. See [plans](src/agents/planner/plan.md) | ✅ |
| 6.4 | **Or-tree mode:** goals γ_win, γ_KO and γ_prize^k, best-first by h_KO, children ordered by action class, node budget B_or. See [or-tree](src/agents/planner/or_tree.md) | ✅ |
| 6.5 | **And-tree mode:** branch-and-bound for the top-K plans (one per end state), depth-first with the best child first, f_bound from a relaxed problem, node budget B_tp. Plan values now deal each state's sampled worlds from its hash, so they don't depend on the search's order. See [and-tree](src/agents/planner/and_tree.md) | ✅ |
| 6.6 | **Planner agent:** in its main phase, the or-tree's win, else the and-tree's best plan, followed while its observation is what the plan expected and planned again when not; Greedy for every other decision; `--agent planner`. See [planner](src/agents/planner/planner_agent.md). *First run, 100 games per pairing: Planner 73.2% (95% CI 71.0% to 75.3%) against Greedy, but ns-zoroark against mega-greninja only 41.0%, and 7.5 games/s on 16 threads. So the Planner now times its parts, agents report counters to the match harness, and `pokebot-sim` shows progress. With one sampled world instead of three: 73.8% (95% CI 71.6% to 75.9%) at 38.0 games/s, so one is now the default (`--planner-samples` and `--planner-or-budget`/`--planner-and-budget` change the settings). The weak pairing is the deck, not the Planner: with Greedy on both sides ns-zoroark wins only 21% (95% CI 14.2% to 30.0%) against mega-greninja, and 22% to 30% against the other decks, so the Planner's 38% there is still 17 points better than Greedy's* | ✅ |
| 6.7 | **Test positions:** the win found in every one, and nodes expanded with and without the closed set and f_bound. The [planner lab](src/harness/planner_lab.md) measures both on positions from Greedy-against-Greedy games (`pokebot-sim --planner-lab N`), besides the hand-built positions in the tests. *64 games, 928 positions searched to the end: no false wins, but the 200-node win search found only 9 of 12 wins (the deepest took 1,420 nodes); the closed set cuts both searches by about 2.3 times; f_bound never changed the best value but saves only 3%. With a 2,000-node win search the Planner scores 74.2% (95% CI 72.1% to 76.3%) against Greedy at 23.3 games/s, no better than 73.8% at 200 nodes and 38.0 games/s: the and-tree's best plan is worth 1 when it wins, so it finds most of the wins the or-tree misses. The budget stays 200* | ✅ |
| 6.8 | **Sign-off:** Planner against Greedy on every deck pairing, and speed | ✅ |

**Sign-off run (2026-11-19):** `pokebot-sim --agents planner,greedy --games 1000` on all 16 deck
pairings (16,000 games, every seed once with the Planner in each seat; 1 sampled world, or-tree
budget 200, and-tree budget 300), with the invariant checker after every action: **no problems
found**.

| Measure | Result |
|---|---|
| The Planner's score against Greedy | **73.8%** (95% CI 73.1% to 74.4%): 11,800 won, 4,200 lost, 0 drawn |
| By who went first | Planner first 75.5% (74.5% to 76.4%), Greedy first 72.0% (71.0% to 73.0%) |
| Every deck pairing | 40.5% to 92.6%; only ns-zoroark against mega-greninja is below 56.5% |
| How the Planner won | Prize cards 10,036, Greedy's deck ran out 953, no Pokémon left 811 |
| How the Planner lost | Prize cards 3,268, its own deck ran out 740, no Pokémon left 192 |
| Per game | 22.3 plans, 0.63 wins found by the or-tree, 57.0 actions from plans and 10.1 from Greedy; 2,293 or-tree and 3,035 and-tree nodes; 10,187 plan endings scored, 8,821 of them uncertain |
| Speed, Planner against Greedy | about 33.9 games/s on 16 threads without checks (32.5 with), 3.5 on one |

The Planner beats Greedy on every pairing but one, and that one is the deck: with Greedy on both
sides, ns-zoroark wins only 21% against mega-greninja (step 6.6), so the Planner's 40.5% there is
still about 20 points better than Greedy's.

Known limits, each documented where it lives:
- **The or-tree's 200-node budget misses some wins** (9 of 12 in the [planner lab](src/harness/planner_lab.md)),
  but the and-tree's best plan finds most of them, and 2,000 nodes scored no better.
- **f_bound saves little:** 3% of and-tree nodes. It never changed the best value in 928 positions,
  but it isn't proven sound.
- **The Planner is slow** for what it adds: about 290 ms a game on one thread, against about 11 ms
  for a game of Greedy against Random. Two thirds of that is scoring plan endings in sampled worlds.
- **e barely sees what a deck is built to do:** ns-zoroark's Abilities and copied attacks are worth
  little to its 10 features, which is why that deck gains least. M10 learns the weights.
- **The opponent's unseen cards are a placeholder** in the worlds until M7 deals them from a
  guessed decklist.

## M7 — Determinization and flat Monte Carlo ✅

**Done when:** Flat MC beats the Planner over at least 1,000 seeded games per deck pairing in both
seat orders.

A determinization ŵ deals the cards the searching player can't see over the hidden zones (the
opponent's hand, both decks, both Prize piles), each at its observed size. The opponent's cards are
dealt from a decklist guessed from a library of archetype lists, never from the list they're really
playing. Flat MC, the stepping stone to IS-MCTS (M8), scores each of the and-tree's top-K plans by
playing on from it in m determinizations and takes the best average.

**Decisions (2026-11-21):**
- **Archetype library:** each deck's list plus two variants of it, in `data/archetypes/<archetype>/<variant>.txt`,
  with a uniform prior over archetypes and then over each archetype's variants.
- **Belief:** P(L | S) ∝ P(L)·∏_c f(n_c(L), k_c(S)) over the opponent cards seen S, where
  f(n, k) = C(n, k) when k ≤ n and ε^(k−n) otherwise (a card no list explains). Every list has 60
  cards, so the hypergeometric denominator cancels.
- **Playouts:** each player decides from its own observation of ŵ, so nothing that decides for the
  searching player sees more than they can. The deck model is needed for playouts: with placeholder
  cards the opponent's reply turn can't play anything. Agents inside playouts keep the placeholder
  in their own worlds.
- **Decisions outside the main phase** go to Greedy, as in the Planner; searching the ones with
  several actions is measured in 7.6.
- **Budgets are counts, not time:** N playouts a decision, split evenly over the plans, so results
  repeat and don't depend on the thread count. One playout is comparable to one IS-MCTS iteration.
  Time budgets come later for live play.
- **Speed (profiled 2026-11-19, release, one thread, mid-game):** an and-tree search 23 ms at budget
  300 (the same for keep 1, 4 or 8); one turn played by Greedy with 3 worlds 0.79 ms, with 1 world
  0.28 ms, by biased random 0.018 ms; a `World` 2.2 µs. Starting point K = 4, m = 8, r = 1, Greedy
  with 1 world: about 0.8 s a game of Flat MC against the Planner on one thread, so about 23 minutes
  for 16,000 games on 16 threads.

| Step | Builds | Status |
|---|---|---|
| 7.1 | **Archetype library:** `data/archetypes/<archetype>/<variant>.txt` in the decklist format, each deck's list as `main` plus two variants using only cards with effects (12 lists); loaded and checked (legal, every card's effects written) on its own, never from the decks a game uses; `pokebot-import` takes folders, so `pokebot-import data/decks data/archetypes` checks every list. See [deck model](src/agents/flat_mc/deck_model.md) and [archetypes](data/archetypes/README.md) | ✅ |
| 7.2 | **Belief over lists:** `DeckModel`, P(L \| S) ∝ P(L)·∏ f(n_c, k_c) from the opponent's cards seen, hypergeometric counts with ε = 0.01 for each copy no list explains, uniform over archetypes and then variants, in logs. Tests: the prior with nothing seen, one archetype's card settling it, a tech card picking the variant, unexplained cards keeping it defined; on the real decks, the same cards seen giving the same belief whatever the real list, and the opponent's archetype found, even with its exact list left out. See [deck model](src/agents/flat_mc/deck_model.md#the-belief) | ✅ |
| 7.3 | **Determinization:** `World(observation, rng, &model)` draws a list from the belief and deals its cards, less the ones seen, to the opponent's never-seen cards; cards seen that the list doesn't explain leave out random ones; each hidden zone keeps its observed size; without a model it keeps the placeholder and uses `rng` exactly as before. Tests: on the real decks against all 12 lists, worlds observe back, keep the invariants and offer the same actions, deal only from the list drawn, draw several lists, and never the opponent's list when the library leaves it out. See [world](src/agents/world.md#with-a-deck-model) | ✅ |
| 7.4 | **Playouts:** from ŵ, play until the searching player's r-th next main phase with a policy (Greedy with w worlds, or biased random), each player deciding from its own observation, then score with e, or u if the game is over: `play_out` stops at the start of the searching player's r-th next main phase (counting only later turns), the game's end or 2,000 actions, its agents seeded from one seed. Leak test: on the real decks, games differing only in cards the searcher can't see give exactly the same playouts. See [playout](src/agents/flat_mc/playout.md). *Cost, from Flat MC's counters in its first match (step 7.5, release, 16 threads): 624 playouts a game, 7.1 actions each, about 0.56 ms a playout with Greedy (1 world) at r = 1, so 0.35 s a game, more than its and-tree's 0.22 s* | ✅ |
| 7.5 | **Flat MC agent:** the or-tree's win, else the and-tree's top-K plans, each scored by its mean over the same m determinized playouts; the best mean is played and followed as the Planner does; Greedy for every other decision. It also reports its ranked plans with their means and playout counts (for the analysis mode later). `--agent flatmc` and its settings. *Done: `FlatMcAgent` with K = 4, N = 32 (m = 8), r = 1 and Greedy with 1 world by default, every plan played out in the same worlds with the same seeds, the root world and playouts dealt from the deck model, the `--planner-*` budgets shared with the Planner; `--mc-plans`, `--mc-playouts`, `--mc-depth`, `--mc-policy`, `--mc-greedy-worlds`; counters for playouts and their time. See [flat MC](src/agents/flat_mc/flat_mc_agent.md). First match, `--agents flatmc,planner --games 100` (1,600 games): Flat MC 55.2% (95% CI 52.8% to 57.7%) against the Planner at 18.4 games/s on 16 threads; 28% to 88% by pairing, lowest ns-zoroark against mega-greninja (28%), basic-box (34%) and dragapult against basic-box (36%); it lost 60 games by its own deck running out and won 18 that way. Per game: 20.0 plans, 19.5 of them played out, 624 playouts, 0.22 s in the and-tree and 0.35 s in playouts* | ✅ |
| 7.6 | **Tuning against the Planner,** one change at a time on the same seeded games: playout policy, r, K, m, searching decisions outside the main phase, and the Planner with the deck model as a control. *First six runs below: the deck model alone adds nothing (K = 1 scores 49.4%), so the gain is the playouts; playing further (r = 2) and more playouts (N = 64) each add about 5 points; Greedy with 3 worlds adds 2 at 3.5 times the playout cost; biased playouts and r = 0 are worse than the defaults. Next three: r = 3 no better than r = 2; r = 2 with N = 64 62.1%; and with K = 8 as well, 67.1% (95% CI 64.7% to 69.3%) at 6.7 games/s. Then K = 16: 67.9% at the same cost, and 69.6% with N = 128 at half the speed. Defaults set to r = 2, N = 64, K = 16. Playing out the decisions outside the main phase too: 73.8% (95% CI 71.5% to 75.8%) at 4.7 games/s, the best of 7.6, so it's on by default (`--mc-greedy-choices` turns it off). Final defaults: K = 16, N = 64, r = 2, Greedy playouts with 1 world, choices played out* | ✅ |
| 7.7 | **Sign-off:** Flat MC against the Planner on every deck pairing, with invariant checks; speed; known limits. *Done: 72.9% (95% CI 72.2% to 73.6%) over 16,000 games, no invariant problems, 5.3 games/s* | ✅ |

How each setting moved Flat MC against the Planner in 7.6, on the same 1,600 seeded games (`--agents flatmc,planner
--games 100 --no-check`, release, 16 threads), one change at a time from the defaults (K = 4, N = 32, r = 1, Greedy
playouts with 1 world):

| Flat MC | Score (95% CI) | Lowest pairing | Lost by own deck-out | Playout s a game | Games/s |
|---|---|---|---|---|---|
| Defaults (step 7.5) | 55.2% (52.8% to 57.7%) | 28% ns-zoroark vs mega-greninja | 60 | 0.35 | 18.4 |
| K = 1: no playouts, the Planner with the deck model (control) | 49.4% (47.0% to 51.9%) | 19% ns-zoroark vs basic-box | 30 | 0 | 29.6 |
| r = 0: scored right after the plan | 47.3% (44.9% to 49.8%) | 17% ns-zoroark vs basic-box | 21 | 0.01 | 24.4 |
| r = 2: to the searching player's second next turn | 60.3% (57.9% to 62.7%) | 30% dragapult vs basic-box | 65 | 0.82 | 12.3 |
| Greedy with 3 worlds in the playouts | 57.3% (54.9% to 59.7%) | 31% ns-zoroark vs basic-box | 46 | 1.22 | 8.6 |
| Biased random playouts | 51.7% (49.2% to 54.1%) | 24% ns-zoroark vs basic-box | 34 | 0.04 | 16.1 |
| N = 64 (m = 16) | 60.4% (58.0% to 62.8%) | 35% ns-zoroark vs basic-box | 50 | 0.91 | 10.3 |
| r = 3 | 60.4% (58.0% to 62.8%) | 33% dragapult vs basic-box | 50 | 1.47 | 7.8 |
| r = 2, N = 64 (m = 16) | 62.1% (59.7% to 64.5%) | 36% dragapult vs basic-box | 58 | 1.94 | 6.4 |
| r = 2, N = 64, K = 8 (m = 8) | 67.1% (64.7% to 69.3%) | 39% ns-zoroark vs mega-greninja | 56 | 1.84 | 6.7 |
| r = 2, N = 64, K = 16 (m = 4) | **67.9% (65.6% to 70.2%)** | 41% ns-zoroark vs basic-box | 62 | 1.89 | 6.6 |
| r = 2, N = 128, K = 16 (m = 8) | 69.6% (67.3% to 71.8%) | 47% dragapult vs basic-box | 64 | 4.01 | 3.5 |
| New defaults with choices played out: **the final defaults** | **73.8% (71.5% to 75.8%)** | 44% ns-zoroark vs basic-box | 59 | 2.89 (0.97 of it choices) | 4.7 |

- **The deck model alone is worth nothing measurable:** with K = 1 Flat MC is the Planner with its root world dealt
  from the model, and it scores 49.4%. The opponent's hidden cards barely matter to e or to Det plans; they matter in
  playouts, where the opponent has to play real cards.
- **r = 0 is worse than the Planner** (47.3%): it re-ranks the and-tree's plans by e in single worlds without the
  follow-up choices the Planner's scorer plays out, so the noise only hurts.
- **Seeing the opponent's reply is what helps:** r = 1 gives 55.2%, r = 2 60.3%, and twice the playouts 60.4%. Better
  playout play (Greedy with 3 worlds) helps less per second than more or longer playouts; biased playouts are too
  weak to rank plans.
- **Depth stops paying at r = 2:** r = 3 scores the same as r = 2 (60.4% and 60.3%) at nearly twice the playout cost,
  and r = 2 with N = 64 only adds 2 points over either alone.
- **More plans matter more than more playouts per plan:** with the same 64 playouts, K = 8 (m = 8) scores 67.1%
  against 62.1% for K = 4 (m = 16), at the same cost. The and-tree's 5th to 8th plans are often better than its
  top 4 once the opponent's reply is played out: e at the end of a turn ranks plans less well than playouts do.
  K = 16 at the same 64 playouts scores 67.9%, no clearly better than K = 8: the gain from more plans levels off.
- **Doubling the playouts again buys little:** N = 128 with K = 16 scores 69.6%, 1.7 points more for twice the
  playout time (3.5 games/s, so about 80 minutes for a 16,000-game sign-off against about 40 at N = 64).
- **Playing out the decisions outside the main phase is worth 6 points:** 73.8% against 67.9% with the same
  settings, for 8.7 choices a game (a new Active, what a search takes, what to discard) that Greedy made before.
  Only 2.3 decisions a game are left to Greedy, all in setup. It costs about 1 s a game, 4.7 games/s against 6.6.
  Greedy's one action ahead was the weakest part of Flat MC's play, more than its plan ranking.
- **Deck-outs grow with depth:** Flat MC loses more games to its own deck running out (65 at r = 2, 21 at r = 0). Longer
  playouts reward drawing, and e counts the deck only up to 30 cards. A limit to keep in mind for M10's learned e.

**Sign-off run (2026-11-22):** `pokebot-sim --agents flatmc,planner --games 1000` on all 16 deck pairings (16,000
games, every seed once with Flat MC in each seat; Flat MC with K = 16, N = 64, r = 2, Greedy playouts with 1 world
and its choices played out; the Planner as signed off in M6, 1 sampled world and budgets 200 and 300, which Flat MC
shares), with the invariant checker after every action: **no problems found**.

| Measure | Result |
|---|---|
| Flat MC's score against the Planner | **72.9%** (95% CI 72.2% to 73.6%): 11,668 won, 4,332 lost, 0 drawn |
| By who went first | Flat MC first 75.7% (74.8% to 76.7%), Planner first 70.1% (69.1% to 71.1%) |
| Every deck pairing | 45.5% to 92.1%; only ns-zoroark against basic-box is below 50% (45.5%, 42.4% to 48.6%), and ns-zoroark against mega-greninja is even (50.7%); every other pairing's lower end is above 50% |
| How Flat MC won | Prize cards 10,642, no Pokémon left 921, the Planner's deck ran out 105 |
| How Flat MC lost | Prize cards 3,588, its own deck ran out 518, no Pokémon left 226 |
| Per game | 18.7 plans, 0.64 wins found by the or-tree, 26.8 decisions played out (8.8 of them choices outside the main phase), 1,686 playouts of 17.1 actions each, 57.4 plan actions, 2.3 Greedy decisions (setup); 1,347 or-tree and 1,735 and-tree nodes |
| Time per game (on 16 busy threads) | 0.18 s in the and-tree, 2.59 s in playouts (0.88 s of it for choices), about 0.1 s a searched decision |
| Speed, Flat MC against the Planner | about 5.3 games/s on 16 threads with invariant checks |

Flat MC beats the Planner on 15 of the 16 pairings. The one it loses, ns-zoroark against basic-box, is the deck the
evaluation reads worst (M5 and M6): with the M7 defaults Flat MC won only 28% to 34% there, and playing the opponent's
reply out has brought it to 45.5%.

How M7 got there, on the same 1,600 seeded games (the full table is above):

| Flat MC | Score against the Planner |
|---|---|
| As built in 7.5: K = 4, N = 32, r = 1, choices to Greedy | 55.2% |
| K = 1, no playouts: the Planner with the deck model (control) | 49.4% |
| r = 2, N = 64, K = 16 | 67.9% |
| and choices played out (the sign-off settings) | 73.8%, and 72.9% over the 16,000 sign-off games |

Known limits, each documented where it lives:
- **Flat MC decks itself out:** 518 of its 4,332 losses are its own deck running out, against 105 wins that way.
  Playouts reward drawing, and e counts the deck only up to 30 cards, however many turns the game has left. M10's
  learned e is where this belongs.
- **e still decides what a playout's end is worth.** ns-zoroark's Abilities and copied attacks are worth little to e's
  10 features, which is why its pairing against basic-box is still below even.
- **The deck model adds nothing measurable on its own** (49.4% with K = 1). The opponent's hidden cards matter in
  playouts, where the opponent has to play real cards, not to e or to Det plans. The library holds the real lists as
  each archetype's `main`; agents facing lists the library doesn't hold are tested in the deck model's and world's
  tests, not yet in an arena.
- **Opponents in a determinization forget what they saw.** In ŵ the opponent knows its own sampled hand and what's in
  the open, but not the searcher's cards it was shown earlier (a mulligan, a revealed search).
- **Flat MC is flat:** each plan's playouts are averaged with no tree below it, and every plan gets the same
  playouts, however clearly it's losing. IS-MCTS (M8) shares one tree across determinizations and puts its playouts
  where they matter.
- **The cost is mostly playouts:** 2.6 of its roughly 2.8 s of search a game. One playout is comparable to one IS-MCTS
  iteration, so M8 starts from the same budget: 64 playouts a decision, about 0.1 s.

## M8 — Model 2: IS-MCTS ✅

**Done when:** IS-MCTS beats Flat MC over at least 1,000 seeded games per deck pairing in both seat orders, and its
win rate is measured as a function of the time budget.

IS-MCTS shares one tree across determinizations: each iteration deals a world ŵ, walks down the tree choosing by UCB1
among the moves legal in ŵ, adds one move, plays out from it and backs the result up. Nodes are information sets of the
searching player, so plays that only work when the hidden cards are known aren't overvalued (strategy fusion).

**Decisions (2026-11-22):**
- **A node is the searcher's information set:** children are keyed by a hash of the searcher's observation after the
  move, so an opponent move or chance outcome that looks the same to the searcher in every world leads to the same
  node. Cards are hashed by what they are, not by their number, since the opponent's unseen cards are numbered
  differently in each world.
- **Macro-actions:** the searcher's from the full and-tree, cached by information set; the opponent's from a cheap
  and-tree, since its hand changes from world to world; decisions outside the main phase as single actions. The
  and-tree costs about 23 ms mid-game, so the opponent's budget is chosen from measurements.
- **Compared with Flat MC at equal time,** set as iteration counts so runs repeat: the iteration budget that matches
  Flat MC's time per decision, and for the time curve about 0.5, 1, 2 and 4 times it, with the measured time per
  decision as the x-axis.
- **It follows its most-visited plan** while the observation is the one it expected, as the Planner and Flat MC do,
  and searches again otherwise.
- **Rewards** are e in [−1, 1] (u at the end of a game), negated for the opponent; C starts at 0.7 and is tuned.

| Step | Builds | Status |
|---|---|---|
| 8.1 | **Information-set key:** a 64-bit hash of an `Observation`, with every card hashed by its definition (or as unseen) rather than its number, the hand and the unordered lists sorted. Tests: equal observations and the hand's order; two worlds where the opponent attaches a different-numbered copy of the same Energy hashing the same; changes the viewer can see changing it. See [information-set key](src/agents/is_mcts/info_set.md) | ✅ |
| 8.2 | **The tree:** nodes (actor, N, Q for each player, A), children keyed by the observation after the move, UCB1 with availability counts over the children legal in ŵ, backpropagation with Q negated for the other player. Tests on a small hand-built game: visits concentrate on the best move, and availability counts only legal siblings. See [tree](src/agents/is_mcts/tree.md) | ✅ |
| 8.3 | **Macro-actions at a node:** the actor's moves in ŵ, the searcher's from the full and-tree cached by information set, the opponent's from a cheap and-tree, single actions outside the main phase; cost and quality measured, and the opponent's budget chosen. *`MacroActions` and the [budget lab](src/harness/budget_lab.md) (`pokebot-sim --budget-lab N`) are built. The lab (64 games, 1,234 positions): the 300-node reference 15.0 ms a search; 25 nodes 1.9 ms, the same best plan 79% of the time, 0.031 of e lost on average; 50 nodes 3.4 ms, 85%, 0.021; 100 nodes 5.9 ms, 91%, 0.010. Every budget found 3.84 plans. The opponent's budget is 25, with 50 and 100 measured in 8.6. See [macro-actions](src/agents/is_mcts/macro_actions.md)* | ✅ |
| 8.4 | **IS-MCTS search:** determinize, select, expand one untried macro-action, evaluate (r turns of Greedy, then e or u), backpropagate; an iteration budget; the most-visited root plan, with the root's visits and means as the analysis. Tests: the same seed gives the same result; the hidden-card leak test. *`IsMcts::search`: 200 iterations, C = 0.7, r = 1 by default; children keyed by the searcher's observation, so chance outcomes need no chance nodes; a root with one move returns it at once. See [IS-MCTS search](src/agents/is_mcts/is_mcts.md)* | ✅ |
| 8.5 | **IS-MCTS agent:** the or-tree's win, else IS-MCTS for every decision after setup, the plan followed while the observation matches; `--agent ismcts` and its settings; counters for iterations, nodes and time. *`IsMctsAgent` with 64 iterations to start; `--mcts-iterations`, `--mcts-c`, `--mcts-depth`, `--mcts-plans`, `--mcts-opponent-budget`, `--mcts-opponent-plans`, the `--planner-*` budgets shared. See [IS-MCTS agent](src/agents/is_mcts/is_mcts_agent.md). The first arena run stopped with "hidden cards don't add up": the searcher's plans were cached by card identity, so a plan found where it had drawn one copy of a card was reused in a world where it had drawn another, and moved a card that wasn't there. The cache now numbers the searcher's cards and checks any cached plan that names an opponent card. The next run stopped with "not an answer to an effect decision" (game seed 18, now saved as a replay by the arena): a plan's Det actions are classed in the world it was found in, and a few depend on how the player's hidden cards split between deck and Prize cards, which they can't see, so the same plan's next action met a pending choice in another world. Plans applied outside their own world now go through `apply_plan_checked` (IS-MCTS's tree and plan walk, Flat MC's scoring); in game 18, 42 of IS-MCTS's moves were unplayable in the world they were tried in. First full match, `--agents ismcts,flatmc --games 100` (1,600 games, no failures): IS-MCTS 41.6% (95% CI 39.2% to 44.0%) against Flat MC at 2.6 games/s. Its 64 iterations already match Flat MC's time: 0.103 s a search (1.6 ms an iteration: 0.7 ms playouts, 0.4 ms the opponent's and-trees, 0.3 ms the searcher's) against 0.106 s a searched decision for Flat MC. Per game: 31.3 searches, 2,003 iterations, 1,049 single-action nodes, 105 searcher and-trees with 1,676 cache hits, 363 opponent and-trees, 0.17 moves unplayable in a world* | ✅ |
| 8.6 | **Tuning against Flat MC** at matched time, one change at a time on the same seeded games: C, r, the opponent's macro-action budget, K at the searcher's nodes. *First four runs below: no setting beats Flat MC; r = 2 (46.2%) and twice the iterations (45.2%) help, at 1.5 and 1.9 times the time; fewer root plans and less exploration don't. The cause found: the searcher's moves were keyed by what came after them, so a plan ending in a draw split into one root child per draw and never built up visits. The searcher's moves are now move nodes with outcome nodes under them, and matched worlds and choosing by mean are options. Matched worlds is what closes the gap (50.9%, and 52.0% with r = 2 at 42 iterations); choosing by mean adds nothing on top. The agent's defaults are now matched worlds, r = 2 and 48 iterations. 48 was meant to match Flat MC's time but takes about 1.16 times it: the time per game is the fair comparison (see below), and 42 iterations match it, so the default is now 42* | ✅ |
| 8.7 | **Sign-off:** IS-MCTS against Flat MC on every deck pairing with invariant checks; win rate against the time budget (4 points); speed; known limits. *16,000 games at 42 iterations, about Flat MC's time: IS-MCTS 51.3% (50.5% to 52.1%); the first run at 48 iterations, with invariant checks, 51.8% and no problems found. Win rate against the time budget: 40.8% at 12 iterations, 46.7% at 24, 51.3% at 42, 57.1% at 96. Details and known limits below* | ✅ |

How each setting moved IS-MCTS against Flat MC in 8.6, on the same 1,600 seeded games (`--agents ismcts,flatmc
--games 100 --no-check`, release, 16 threads), one change at a time from 64 iterations, C = 0.7, r = 1, 16 plans at the
searcher's nodes and a 25-node opponent and-tree keeping 4. The last column but one compares the time each agent spends
searching a game: IS-MCTS's search against Flat MC's and-tree and playouts (its choices included), as both search
the same 28 to 30 decisions a game. (Earlier notes compared seconds a search with 0.106 s a searched decision for
Flat MC, which isn't like for like; by time per game, step 8.5's 64 iterations took about 1.2 times Flat MC's time.)

| IS-MCTS | Score (95% CI) | Lowest pairing | Search s a game | s a search | Games/s |
|---|---|---|---|---|---|
| Defaults (step 8.5) | 41.6% (39.2% to 44.0%) | 15% ns-zoroark vs mega-greninja | 3.22 | 0.103 | 2.6 |
| 6 plans at the searcher's nodes | 39.5% (37.1% to 41.9%) | 21% dragapult vs basic-box, ns-zoroark mirror | 3.26 | 0.109 | 2.5 |
| r = 2 | 46.2% (43.8% to 48.7%) | 23% ns-zoroark vs basic-box | 4.97 | 0.162 | 2.0 |
| C = 0.3 | 40.9% (38.6% to 43.4%) | 20% ns-zoroark vs mega-greninja | 3.30 | 0.104 | 2.5 |
| 128 iterations (twice the time) | 45.2% (42.8% to 47.6%) | 15% ns-zoroark vs mega-greninja | 5.98 | 0.191 | 1.9 |
| Move and outcome nodes (the new tree) | 43.5% (41.1% to 45.9%) | 20% ns-zoroark vs mega-greninja | 3.17 | 0.105 | 2.7 |
| New tree, matched worlds | 50.9% (48.5% to 53.4%) | 30% ns-zoroark vs mega-greninja | 2.98 | 0.099 | 2.8 |
| New tree, final by mean (3 visits) | 45.1% (42.7% to 47.6%) | 20% ns-zoroark vs mega-greninja | 3.07 | 0.101 | 2.9 |
| New tree, r = 2 at 42 iterations (the same time) | 46.4% (43.9% to 48.8%) | 24% ns-zoroark vs mega-greninja | 2.66 | 0.093 | 3.1 |
| Matched worlds, r = 2 at 42 iterations | 52.0% (49.6% to 54.4%) | 27% ns-zoroark vs basic-box | 2.58 | 0.090 | 3.1 |
| Matched worlds, final by mean | 50.2% (47.7% to 52.6%) | 27% dragapult vs basic-box | 2.95 | 0.097 | 2.9 |
| Matched worlds, final by mean, r = 2 at 42 iterations | 50.4% (48.0% to 52.9%) | 30% ns-zoroark vs mega-greninja | 2.56 | 0.090 | 3.1 |

- **Every root plan gets too few visits to tell them apart.** With 16 plans, 64 iterations give each 2 to 4 visits,
  each from a different world and a different playout seed. Flat MC gives each of its 16 plans 4 playouts on the
  same 4 worlds with the same seeds, so its comparison between plans is much less noisy. IS-MCTS's most visited plan,
  on that few visits, is close to chance among the good ones.
- **Fewer plans is worse, not better** (39.5%): as Flat MC found in 7.6, the and-tree's lower plans are often the best
  once the opponent's reply is played out, and 6 plans cuts them.
- **Less exploration changes nothing** (40.9%): with so few visits, UCB1 barely gets to exploit.
- **Looking further and searching longer both help**, but neither reaches even with twice Flat MC's time.
- **The new tree helps a little** (43.5%, from 41.6%, inside the noise): a plan ending in a draw now keeps its
  visits on one move node, with 1,340 outcome nodes a game under the moves.
- **Matched worlds is the big change** (50.9%, even with Flat MC for the first time, at 1.12 times its time): the
  n-th visit to every root move sees the same world and playout seed, so the root compares plans as Flat MC does.
  The worst pairing rises from 20% to 30%. Cache hits (1,646 to 383 a game) and single-action nodes (982 to 289) fall
  because matched mode takes the root's moves from a list instead of asking for them every iteration, so they don't
  show a shallower search; opponent and-trees, which are only run below the root, barely change (591 to 557). (An
  earlier version of this note read the drop as the descent stopping at the root.)
- **Choosing by mean and r = 2 at 42 iterations each help a little** (45.1%, 46.4%), on the unmatched tree.
- **On matched worlds, r = 2 adds a little and choosing by mean nothing:** 52.0% with r = 2 at 42 iterations (the best
  so far, its interval just touching 50%), 50.2% and 50.4% with the mean. Matched visits already make the means
  comparable, so the most visited move is the best mean.
- **Time against Flat MC, search seconds a game over Flat MC's:** the new tree 1.22, matched worlds 1.12, final by
  mean 1.25, r = 2 at 42 iterations 1.07, matched with r = 2 at 42 iterations 1.02, matched with the mean 1.15, all
  three 1.01. So matched worlds with r = 2 at 42 iterations is the setting that wins at Flat MC's time.

**Sign-off run (2026-11-23):** `pokebot-sim --agents ismcts,flatmc --games 1000 --no-check` on all 16 deck pairings
(16,000 games, every seed once with IS-MCTS in each seat; IS-MCTS with its defaults, 42 iterations, C = 0.7, r = 2,
matched worlds, 16 plans at its nodes and a 25-node opponent and-tree keeping 4; Flat MC as signed off in M7). The
invariant checks ran on the first sign-off, the same 16,000 games at 48 iterations (2026-11-22): **no problems
found**.

| Measure | Result |
|---|---|
| IS-MCTS's score against Flat MC | **51.3%** (95% CI 50.5% to 52.1%): 8,206 won, 7,794 lost, 0 drawn |
| By who went first | IS-MCTS first 54.5% (53.4% to 55.6%), Flat MC first 48.1% (47.0% to 49.2%) |
| Every deck matchup, both seatings averaged (2,000 games each) | 49.5% (the basic-box mirror) to 53.6% (dragapult and ns-zoroark); 8 of the 10 above 50%, the other two the basic-box and ns-zoroark mirrors (49.5%, 49.6%) |
| Every deck pairing | 26.9% (ns-zoroark against mega-greninja) to 75.2% (mega-greninja against ns-zoroark): a pairing mostly measures the decks, so the matchup averages above are the fair comparison |
| How IS-MCTS won | Prize cards 7,388, no Pokémon left 411, Flat MC's deck ran out 407 |
| How IS-MCTS lost | Prize cards 6,846, its own deck ran out 518, no Pokémon left 430 |
| Per game | 28.5 searches of 42.0 iterations, 1,772 tree nodes (899 of them outcome nodes), 0.45 wins found by the or-tree, 80 searcher and-trees with 202 cache hits, 292 opponent and-trees, 161 single-action nodes, 0.04 moves unplayable in a world, no matched iterations skipped |
| Time per game (on 16 busy threads) | IS-MCTS 2.98 s searching (1.73 s playouts, 0.59 s its own and-trees, 0.55 s the opponent's); Flat MC 2.79 s (0.15 s and-tree, 2.64 s playouts): IS-MCTS 1.07 times Flat MC's time (1.02 in 8.6; this run stalled once, 800 games taking 861 s) |
| Speed, IS-MCTS against Flat MC | about 2.8 games/s on 16 threads without checks, 2.0 with them |
| The first sign-off, 48 iterations, with invariant checks | 51.8% (51.0% to 52.5%) at 1.16 times Flat MC's time; no problems found |

**Win rate against the time budget,** IS-MCTS's iterations varied against the default Flat MC (1,600 games each,
`--games 100 --no-check`, except the 16,000 of the sign-off at 42):

| IS-MCTS iterations | Time against Flat MC's | Score (95% CI) | Games/s |
|---|---|---|---|
| 12 | 0.26 | 40.8% (38.4% to 43.2%) | 3.8 |
| 24 | 0.56 | 46.7% (44.3% to 49.1%) | 4.0 |
| 42 (the sign-off) | 1.07 | 51.3% (50.5% to 52.1%) | 2.8 |
| 96 | 2.50 | 57.1% (54.6% to 59.5%) | 1.6 |

The score rises steadily with the budget, about 5 to 6 points a doubling, and crosses 50% near Flat MC's own time.
At a quarter of Flat MC's time IS-MCTS still wins 40.8%.

**Known limits:**

- **The lead at equal time is small:** 1.3 points, its interval's lower end 0.5 points above even, at 1.02 to 1.07
  times Flat MC's time. Flat MC beat the Planner by 22.9 points.
- **The sign-off's seeds include 8.6's.** A 1,600-game run plays the first 50 of a sign-off's 500 seeds a pairing, with
  the same agent seeds, so the runs that chose the defaults are 10% of it (`slot_of` in `arena.cpp`). Without those
  1,600 games, taking them from 8.6's run at the same settings: 51.2% (50.4% to 52.0%) over 14,400.
- **Where the lead comes from isn't measured.** Matched worlds made the largest difference in 8.6, which is a change
  at the root, but how much the tree below adds was never isolated. (An earlier version said most iterations stop at
  the root, from the fall in cache hits; that fall comes from how the root's moves are looked up, see 8.6.)
- **The curve varies only IS-MCTS's budget.** Flat MC stays at its default throughout, so the curve shows what
  IS-MCTS gains from time against a fixed opponent, not how the two compare at every budget.
- **Going second it is slightly behind** (48.1%), and ns-zoroark is still the weakest deck for both agents.
- **Times are noisy:** both long runs stalled once, which inflates both agents' seconds.

## M9 — Model 3: endgame solver ✅

**Done when:** the solver's values match a brute-force search without pruning on small positions, and IS-MCTS with the
solver beats IS-MCTS alone over at least 1,000 seeded games per deck pairing in both seat orders.

The endgame solver replaces sampling with enumeration where the game is short enough: an expectiminimax search over
turn segments, the searcher's plans at OR nodes, the opponent's at AND-min nodes and chance outcomes at AND-chance
nodes, pruned by alpha-beta and Star1, with iterative deepening under a budget.

**What the proposal's trigger meets (2026-11-23).** The proposal enumerates every world at the root and triggers when
there are at most N_end. A scratch measurement over 400 Greedy-against-Greedy games on all 16 pairings counted the
worlds at each main-phase decision, by card definition, with the opponent's true list (so a lower bound):

| Smaller prize count left | Decisions a game | Median worlds | With at most 10⁴ worlds | Opponent's hidden hand (median) |
|---|---|---|---|---|
| 1 | 4.2 | 10¹⁰ | 0.7% | 8 cards |
| 2 | 5.9 | 10¹¹ | 0.2% | 8 cards |
| 3 | 6.8 | 10¹² | 0.1% | 8 cards |

The opponent's hidden hand gives about 10⁸ of them; the split of the searcher's own deck and Prize cards about 10².
65% of games reach 2 or fewer prizes for someone, so endgames are common, but exact enumeration at the root almost
never applies.

**Decisions (2026-11-23):**
- **The root:** the worlds are enumerated when there are at most N_end, and otherwise M are sampled from the
  [deck model](src/agents/flat_mc/deck_model.md). The trigger is the smaller prize count at most P_end (2 to start,
  about 10% of decisions). Only plans playable in every root world are compared. So the solver is exact over chance
  and the next few turns, and a sampled search over the opponent's hand: the change from the proposal.
- **Chance in the tree:** a one-card draw branches on each distinct card left in the deck (about 14 late in a game),
  by moving a copy to the top of the world's deck; coin flips through a scripted `Rng`, replaying the action for each
  heads and tails sequence; big draws (a Supporter's 6 or 7 cards) sampled, or the segment ends there. Cards whose
  deck position the player knows stay where they are.
- **Moves and depth:** the and-tree's top plans as in IS-MCTS (16 for the searcher, 4 for the opponent at 25 nodes),
  not every end state; depth in turn segments, deepened one at a time, e at the depth limit, children ordered by e.
  One turn and the opponent's draw and reply cost about 16 × 14 × 2 ms ≈ 0.45 s a world before pruning, so pruning
  and ordering decide what's affordable; 9.1 measures it.
- **Budgets:** a node budget, so runs repeat, set so a solver decision costs about an IS-MCTS search; only about 10%
  of decisions qualify, so even three times that adds about 20% a game.
- **The agent:** `EndgameAgent` (`--agent endgame`) runs the proposal's decision procedure: the solver if the
  trigger holds, else the or-tree's win, else IS-MCTS; IS-MCTS too if the first depth doesn't finish.
- **Checks:** brute-force agreement on small positions; an endgame suite of positions at 2 or fewer prizes saved
  from IS-MCTS games, each played to the end many times by each agent; nodes pruned by alpha-beta and Star1. The
  sign-off, `endgame,ismcts` at about 2.5 to 2.8 games/s, takes about 1.5 to 2 hours for 16,000 games.

| Step | Builds | Status |
|---|---|---|
| 9.1 | **Endgame lab:** world counts, chance branching and the solver's cost on positions from real games (`pokebot-sim --endgame-lab N`); P_end, M and the depth chosen. *The scratch measurement above is its first table. The [endgame lab](src/harness/endgame_lab.md) is built (`pokebot-sim --endgame-lab N [--every K] [--endgame-prizes P]`): world counts at every main-phase decision, and at qualifying ones a two-segment search in the true world without pruning, with its value, size and time. `--endgame-lab 160` (15,754 decisions, 95 s): 63% of games reach 2 or fewer Prize cards, 10.4 decisions a game (11%); median worlds 10¹¹, at most 10⁴ in under 1%; an unpruned two-segment tree a world has 15 plans (6.6 of them stopping mid-turn after a draw), 107 draw branches, 425 replies, and takes 0.15 s at the median, 0.27 on average, 0.7 at p90. Chosen: P_end = 2; worlds enumerated at most 16, else M = 4 sampled from the deck model (2, 4 and 8 tuned in 9.6); iterative deepening 1 to 2 segments; one-card draws for either player enumerated by card, 2 or 3 cards enumerated up to 32 outcomes and sampled at 8 beyond, 4 or more sampled; a plan stopping on a draw is a chance node and then the searcher's next segment from a 25-node and-tree keeping 4, counting one level of depth; coin flips scripted; a node budget set from 9.3's pruning* | ✅ |
| 9.2 | **Chance outcomes:** a boundary action's outcomes in a world, with probabilities: one-card draws by card, coin flips through a scripted `Rng`, big draws sampled; tests that the probabilities add up to 1 and match brute force over deck orders. *[`chance_outcomes`](src/agents/endgame/chance.md) and `Rng::script_flips` are built: flips enumerated by script up to 8; the deck's touched top cards found by a probe run and every combination placed there (up to 32, else 8 sampled), for both players, so ending the turn branches on the opponent's draw; a shuffle before a draw sampled; outcomes that look the same to both players merged, so a search is one outcome. `count_placements` moved here from the lab. `GameState` grows by the script's 8 bytes: a clean rebuild. All tests pass* | ✅ |
| 9.3 | **Expectiminimax with alpha-beta and Star1:** OR, AND-min and AND-chance nodes over and-tree plans, values in [−1, 1], depth in turn segments, children ordered by e; the same values as a brute-force search on small positions; nodes pruned counted. *[`expectiminimax`](src/agents/endgame/expectiminimax.md) is built: and-tree plans at main-phase decisions (16 at 300 nodes at the root, 4 at 25 deeper), every legal action at other decisions without using depth, chance through `chance_outcomes`, fail-soft alpha-beta and Star1; move lists and chance seeded from each node's state hash, so the search with and without pruning sees the same tree. Tests compare it with the search without pruning on sample cards and real-deck endgames. All tests pass* | ✅ |
| 9.4 | **Root and iterative deepening:** worlds enumerated (at most N_end) or sampled from the deck model, only plans playable in every world, deepening under a node budget, the deepest finished iteration's best plan. *The [endgame solver](src/agents/endgame/endgame_solver.md) is built: M worlds always sampled (enumeration dropped: under 1% of endgame decisions have at most 10⁴ worlds); the root a chance node over the worlds under the OR node over the plans, with Star1; plans dropped unless playable in every world; depth 1, 2, … under a shared `max_nodes`, each depth in the last one's order. The endgame lab now runs the solver too (`--endgame-worlds M`, `--endgame-depth D`), to set the budget. `--endgame-lab 160` (332 solves, no budget): every solve with more than one plan reached depth 2, in about 39,000 nodes and 1.2 s at the median (3 s mean, 6 s p90), with about 1,300 and-trees a solve (the time isn't profiled); Star1 cut 4,600 to 8,500 outcomes a solve, alpha-beta under 100 moves; no root plan dropped. The default budget is 10,000 nodes (about 1 s); 2,500, 10,000 and 40,000, M = 2 or 4 and 8 or 16 root plans are tuned in 9.6, with IS-MCTS also given the solver's time at the same decisions* | ✅ |
| 9.5 | **Endgame agent:** the solver, else the or-tree's win, else IS-MCTS; `--agent endgame`, its settings and counters. *[`EndgameAgent`](src/agents/endgame/endgame_agent.md) is built: at a main-phase decision with either player at 2 or fewer Prize cards, unless IS-MCTS is partway through a plan, the solver's best plan, followed like IS-MCTS's; on no plan, a fallback on IS-MCTS, which decides everything else (its or-tree's win first). `--agent endgame`, `--endgame-prizes`, `--endgame-worlds`, `--endgame-depth`, `--endgame-nodes`; the solver's moves follow the `--mcts-*` plan settings. First match, `--agents endgame,ismcts --games 100 --no-check` (1,600 games): **45.9% (43.5% to 48.4%)**, so the solver as it stands makes IS-MCTS worse, at 1.8 games/s. Per game: 2.9 endgame decisions, all solved (no fallbacks), 1.7 of them only to depth 1 within 10,000 nodes and 1.2 to depth 2; 2.5 s in the solver, so the agent takes about 1.6 times IS-MCTS's time. Going first 50.5%, second 41.4%. 9.6 looks for why* | ✅ |
| 9.6 | **Endgame suite and tuning:** positions at 2 or fewer prizes saved from IS-MCTS games; the solver's win rate from them against IS-MCTS alone; P_end, M, the depth and the budget tuned at matched time *The first runs, in the table below: playing only depth-2 solves gets back to even (50.1%); every solve at depth 2, at five times the time, is no better (49.3%); so more depth-2 solves and more time didn't help. The cause isn't isolated (see below). The default plays only depth-2 solves (`min_depth` 2); the endgame suite and further tuning are left until after M10* | ✅ |
| 9.7 | **Sign-off:** `endgame` against `ismcts` on every deck pairing, at least 1,000 games each, with invariant checks; brute-force agreement; nodes pruned by alpha-beta and Star1; speed; known limits *16,000 games with checks, below: 50.5% (49.7% to 51.3%), no problems found. The brute-force test is met; beating IS-MCTS isn't* | ✅ |

How the endgame agent did against IS-MCTS in 9.6, on the same 1,600 seeded games (`--agents endgame,ismcts --games 100
--no-check`, release, 16 threads), one change at a time from the defaults (the solver at 2 or fewer Prize cards, 4
worlds, depth 2, 10,000 nodes, its plan played if the solve reached depth 1). IS-MCTS spends about 2.6 to 3.4 s a
game searching throughout.

| Endgame agent | Score (95% CI) | Solves a game: played, depth 2, fallbacks | Solver s a game | Games/s |
|---|---|---|---|---|
| Defaults (step 9.5) | 45.9% (43.5% to 48.4%) | 2.9, 1.2, 0 | 2.5 | 1.8 |
| Only depth-2 solves played (`--endgame-min-depth 2`) | 50.1% (47.7% to 52.6%) | 1.4, 1.4, 2.0 | 2.4 | 2.0 |
| No node budget (`--endgame-nodes 0`): every solve depth 2 | 49.3% (46.9% to 51.8%) | 4.0, 4.0, 0 | 15.5 | 0.6 |
| The solver at 1 Prize card (`--endgame-prizes 1`) | 48.9% (46.4% to 51.3%) | 1.2, 0.6, 0 | 0.8 | 2.7 |

- **Depth-1 solves are what lost** (45.9% to 50.1% without them): scoring a plan by e after the searcher's own turn,
  with no reply, is worse than IS-MCTS's search with its two-turn playouts.
- **A full depth-2 solve is only as good as IS-MCTS** (49.3%), at about six times the default's solver time. Why isn't
  isolated: no run changed the leaf evaluation, the number of worlds, the plans kept (16 and 4) or the opponent's model.
  e at the leaves is the leading guess; 4 sampled worlds, the opponent's 4 cheap plans, and an opponent who sees its
  hand in each world are others.
- **Solving only at the last Prize card changes little** (48.9%), with depth-1 solves still played.

**Sign-off run (2026-11-24):** `pokebot-sim --agents endgame,ismcts --games 1000` on all 16 deck pairings (16,000
games, every seed once with the endgame agent in each seat; the solver at 2 or fewer Prize cards left, 4 worlds from the
deck model, depth up to 2 within 10,000 nodes, its plan played only if the solve reached depth 2, IS-MCTS as signed off in
M8 otherwise and on the other side), with the invariant checker after every action: **no problems found**.

| Measure | Result |
|---|---|
| The endgame agent's score against IS-MCTS | **50.5%** (95% CI 49.7% to 51.3%): 8,077 won, 7,923 lost, 0 drawn |
| By who went first | the endgame agent first 53.5% (52.4% to 54.6%), IS-MCTS first 47.5% (46.4% to 48.6%) |
| Every deck matchup, both seatings averaged (2,000 games each) | 48.6% (dragapult and ns-zoroark) to 51.5% (basic-box and ns-zoroark); 8 of the 10 at 50% or above |
| How it won | Prize cards 7,097, IS-MCTS's deck ran out 522, no Pokémon left 458 |
| How it lost | Prize cards 6,703, its own deck ran out 784, no Pokémon left 436 |
| Per game | 3.35 endgame decisions: 1.34 solves reached depth 2 and were played (3.9 plan actions), 2.01 stopped at depth 1 and went to IS-MCTS; about 24,900 solver nodes, 950 and-trees, 4,350 outcomes cut by Star1 and 68 moves by alpha-beta; no root plan dropped |
| Time per game (on 16 busy threads) | the solver 3.66 s and the agent's IS-MCTS 3.84 s, against IS-MCTS's 4.22 s: about 1.8 times its time |
| Speed, the endgame agent against IS-MCTS | about 1.4 games/s on 16 threads with invariant checks (the run stalled once: 800 games took 3,935 s) |

**Against the done-when:** the solver's values match a brute-force search without pruning on small positions and on
real-deck endgames (the 9.3 and 9.4 tests); **IS-MCTS with the solver does not beat IS-MCTS alone**: 50.5%, its interval
including even, at about 1.8 times the time. M9 is closed with that result rather than tuned further: 9.6's four runs
didn't beat even, and didn't isolate why.

**Known limits:**

- **The sign-off's seeds include 9.6's** (see M8's limits). Without those 1,600 games, taking them from 9.6's
  `min_depth` 2 run (whose totals 10.1 repeated exactly): 50.5% (49.7% to 51.3%) over 14,400, still a tie.
- **Why the solver only ties isn't known.** With every solve at depth 2 and about six times the solver's time, the
  agent still ties IS-MCTS (49.3% in 9.6). The leaf evaluation e was the leading guess, but with M10's learned e on both
  sides the agent still ties (49.7% in 10.7), so this better e doesn't change it. No run isolated the 4 sampled worlds, the
  plans kept, or the opponent's model.
- **Depth-1 solves lose** (45.9% when they were played): a plan scored by e after the searcher's own turn, with no reply.
  Within 10,000 nodes, 60% of solves stop at depth 1, so the solver plays in only 1.3 decisions a game.
- **It is slow:** a full depth-2 solve averages about 3 s and 39,000 nodes, with about 1,300 and-trees. Where the time
  goes isn't profiled: about 2 ms an and-tree (the budget lab's mid-game figure) would account for most of it, but the
  chance outcomes' replays and hashing aren't timed.
- **The root is sampled, not enumerated** (worlds with at most 10⁴ placements are under 1% of endgame decisions), so with
  the opponent's hand unknown the solver is a sampled search over it, not exact; and the opponent in each world sees its
  hand, so its replies are the best against that world.
- **It loses more games by deck-out** (784 against 522 won that way).
- **Not built:** the endgame suite of saved positions (9.6), choices outside the main phase, and a transposition table.

## M10 — Learned evaluation function ✅

**Done when:** learned θ beats hand-set θ: IS-MCTS with the learned weights against IS-MCTS with the hand-set ones, over
at least 1,000 seeded games per deck pairing in both seat orders, on seeds no earlier run used.

M10 fits e's weights to the results of self-play games. tanh(z) = 2σ(2z) − 1, so logistic regression of the winner on
the features makes e an estimate of the expected result. Better prediction doesn't guarantee better play, so the arena
decides.

**What the code showed (2026-11-24):**
- **A negative weight turns off `f_bound`:** `upper_bound` returns 1 if any θ is negative, so the and-tree searches
  differently on the same budget.
- **The weights are copied into many option structs.** `IsMctsAgentOptions::weights` only reaches its Greedy setup rule;
  the playouts, macro-actions and endgame solver hold their own. No sim flag sets any of them.
- **Tuning seeds have been inside sign-off seeds** (M8's and M9's limits).

**Decisions (2026-11-24):**
- **Measure e before fitting it:** log loss and calibration on held-out games, by prizes left and by deck, against a
  prizes-only baseline and the hand θ with its best single scale. If the hand θ is close to the fitted one, new weights
  can't gain much: go to new features, or stop and report that.
- **Training games:** IS-MCTS self-play, where e is used (16,000 games, an estimated 1.5 to 2.5 hours); whether a fit on
  cheaper Planner games predicts IS-MCTS games as well is measured.
- **Deck strength is a confound:** the fit has a term for each deck pairing and for going first, dropped at play time,
  and is repeated with each deck left out.
- **Choosing weights (2026-11-24, before any fit was run):** by deck-adjusted held-out loss, since raw loss rewards using
  features to recognise the deck, and within a game the pairing never changes. The θ ≥ 0 fit unless the free fit's
  advantage is outside its 95% interval. On to 10.5 if the chosen fit beats the scaled hand θ outside the 95% interval
  and isn't clearly worse on any left-out deck; otherwise on to new features.
- **10.5's runs (2026-11-24, before any was run):** seeds from 2,000,001, used by no other step. Learned θ is
  `data/weights/m10/fitted-nonneg.txt`. The main run is IS-MCTS with learned θ against hand θ at C = 0.7, 6,400 games;
  Greedy and Flat MC the same way, to see where a difference starts; and IS-MCTS with `hand-scaled-terms.txt` against hand
  θ, to tell a better θ from a better scale. C is re-checked by playing learned θ with another C against learned θ with
  0.7, and changed only if that wins outside its 95% interval. The search seconds of both sides are reported, since the
  weights can change how much work 42 iterations are. If the main run isn't above 50% outside its interval, the Greedy and
  Flat MC runs say where to look before anything is tuned.
- **10.6's rules (2026-11-25, before any run):** the candidates are late prizes, Knock Out threat, payable damage, evolved,
  board HP and deck-out. Whose turn it is is dropped as a feature: at the end of a full-depth playout it would be almost a
  constant, and elsewhere mostly a price on ending the turn. One passes if, with θ ≥ 0 on parts 1 to 4, its adjusted loss is at least
  0.0010 below that of the played features, outside the 95% interval, and it isn't clearly worse on any left-out deck.
  The best that passes is added and the rest are measured again on top of it, until none passes. If none passes at first,
  10.6 ends without new features; if all six together gain much more than any one, that's reported and decided then.
  Otherwise the chosen ones get their `upper_bound` terms and one match, IS-MCTS with the new θ against
  `fitted-nonneg`, 6,400 games on seeds from 2,100,001: the new θ is kept only if its whole interval is above 50%, and, if it
  takes more than 10% more search time, if its lead is also more than that time is worth at 5.5 points a doubling.
  Candidates that aren't kept are removed.
- **10.7's rules (2026-11-25, before any run):** the final θ is `fitted-evolved-deck_out.txt` as the match tested it, not
  refitted. First the cleanup is checked: 320 games of it against `fitted-nonneg` (`--games 20 --seed 2000001`) must repeat
  173 to 147 with every count the same. Part 5 is looked at once, with `hand-scaled-terms` (the reference), `hand`,
  `fitted-nonneg` and the final θ: if the final θ isn't better than the reference outside its interval, adjusted, stop and look into
  it before the sign-off. The sign-off is IS-MCTS with the final θ against hand θ, 16,000 games with invariant checks on
  seeds from 3,000,001: M10 passes if the whole interval is above 50% and no invariant breaks. Each deck, each mirror and
  both sides' search times are reported, and, if the final θ takes over 10% longer, what that's worth at 5.5 points a
  doubling. Then `endgame` against `ismcts`, both with the final θ, 6,400 games on seeds from 3,100,001: the endgame agent
  counts as better only if its whole interval is above 50%.
- **Split by seed,** never by position or game: a game's positions share one result, and the arena plays each deal
  twice (pairing (i, j) in one seat and (j, i) in the other), so 16,000 games hold 8,000 deals.
- **The data:** IS-MCTS against itself with the hand weights, seeds 1,000,001 to 1,000,500 in five parts of 100 seeds
  (3,200 games each). Parts 1 to 4 are for fitting and choosing, by cross-validation over seeds; part 5 is kept aside
  and looked at once, for the final weights. (Its counts and going-first score were checked with the others; nothing
  about e was.) The files are in `replays/self-play/`, which git ignores: keep a copy, since a change to how agents
  play or to the cards means they can't be made again.
- **Negative weights:** fitted free and with θ ≥ 0; the non-negative one unless the free one is clearly better, and then
  `upper_bound` is extended first.
- **Features:** the 10 refitted first; then whose turn it is, turns until deck-out, and prizes with turns to Knock Out,
  one at a time, kept if held-out loss improves.
- **Judging:** separate seeds for training, tuning (at least 6,400 games, about ±1.2 points) and one sign-off. Rescaling θ
  changes e's spread, so UCB's C is re-checked.

| Step | Builds | Status |
|---|---|---|
| 10.1 | **Weights in one place:** θ files, one function that sets every copy in an agent, weights per side in the sim; default weights play exactly today's games. *[Weights files and `set_weights`](src/agents/evaluation/weights.md) are built: `data/weights/hand.txt` holds the hand-set weights, `set_weights` sets every copy in the Planner's, Flat MC's, IS-MCTS's and the endgame agent's options, and the sim has `--weights`, `--weights-a` and `--weights-b`. All tests pass. `--agents ismcts,ismcts --games 20 --no-check --seed 300001` (320 games) plays identically with `--weights-b data/weights/hand.txt` and without: the score, every pairing and every count. With `data/weights/prizes.txt` (only the prizes weight) for B on the same seeds, the hand weights score 69.1% (63.8% to 73.9%), so each side gets its own weights. `--agents endgame,ismcts --games 100 --no-check` repeats 9.6's `min_depth` 2 run from before 10.1 exactly: 802 to 798, every pairing and every count* | ✅ |
| 10.2 | **Self-play data:** whole games saved, and from them the features at each turn's start and one later main phase, with game, player, prizes left and result. *[`self_play`](src/harness/self_play.md) is built: games saved as replays, one a line, and positions worked out by playing them back, so new features need no new games. A turn's end is the next turn's start, so a random later main phase is recorded instead. `--save-games`, `--check-games`. All tests pass. A trial of 320 games (`--seed 310001`, 3.2 games/s) played back to its results: 160 different deals, 6,612 turn starts and 5,896 later main phases. The data run: 16,000 games in five parts, each 3,200 games of 100 seeds and 1,600 different deals, all played back to their saved result, none without positions. 622,664 positions (329,470 turn starts, 293,194 later), 498,549 of them in parts 1 to 4. The player who went first scored 53.3%, 50.9%, 52.9%, 51.6% and 49.8% by part, 51.7% in all* | ✅ |
| 10.3 | **Evaluation lab:** held-out log loss and calibration of e against the baselines. *[`evaluation_lab`](src/harness/evaluation_lab.md) is built: a coin flip, prizes with a fitted scale, the hand θ as played and with a fitted scale, scored on seeds they weren't fitted on (10 folds by seed), with errors clustered by seed. `--evaluate FILE,...`. How far to go needs the fitted θ, so that decision moves to the end of 10.4. All tests pass. The results on parts 1 to 4 are below* | ✅ |
| 10.4 | **The fit:** logistic regression in C++ (Newton, L2), pairing and first-player terms, optional θ ≥ 0, leave-one-deck-out; recovers known weights on made-up data. *[`weight_fit`](src/harness/weight_fit.md) is built, with terms for each pairing, going first and the player to move, and the [evaluation lab](src/harness/evaluation_lab.md) scores every model raw and with those terms fitted on the training folds (deck-adjusted), and on decks left out of the fit. `--evaluate` compares the fits and `--save-weights` saves them. All tests pass, and the raw losses repeat 10.3's from a different solver. The results on parts 1 to 4 are below. `data/weights/m10/fitted-nonneg.txt` goes to 10.5* | ✅ |
| 10.5 | **Arena tuning:** learned against hand θ in Greedy, Flat MC and IS-MCTS; C re-checked. *The main run: IS-MCTS with learned θ scores 52.4% (51.2% to 53.6%) against hand θ over 6,400 games; the hand θ only rescaled scores 49.0% (47.8% to 50.3%), and C stays at 0.7. `--mcts-c-a` and `--mcts-c-b` are built for the C run. Greedy with learned θ loses: 42.9% (42.2% to 43.7%). Flat MC with learned θ scores 52.4% (51.1% to 53.6%). Learned θ takes about 9% more search time a game in IS-MCTS. Playouts played with hand θ instead make no measurable difference (50.3%), so learned θ plays them. The runs are below* | ✅ |
| 10.6 | **New features,** one at a time, with their `upper_bound` terms. *The measuring: six candidate features that weighed 0 until chosen, a lab mode for them (both removed since), and `evaluate` skipping features that weigh 0. All tests pass, and 320 games of 10.5's main run repeat exactly. Round 1 adds `evolved` and round 2 `deck_out`, and in round 3 nothing passes: `data/weights/m10/fitted-evolved-deck_out.txt`. The rounds are below. Their [`upper_bound` terms](src/agents/planner/and_tree.md#the-upper-bound) are built, and all tests pass. The match: the new θ scores 55.1% (53.9% to 56.4%) against `fitted-nonneg` over 6,400 games, with 1% more search time, so it's kept. The four candidates that weren't kept are removed, the older weights files list the two new features with 0, and 454 tests are left* | ✅ |
| 10.7 | **Sign-off:** IS-MCTS with learned against hand θ, 16,000 games with invariant checks on unused seeds; the endgame agent re-tested; known limits. *`--score-weights` in the [evaluation lab](src/harness/evaluation_lab.md#saved-weights) is built, for part 5. All 454 tests pass, the cleanup check repeats exactly, and part 5 agrees with the folds: the final θ's adjusted loss is 0.0208 ± 0.0049 below the reference's. The sign-off passes: 57.6% (56.8% to 58.4%) against hand θ over 16,000 games, with no invariant problems. The endgame agent with the final θ still ties IS-MCTS: 49.7% (48.5% to 50.9%) over 6,400 games. Results and known limits below* | ✅ |

**What 10.3 measured (2026-11-24):** parts 1 to 4, 12,800 games, 498,549 positions, 400 seeds; 95% intervals by seed.

| Model | Held-out log loss | Change from hand, scaled |
|---|---|---|
| Coin flip | 0.6931 | +0.0949 ± 0.0049 |
| Prizes, scaled (θ = 1.718) | 0.6049 | +0.0066 ± 0.0024 |
| Hand, as played | 0.6260 | +0.0277 ± 0.0032 |
| Hand, scaled (× 0.588) | 0.5983 | 0 |

- **The hand θ is overconfident:** predictions of 10% to 40% win 12 to 16 points more often than predicted, and those of
  70% to 90% about 10 points less. A scale of 0.588 fits best. As played it predicts worse than prizes alone.
- **The other nine features add little as weighted:** prizes alone give 93% of the scaled hand θ's gain over a coin flip.
- **Before any prize is taken (41% of positions) the hand θ predicts worse than a coin flip:** 0.7056 scaled and 0.7469 as
  played, against 0.6931. With prizes taken, the scaled hand θ beats prizes alone at every count. One scale can't fix
  this: scaled, it's still overconfident in the middle (30% to 40% wins 42%) and underconfident at the top (80% to 90%
  wins 91%), while prizes alone fit a larger prizes weight (1.718 against 1.175).
- **The player to move at a turn's start does better than predicted,** by 4.6 ± 0.1 points (0.6 later in the turn). With
  prizes alone it's 2.2 and 1.7.
- **Decks:** mega-greninja wins 13.3 ± 1.0 points more than predicted and ns-zoroark 7.2 ± 0.9 less. These mix the decks'
  strength with any misreading of their positions; the lab can't tell them apart.

**What 10.4 measured (2026-11-24):** parts 1 to 4 again, with the change from the hand θ scaled with terms; 95% intervals by
seed. 24.7 s on 16 threads.

| Model | Raw loss | Change | Adjusted loss | Change |
|---|---|---|---|---|
| Coin flip | 0.6931 | +0.0947 ± 0.0051 | 0.6565 | +0.0880 ± 0.0048 |
| Prizes, scaled | 0.6049 | +0.0065 ± 0.0025 | 0.5823 | +0.0138 ± 0.0023 |
| Hand, as played | 0.6260 | +0.0276 ± 0.0030 | 0.5896 | +0.0211 ± 0.0028 |
| Hand, scaled with terms (× 0.614) | 0.5984 | 0 | 0.5685 | 0 |
| Fitted | 0.5941 | −0.0043 ± 0.0029 | 0.5572 | −0.0113 ± 0.0017 |
| Fitted, θ ≥ 0 | 0.5934 | −0.0050 ± 0.0025 | 0.5580 | −0.0106 ± 0.0016 |
| Fitted, L2 1,000 | 0.5918 | −0.0066 ± 0.0025 | 0.5574 | −0.0111 ± 0.0016 |
| Fitted, L2 10,000 | 0.5922 | −0.0062 ± 0.0016 | 0.5641 | −0.0045 ± 0.0014 |
| Fitted without terms | 0.5816 | −0.0168 ± 0.0021 | 0.5639 | −0.0047 ± 0.0018 |

- **The fits beat the scaled hand θ,** and on every deck left out of the fit: by 0.0072 to 0.0098 (free) and 0.0073 to
  0.0091 (θ ≥ 0), each outside its interval. By the rules above, M10 goes on to 10.5. This is prediction, not play.
- **Raw loss does reward recognising decks:** the fit without terms has the best raw loss and a worse adjusted loss than
  the fits with terms.
- **The free fit against the θ ≥ 0 fit, paired** (`--reference fitted-nonneg`): better by 0.0007 ± 0.0003 adjusted, outside
  its interval, so the rule above picks the free fit. But on decks left out it's worse on basic-box (+0.0016 ± 0.0008) and
  dragapult (+0.0011 ± 0.0008) and better on ns-zoroark (−0.0006 ± 0.0005); mega-greninja is level (−0.0003 ± 0.0005). The
  rule said nothing about this comparison on left-out decks.
- **The choice: θ ≥ 0, against the rule, after seeing the result.** The free fit does no better on left-out decks; its
  conditions weight swings from −0.07 to −0.39 with the deck left out; its gain is 0.0007 of about 0.011; and a negative
  weight turns off `f_bound`, which would mix a weaker search into every arena test.
- **L2 doesn't help** (1,000 is level with none, 10,000 is worse), so none is used.
- **What changed, against the scaled hand θ:** energy 0.18 to 0.97, deck 0.31 to 0.62, hand 0.12 to 0.28, turns to Knock
  Out 0.49 to 0.29, and ready attackers 0.25 to about 0. Conditions (−0.29 ± 0.11) and prizes at risk (−0.16 ± 0.05) come
  out negative, which is the opposite of what they were written for; the cause isn't known.
- **Before any prize is taken, even the fit is little better than a coin flip** (0.6533 against 0.6595, adjusted), so that's
  where new features have most to add: 41% of the positions.
- **The player to move's term isn't the same all game:** with one term, predictions are 1.5 points too high before any
  prize is taken and 1.3 to 1.9 too low at 1 to 3 prizes. A whose-turn feature in 10.6 may need to depend on prizes.

**What 10.5 measured:** IS-MCTS's defaults on both sides, 400 games per pairing, `--seed 2000001`, no invariant checks.

| A against B | A's score |
|---|---|
| IS-MCTS, learned θ against hand θ, C = 0.7 (2026-11-24) | **52.4%** (51.2% to 53.6%): 3,355 won, 3,045 lost |
| IS-MCTS, hand θ scaled (× 0.614) against hand θ, C = 0.7 | 49.0% (47.8% to 50.3%): 3,139 won, 3,261 lost |
| IS-MCTS, learned θ, C = 0.4 against C = 0.7 | 50.2% (49.0% to 51.5%): 3,215 won, 3,185 lost |
| Flat MC, learned θ against hand θ | **52.4%** (51.1% to 53.6%): 3,351 won, 3,049 lost |
| IS-MCTS, learned θ, A's playouts played with hand θ against learned θ throughout (2026-11-25) | 50.3% (49.1% to 51.5%): 3,220 won, 3,180 lost |
| Greedy, learned θ against hand θ, 1,000 games per pairing | **42.9%** (42.2% to 43.7%): 6,869 won, 9,130 lost, 1 drawn |

- **Greedy gets worse with learned θ,** with every deck (by 10 to 17 points), while IS-MCTS gets better. It loses 4,019 games by
  running out of cards against 2,653 won that way; 41.7% of Greedy's games end in deck-out, against 6.4% of the IS-MCTS
  run's. The cause isn't known: θ was fitted on IS-MCTS's positions, and Greedy scores every single action with it. Flat
  MC's and IS-MCTS's playouts are played by Greedy with the same θ, so learned θ at the leaves with hand θ playing the
  playouts is worth testing.
- **Flat MC gains as much as IS-MCTS,** so the gain doesn't need the tree search. By the deck it plays: dragapult +12.1 ± 3.7,
  ns-zoroark +4.4 ± 3.7, basic-box +2.1 ± 3.7, mega-greninja +0.5 ± 3.7. The mega-greninja mirror is 45.8% (40.9% to 50.6%).
  It takes about 6% more search seconds a game (3.73 against 3.51), and its and-trees expand 19% more nodes.
- **The gain isn't from the scale:** rescaling the hand θ alone doesn't help at C = 0.7, though it was most of the gain in
  log loss. What's left is the other change in θ, its direction.
- **C stays at 0.7:** 0.4 doesn't beat it outside its interval. Only 0.4 was tried.
- **By the deck it plays,** learned less hand θ over the same 1,600 games each: dragapult +12.1 ± 3.8 points, ns-zoroark +6.3
  ± 3.7, basic-box +3.0 ± 3.8, mega-greninja −2.0 ± 3.8. In the mega-greninja mirror it scores 43.8% (39.0% to 48.6%).
- **Time:** 3.87 search seconds a game against 3.90, but the machine stalled for about 800 s during the run, which adds
  about 2 s a game to one side or the other at random, so this run doesn't settle it. By counts, learned θ makes 3% more
  iterations a game and 7% more playout actions an iteration, and 6% fewer opponent and-trees. The scaled run had no stall
  (2.59 s on both sides, 3.1 games/s); the C run had several, so its times mean nothing either.
- **Time, measured again** on 320 of the main run's games (`--games 20`, 2.4 games/s, no stall; 173 to 147): 3.44 search
  seconds a game for learned θ against 3.16, 9% more, of which 3% is more searched decisions. At M8's 5 to 6 points a
  doubling that's worth about 0.6 to 0.7 of the 2.4-point lead: an estimate, not a run at matched time. The lead doesn't
  come from time alone.
- **Playout weights (2026-11-24, before the run):** `policy_weights` in [`playout`](src/agents/flat_mc/playout.md) and
  `--playout-weights-a`/`-b` let a side's playouts be played with other weights than they're scored with. One run, IS-MCTS
  with learned θ on both sides and A's playouts played with hand θ, 6,400 games on the same seeds: hand θ plays the
  playouts from then on only if A wins outside its 95% interval. All tests pass.
- **Playout weights, the result (2026-11-25):** 50.3% (49.1% to 51.5%), so by the rule learned θ goes on playing the
  playouts. Greedy with learned θ being much weaker doesn't show up as weaker search through the playouts: an effect of
  more than about 1.5 points either way is ruled out, a smaller one isn't. By the deck it plays, A less B: basic-box +3.6,
  ns-zoroark +1.2, mega-greninja −0.8, dragapult −1.3, all within about ± 3.8. A loses more games by running out of cards
  (223 against 148) and wins more on prizes (2,819 against 2,697); the cause isn't known. Its playouts are 5% shorter (17.4
  actions an iteration against 18.3) and it spends 3% less search time a game (2.89 s against 2.98). The run had slow
  stretches (2.7 games/s), so the seconds are rough.

**What 10.6 measured:** parts 1 to 4, θ ≥ 0, adjusted loss and its change from the played features; 95% intervals by seed.

Round 1 (2026-11-25; 30.9 s on 16 threads). Without a candidate the loss is 0.5580, as in 10.4.

| Candidate | Change | Weight | Left-out basic-box | dragapult | mega-greninja | ns-zoroark | Passes |
|---|---|---|---|---|---|---|---|
| `late_prizes` | −0.0004 ± 0.0003 | 0.52 | −0.0001 | −0.0005 | −0.0007 | −0.0005 | no: under 0.0010 |
| `knockout_threat` | −0.0017 ± 0.0004 | 0.42 | −0.0012 | −0.0022 | −0.0025 | +0.0007 ± 0.0007 | too close to call, and not the best |
| `payable_damage` | −0.0021 ± 0.0006 | 0.36 | −0.0012 | −0.0029 | −0.0033 | +0.0062 ± 0.0013 | no: worse on ns-zoroark |
| `evolved` | **−0.0083 ± 0.0016** | 0.82 | −0.0091 | −0.0102 | −0.0086 | −0.0084 | **yes, and the best: added** |
| `board_hp` | −0.0040 ± 0.0010 | 0.40 | +0.0103 ± 0.0035 | −0.0039 | −0.0047 | −0.0047 | no: worse on basic-box |
| `deck_out` | −0.0043 ± 0.0008 | 1.15 | −0.0034 | −0.0064 | −0.0026 | −0.0053 | yes |
| all six | −0.0162 ± 0.0020 | | −0.0086 | −0.0201 | −0.0156 | −0.0099 | not a candidate |

Round 2 (2026-11-25), on top of `evolved`: without another candidate the loss is 0.5496.

| Candidate | Change | Weight | Left-out basic-box | dragapult | mega-greninja | ns-zoroark | Passes |
|---|---|---|---|---|---|---|---|
| `late_prizes` | −0.0004 ± 0.0003 | 0.52 | +0.0002 | −0.0003 | −0.0007 | −0.0005 | no: under 0.0010 |
| `knockout_threat` | −0.0012 ± 0.0003 | 0.37 | −0.0011 | −0.0016 | −0.0017 | +0.0013 ± 0.0006 | no: worse on ns-zoroark |
| `payable_damage` | −0.0011 ± 0.0004 | 0.26 | −0.0011 | −0.0014 | −0.0016 | +0.0060 ± 0.0011 | no: worse on ns-zoroark |
| `board_hp` | −0.0022 ± 0.0007 | 0.32 | +0.0049 ± 0.0025 | −0.0019 | −0.0022 | −0.0020 | no: worse on basic-box |
| `deck_out` | **−0.0039 ± 0.0008** | 1.10 | −0.0031 | −0.0058 | −0.0020 | −0.0046 | **yes, the only one: added** |
| all five | −0.0079 ± 0.0013 | | +0.0005 ± 0.0033 | −0.0099 | −0.0070 | −0.0015 | not a candidate |

Round 3 (2026-11-25), on top of `evolved` and `deck_out`: without another candidate the loss is 0.5457.

| Candidate | Change | Weight | Left-out basic-box | dragapult | mega-greninja | ns-zoroark | Passes |
|---|---|---|---|---|---|---|---|
| `late_prizes` | −0.0004 ± 0.0003 | 0.56 | +0.0002 | −0.0004 | −0.0008 | −0.0006 | no: under 0.0010 |
| `knockout_threat` | −0.0013 ± 0.0003 | 0.38 | −0.0012 | −0.0016 | −0.0017 | +0.0014 ± 0.0006 | no: worse on ns-zoroark |
| `payable_damage` | −0.0011 ± 0.0004 | 0.27 | −0.0011 | −0.0014 | −0.0016 | +0.0062 ± 0.0011 | no: worse on ns-zoroark |
| `board_hp` | −0.0021 ± 0.0007 | 0.31 | +0.0047 ± 0.0025 | −0.0019 | −0.0021 | −0.0020 | no: worse on basic-box |
| all four | −0.0040 ± 0.0009 | | +0.0036 ± 0.0029 | −0.0041 | −0.0049 | +0.0031 ± 0.0015 | not a candidate |

- **Chosen: `evolved` (0.808) and `deck_out` (1.105),** a loss of 0.5457 against 0.5580, more than refitting the ten weights
  gained (0.0106). With the other four added, left-out basic-box and ns-zoroark get worse.
**The match (2026-11-25):** IS-MCTS's defaults, `fitted-evolved-deck_out` for A against `fitted-nonneg` for B, 400 games per
pairing, `--seed 2100001`, no invariant checks. **A scores 55.1% (53.9% to 56.4%): 3,529 won, 2,871 lost.**

- **The rule is met:** the whole interval is above 50%, and the search takes 2.39 s a game against 2.36, 1% more, well inside
  10%, so the new θ is kept.
- **It's bigger than the refit's gain** (52.4% against hand θ), on different seeds and against a different opponent. The two don't add up
  to a score against hand θ, which is 10.7's to measure.
- **By the deck it plays,** A less B over the same 1,600 games each, all ± about 3.8: dragapult +14.1, mega-greninja
  +13.6, ns-zoroark +9.4, basic-box +4.0. Basic-box's is just outside its error; its mirror is 50.0% (45.1% to 54.9%).
- **The mirrors:** mega-greninja 60.8% (55.9% to 65.4%), dragapult 58.8% (53.9% to 63.5%), ns-zoroark 55.5% (50.6% to
  60.3%). Mega-greninja's was the one `fitted-nonneg` did badly in against hand θ; against hand θ with the new θ it's
  for 10.7.
- **It runs out of cards less:** A loses 98 games that way and wins 193. All games: 4.5% end so, against 6.4% in 10.5's main run.
- **What won't be known from this:** which of the two features does it, as only both were played together.

- **Most of the early-game gain is `evolved`'s:** before any prize is taken the loss goes from 0.6533 to 0.6434, and to 0.6432 with all
  six. A coin flip with terms is 0.6595. This is still prediction, not play.
- **Skipping features that weigh 0 changes nothing but time:** `--games 20` of 10.5's main run is 173 to 147 again, with every
  count the same. Learned θ, with three weights of 0, now takes 2.03 search seconds a game against hand θ's 2.18, 7% less,
  where it took 9% more. (The machine was faster this time, so only the two sides of a run compare.)

**What 10.7 measured:**

- **The cleanup check (2026-11-25):** 173 to 147 again, with every count the same as before the cleanup.
- **Part 5, looked at once (2026-11-25):** 3,200 games, 124,115 positions, weights as saved; the change is from
  `hand-scaled-terms`, 95% by seed.

| Weights | Raw loss | Change | Adjusted loss | Change | Change in the folds of parts 1 to 4 (10.4, 10.6) |
|---|---|---|---|---|---|
| `hand-scaled-terms` | 0.5950 | 0 | 0.5578 | 0 | 0 |
| `hand` | 0.6222 | +0.0272 ± 0.0055 | 0.5775 | +0.0197 ± 0.0049 | +0.0211 ± 0.0028 |
| `fitted-nonneg` | 0.5913 | −0.0037 ± 0.0044 | 0.5480 | −0.0097 ± 0.0031 | −0.0106 ± 0.0016 |
| `fitted-evolved-deck_out` | 0.5639 | −0.0311 ± 0.0051 | 0.5370 | **−0.0208 ± 0.0049** | −0.0228 (0.5457 against 0.5685) |

- **The rule is met,** and the gains on part 5 are within its intervals of those in the folds, so there's no sign that the
  features were chosen by luck. Both fitted weights gain slightly less here, but the differences are well
  inside the intervals.
- **Better on every deck's games,** by 0.0163 to 0.0256, each outside its interval, and at every prize count. Before any prize is
  taken it's 0.6415 against 0.6549, still not far from a coin flip.
- **As played it isn't perfectly calibrated:** predictions of 20% to 50% win 4 to 6 points more often than predicted, and
  those of 70% and more 3 to 4 more. The result averages 2.1 points above the prediction, as the player to move does better than e
  says, and that isn't in e.

**The sign-off (2026-11-25):** IS-MCTS's defaults, the final θ for A against hand θ for B, 1,000 games per pairing,
`--seed 3000001`, with invariant checks. **A scores 57.6% (56.8% to 58.4%): 9,214 won, 6,786 lost,** and no problems were found.

- **The rule is met, and so is M10's done-when:** the whole interval is above 50% on seeds no earlier run used, and no invariant
  broke.
- **It isn't bought with time:** 42 iterations a search on both sides, and the search takes 2.50 s a game against 2.69, 7% less,
  with 5% more searches a game. Less time, so there's nothing to estimate at 5.5 points a doubling.
- **By the deck it plays,** A less B over the same 4,000 games each, all ± about 2.4: dragapult +25.4, ns-zoroark +13.7,
  mega-greninja +13.4, basic-box +8.3.
- **By matchup,** both seatings averaged (2,000 games, ± about 2.0; mirrors 1,000, ± about 3.0):

| | basic-box | dragapult | mega-greninja | ns-zoroark |
|---|---|---|---|---|
| basic-box | 50.7% | 56.0% | 53.3% | 56.7% |
| dragapult | | 68.0% | 63.7% | 63.1% |
| mega-greninja | | | 54.6% | 55.1% |
| ns-zoroark | | | | 52.5% |

- **All 10 matchups are above 50%;** only the basic-box mirror (47.6% to 53.8%) and the ns-zoroark mirror (49.4% to 55.6%) have
  50% in their intervals.
- **The mega-greninja mirror is won:** 54.6% (51.5% to 57.7%), where `fitted-nonneg` scored 43.8% in IS-MCTS and 45.8% in Flat MC
  against hand θ in 10.5.
- **Dragapult gains most, and basic-box least.** Every Pokémon in basic-box is a Basic, so `evolved` never counts its
  own side, which may be why; it wasn't tested.
- **By who went first:** A first 61.1%, B first 54.1% for A. Going first is worth about 3.5 points to either side.
- **Deck-outs:** A loses 335 games that way and wins 545. All games: 5.5% end so.

**The endgame run (2026-11-25):** `--agents endgame,ismcts`, the final θ on both sides, 400 games per pairing,
`--seed 3100001`, no invariant checks. **The endgame agent scores 49.7% (48.5% to 50.9%): 3,181 won, 3,219 lost.**

- **It doesn't count as better:** the interval includes 50%, as in M9 (50.5%, 49.7% to 51.3%). It isn't shown to be worse either.
- **By matchup,** both seatings averaged, 47.5% (dragapult and mega-greninja, ± 3.4) to 51.2% (the basic-box mirror, ± 4.9); every one's
  interval includes 50%.
- **Time:** the solver 4.24 s a game and the agent's IS-MCTS 4.54 s, against IS-MCTS's 4.75 s: 1.85 times its time, as in M9
  (1.8). The machine slowed to about a third of its speed during the run, so only the two sides compare.
- **The solver plays about as little as before:** 3.35 decisions a game, of which 1.23 reached depth 2 and were played (1.34 in M9);
  the other 2.12 went to IS-MCTS. This θ doesn't get more solves to depth 2 within 10,000 nodes.
- **It still runs out of cards more:** it loses 178 games that way and wins 118 (784 and 522 in M9).
- **What it says about e:** both sides use the same θ, so this doesn't measure how much e matters to the solver, only that learning it
  doesn't help the solver more than IS-MCTS.

**Against the done-when:** met by the sign-off (57.6%, on unused seeds).

**Known limits:**

- **Four decks only,** the same four that θ was fitted on, though on unused seeds. How far it holds for a deck it wasn't fitted on is
  only known from the leave-one-deck-out losses, not from play.
- **θ is fitted once, on hand-θ games.** Games played by IS-MCTS with the final θ weren't used to refit it, so what a second round
  would add isn't known.
- **Only IS-MCTS and the endgame agent were played with the final θ,** and the endgame agent only against IS-MCTS with the same θ.
  Flat MC was played with `fitted-nonneg` only (52.4%), and Greedy lost with it (42.9%), for reasons that aren't known.
- **C was re-checked with `fitted-nonneg`** (0.4 against 0.7), not with the final θ, and no other value was tried.
- **Which of the two new features does it isn't known from play,** since both were always played together; their losses were
  −0.0083 for `evolved` and −0.0039 for `deck_out` (10.6).
- **One card was wrong:** Greninja ex (30C 21) in mega-greninja was played as a Basic, though it's a Stage 2 (see M3b), in every run and all
  the training games. After the fix the final θ scores 57.8% (56.6% to 59.0%) against hand θ over 6,400 games, so the
  result stands; θ wasn't refitted.
- **Not perfectly calibrated:** the player to move does better than e says, by 2.1 points on average, and before any prize is taken the loss
  (0.6415, adjusted) is not far from a coin flip's with the same terms (0.6595 in the folds).
- **`ready_attackers`, `conditions` and `prizes_at_risk` are fitted at 0** (θ ≥ 0), so they're skipped. In 10.4's free fit
  of the ten features, `conditions` and `prizes_at_risk` came out negative, for reasons that aren't known, so their 0 is the
  limit rather than a fitted value; `ready_attackers` came out about 0.

**The default (2026-11-25):** the final θ, `learned_weights()`, is now IS-MCTS's and the endgame agent's default. Greedy, the
Planner and Flat MC keep the hand-set weights, since they haven't been played with the final θ. The earlier IS-MCTS and endgame
runs above that name no weights files were played with the hand-set ones: `--weights data/weights/hand.txt` repeats them.

- **All 456 tests pass,** and three checks of 320 games repeat earlier or matching runs in everything but the times: IS-MCTS on the
  default against `fitted-nonneg` (`--seed 2000001`) plays 10.6's games; the endgame agent on the default plays the same games as with
  the weights file (`--seed 3100001`); and `fitted-nonneg` against `hand.txt` (`--seed 2000001`) plays 10.5's.
- **The score alone wouldn't have shown it:** 173 to 147 is both `fitted-nonneg` against hand θ and the final θ against
  `fitted-nonneg` on these seeds, by chance. The games differ (19.9 and 18.5 turns).

## M5 onward

The full definitions of M5–M11 (what each builds and its "done when" test) are in the project
proposal. The table at the top lists them in order. Two changes since:

- **M7 starts with an opponent deck model:** a library of archetype lists (`data/archetypes/`), a
  belief over them updated from the cards seen (Bayes' rule, with a small chance for cards no list
  explains), and dealing the opponent's unseen cards from a list drawn from that belief. Lists can
  only use cards the engine has effects for, so M3b widens what they can hold.
- **M11's opponent inference** becomes the learned, stronger version of that model.

## Long-term goals (back burner)

Not scheduled yet; recorded 2026-11-19 so the milestones above keep them possible.

- **Speed (added 2026-11-26).** Faster simulation and search, as a goal of its own apart from M3b's coverage work: more games
  for training and arenas, and quicker agents for live play. Measured as 3b.4's fix was, two builds playing the same games
  one after the other, since single timed runs on one machine vary by up to 3 times. M3b's sign-off check (the old decks at
  most 10% slower than at M10) still applies.
- **Many more decks and variants.** A much larger pool of decks, and variants of each, for the
  models to train and be tested on (M10's learned evaluation especially), so they get stronger in
  general rather than tuned to four lists. It also gives the archetype library (M7) more to tell
  apart. It depends on M3b: lists can only use cards the engine has effects for.
- **A client to play our agents in real time:** a desktop app or a TypeScript web interface, likely
  one web UI talking to the C++ engine through a JSON protocol of observations, actions and
  analysis, wrapped as a desktop app if wanted. It showcases every model on the agent ladder.
- **Agents fast enough for live play.** The best agents should take their turns in a few seconds.
  The arena keeps iteration budgets so results repeat; live play adds time budgets, which suit the
  anytime searches (IS-MCTS, the endgame solver's iterative deepening). M8 should profile IS-MCTS's
  expansions first: an and-tree search per new node (about 23 ms mid-game at M7) is its likely limit.
- **An analysis mode, like a chess engine's:** while you play, or while watching two agents, an agent
  shows its best plans for the player to move, with their values and how sure it is (playout or
  visit counts, a range). It works from that player's observation only, as every agent does, so it
  gives advice a player could really use. Its values become win chances once M10 trains the
  evaluation on game results. Agents therefore report their ranked candidates, not just one choice,
  from Flat MC (M7) on.

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
