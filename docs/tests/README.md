# `tests/` — test suite

**Framework:** [Catch2](https://github.com/catchorg/Catch2) v3.8.1, downloaded automatically by CMake.
**Executable:** `pokebot_tests` (links `pokebot_lib` and `pokebot_import_lib`).

No test touches the network. Code that would use the network is tested through a fake HTTP client.

## Running

```bash
ctest --preset debug
```

Run one group, with Catch2's full output:

```bash
./build/debug/tests/pokebot_tests.exe "[convert]"
```

List every test:

```bash
./build/debug/tests/pokebot_tests.exe --list-tests
```

## Files

| File | Tag | Tests | Covers |
|---|---|---|---|
| [`test_decklist.cpp`](../../tests/test_decklist.cpp) | `[decklist]` | 5 | Parsing names with spaces/apostrophes/`{D}`, Windows line endings, total mismatch, error line numbers, both project decklists load with 60 cards |
| [`test_tcgdex.cpp`](../../tests/test_tcgdex.cpp) | `[tcgdex]` | 5 | Padded card ids, unknown sets and numbers, cache then offline reuse, retries after empty/failed responses, giving up, offline with no cache |
| [`test_convert.cpp`](../../tests/test_convert.cpp) | `[convert]` | 7 | Pokémon ex with `×` damage and a free attack, damage modifiers surviving save and reload, Mega ex worth 3 Prizes with no Tera check, ACE SPEC detection, Basic Energy type from its name, Special Energy mislabeled as Normal, rejecting unknown data |
| [`test_deck_validator.cpp`](../../tests/test_deck_validator.cpp) | `[deck_validator]` | 9 | One test per deck-building rule, all printings of a name counting together, Basic Energy exemption, unknown cards, several problems at once, and every real deck in `data/decks/` (skipped without generated data) |
| [`test_card_db.cpp`](../../tests/test_card_db.cpp) | `[card_db]` | 6 | JSON round trip, effects files adding rules and attack effects, typo'd attack names rejected, `missing_effects` list, `no_effect` Abilities not missing, bad card data rejected |
| [`test_engine_state.cpp`](../../tests/test_engine_state.cpp) | `[engine]` | 10 | `FixedVector`, `Rng` determinism and shuffles, independent `GameState` copies, card ownership, `CardTable` numbering and illegal decks |
| [`test_engine_setup.cpp`](../../tests/test_engine_setup.cpp) | `[setup]` | 10 | Opening hands, determinism, turn order, Prize cards and turn 1, merged choices and the Bench limit, mulligan bonus draws, 1,000 random real-deck setups, illegal actions, `apply_legal` playing the same game as `apply` |
| [`test_engine_turns.cpp`](../../tests/test_engine_turns.cpp) | `[turns]` | 5 | Ending the turn, drawing, alternating turns, flag resets, deck-out |
| [`test_engine_actions.cpp`](../../tests/test_engine_actions.cpp) | `[actions]` | 10 | Benching, one Energy per turn, evolution timing, retreating (with and without a choice, free, not allowed), 200 random real-deck games (the Bench never over `rules::bench_size`) with a summary of how they ended |
| [`test_engine_attacks.cpp`](../../tests/test_engine_attacks.cpp) | `[attacks]` | 11 | Cost matching, no attack on turn 1, damage, Weakness and Resistance, copied attacks using the attacker's type, Pokémon ids, Knock Outs and promotion, ex / Mega ex Prize cards, every way to win including draws |
| [`test_rules.cpp`](../../tests/test_rules.cpp) | `[rules]` | 4 | Rulebook answers for HP and Knock Outs, type, costs, Energy, Prize cards, turn limits and evolution timing |
| [`test_engine_events.cpp`](../../tests/test_engine_events.cpp) | `[events]` | 5 | Events of an attack that Knocks Out (in order), of benching, evolving, attaching and retreating; history of the last two turns; the extra-attack decision |
| [`test_effect_program.cpp`](../../tests/test_effect_program.cpp) | `[effects]` | 13 | Compiling effects: damage first, `search_deck`, `may` and `if` expansion, placed damage, Trainer programs, conditions, `special_condition`, rejected mistakes, every filter field, errors naming the attack |
| [`test_engine_effects.cpp`](../../tests/test_engine_effects.cpp) | `[effects]` | 10 | Running effects: draw, search, filters, choosing nothing, yes/no, Energy to hand, switching, discard and draw, Knock Outs waiting for the effect, copying a game mid-effect |
| [`test_engine_trainers.cpp`](../../tests/test_engine_trainers.cpp) | `[trainers]` | 12 | Items resolving and going to the discard pile, one Supporter per turn and none on turn 1, Stadiums replacing each other, one Tool per Pokémon, `"requires"` and unwritten cards, Ultra Ball, Boss's Orders, Poffin to the Bench, `if`, Judge, Night Stretcher's filter |
| [`test_engine_conditions.cpp`](../../tests/test_engine_conditions.cpp) | `[conditions]` | 11 | Asleep and Paralyzed blocking attacks and retreats, an attack applying Asleep, which conditions replace or stack, Active only, retreating and evolving clearing them, Poison and Burn counters, a Poison Knock Out, recovering on heads, Paralysis timing, confusion failing on tails |
| [`test_engine_damage.cpp`](../../tests/test_engine_damage.cpp) | `[damage]` | 11 | Damage formulas: × per card in hand, + per Benched Pokémon, flip until tails, an "if you do" bonus, discarding Energy from several Pokémon, × per damage counter; Red Card; Cipher's deck order; the compile rules for formulas |
| [`test_engine_targets.cpp`](../../tests/test_engine_targets.cpp) | `[targets]` | 10 | Weakness on the Active but not the Bench, Tera Bench protection, two different targets, the damage-counter spread order, damage by the target's counters, Flamebody Cannon, Tuck Tail and promoting, copying an N's attack (and with nothing to copy), Crushing Hammer into the owner's discard pile |
| [`test_engine_lasting.cpp`](../../tests/test_engine_lasting.cpp) | `[lasting]` | 9 | "Can't use that attack" and "can't attack" next turn, retreating ending an effect, "can't retreat" for one turn, the Item lock, a damage bonus only against an Active ex, this-turn effects ending, attack Knock Outs versus Checkup ones in history, the compile checks |
| [`test_engine_passive.cpp`](../../tests/test_engine_passive.cpp) | `[passive]` | 7 | A Tool changing only its holder, a Stadium reaching both players' N's Pokémon, Skyliner on your Basic Pokémon only, Fairy Zone adding Weakness and Shred ignoring it, Binding Mochi only while Poisoned, passive lists counting as written, the compile checks |
| [`test_engine_abilities.cpp`](../../tests/test_engine_abilities.cpp) | `[abilities]` | 10 | Once per turn and again next turn, Active only, one per name, requirements and costs, damage counters Knocking Out at once, the top 2 cards, shuffling the Active away, moving counters, a filtered switch that Poisons, a Stadium action once per turn |
| [`test_engine_triggers.cpp`](../../tests/test_engine_triggers.cpp) | `[triggers]` | 8 | A trigger asking yes/no then searching, not offered as an Ability to use, one per name, unmet requirements, discarding the Stadium to its owner, switching in and moving Energy, Risky Ruins on Basic non-{D} Pokémon (Poffin too), Poffin not counting as "played", trigger names |
| [`test_engine_energy.cpp`](../../tests/test_engine_energy.cpp) | `[energy]` | 7 | A Special Energy's usual Energy and its holder-dependent Energy, one card providing several, a cost a greedy match would miss, Retreat paid in Energy provided, the discard choice stopping once paid, the end-of-turn discard, the load checks |
| [`test_engine_attach.cpp`](../../tests/test_engine_attach.cpp) | `[attach]` | 6 | Energy from the discard pile to the only matching Benched Pokémon, each of 2 targets getting its own Energy from the deck and Poison for the Active, no Poison without it, up to 2 Energy only with more Prize cards left, moving Energy between your Pokémon, the compiled loop |
| [`test_engine_custom.cpp`](../../tests/test_engine_custom.cpp) | `[custom]` | 7 | Rare Candy straight to Stage 2, not on the first turns or a Pokémon played this turn; Transformation Tome needing two, keeping Energy and damage, and Knocking Out a Basic with less HP; Area Zero's 8 spaces with a Tera Pokémon and the discard down to 5 when it leaves; the load checks |
| [`test_engine_knowledge.cpp`](../../tests/test_engine_knowledge.cpp) | `[knowledge]` | 9 | The open zones and hands, face-down setup, a revealed search known until shuffled away, a search that doesn't reveal, searching the deck showing all of it to its owner, a card picked up from the discard pile, looking at the top of the deck, a mulligan showing the hand, and 100 random games checking every visible card is recorded and no Prize card is known |
| [`test_observation.cpp`](../../tests/test_observation.cpp) | `[observation]` | 14 | Your own hand and only the size of the opponent's, definitions only for seen cards, the deck and Prize cards as one sorted list, a revealed card in the opponent's view, cards on the bottom of the deck shown where they are, the decision stack and an effect's chosen cards hidden from the opponent, face-down setup, games differing only in hidden cards looking the same (and a visible change showing), random games on the sample cards and the real decks naming only cards the viewer knows, `describe(observation)`, `play_game` showing each agent its own view, and the actions offered naming only cards the deciding player knows |
| [`test_invariants.cpp`](../../tests/test_invariants.cpp) | `[invariants]` | 5 | The checker catches duplicated, missing and misplaced cards, missed Knock Outs, bad evolutions and Energy, and a missing Active |
| [`test_agents.cpp`](../../tests/test_agents.cpp) | `[agents]` | 3 | Random choices stay legal and cover every option, the biased agent attacks, seeds repeat (given an empty observation) |
| [`test_world.cpp`](../../tests/test_world.cpp) | `[world]` | 7 | A world built from an observation observes back to it, passes the invariant checker and offers the same actions (sample cards and real decks, every action after setup, both players); your deck and Prize cards dealt from the same cards in different ways; the opponent's unseen cards as `unknown_card()`; attaching, benching and retreating looking the same as in the game; a card the opponent is discarding from hand stood in for from their hand; no world during setup |
| [`test_evaluation.cpp`](../../tests/test_evaluation.cpp) | `[evaluation]` | 7 | Finished games scored 1, −1 or 0; an even position about 0; a Prize card, damage and Energy scoring higher and Poison lower, with the expected feature values; a player choosing a new Active scored by their healthiest Benched Pokémon; the hand counting up to 7 cards and the deck up to 30; random games with exactly opposite features in [−1, 1]; on the real decks, a world giving the same evaluation as the game |
| [`test_greedy.cpp`](../../tests/test_greedy.cpp) | `[greedy]` | 7 | Taking a Knock Out; attaching Energy rather than ending the turn, and scoring it higher; an Ability that discards then draws, with an empty deck, scored by its discard and not used; going first and putting down the Basics with the most HP during setup; the same seeds playing the same game; whole sample-card games keeping every invariant; beating the random agent in at least 3 of 4 real-deck games from either seat |
| [`test_arena.cpp`](../../tests/test_arena.cpp) | `[arena]` | 6 | The Wilson interval's known values; a tally from either seat with draws as half; every pairing played equally with each seed in both seats and the totals adding up; kept games playing back to the same result; the same results with 1 and 4 threads; an agent's failure stopping the arena |
| [`test_action_class.cpp`](../../tests/test_action_class.cpp) | `[planner]` | 4 | Attaching, benching, evolving and retreating deterministic; ending the turn, attacking and a Knock Out (without chance) final; a draw, a search, a coin-flip attack and ending the turn while Asleep uncertain; on the real decks, the same class in two worlds, the same view after a deterministic action and the same value after a final one |
| [`test_state_hash.cpp`](../../tests/test_state_hash.cpp) | `[planner]` | 4 | Equivalent orders of the same actions reaching the same state and hash; the hash changing with what decides the game but not with the random number generator or the hand's order; on the real decks, equal hashes meaning equal states; a closed set holding each state once |
| [`test_plan.cpp`](../../tests/test_plan.cpp) | `[planner]` | 4 | `apply_plan` in order; a final plan's value e after it with any number of samples; a draw worth e after it in every sampled world, and a search scored after taking a Pokémon; a coin-flip attack averaged over heads and tails, the same for the same state and seed |
| [`test_or_tree.cpp`](../../tests/test_or_tree.cpp) | `[planner]` | 6 | Attach and attack for a Knock Out; evolve, attach, attack; a free retreat then the other Pokémon's attack; a win by the last Prize card and k Prize cards; no plan when none exists, the closed set saving nodes, and the budget; on the real decks, every plan found legal and taking a Prize card |
| [`test_and_tree.cpp`](../../tests/test_and_tree.cpp) | `[planner]` | 6 | The best plan evolving, attaching and attacking; the best worth at least any one plan-ending action; top-K best first and every end state; the closed set and f_bound expanding fewer nodes for the same best value; the upper bound at least e and 1 for a winning Knock Out; on the real decks, the bound at least e, the best plan legal and worth its value, and f_bound not changing it |
| [`test_replay.cpp`](../../tests/test_replay.cpp) | `[replay]` | 10 | Saving and loading, playing back to the same `GameState` and ending (including `play_game`'s own final state) (sample and real decks), illegal actions and wrong results caught, bad files rejected, actions described to each player without the cards they don't know, `print_replay` |
| [`test_client.cpp`](../../tests/test_client.cpp) | `[client]` | 10 | The human agent: the typed number taken, anything else asked again, `b` and `q`, a single choice not asked, no repeated board partway through an effect, a whole game through `play_game`; actions described from an observation reading the same as from the game (sample and real decks); the opponent's Pokémon named as theirs; `print_replay` stopping before each turn |
| [`test_coverage.cpp`](../../tests/test_coverage.cpp) | `[coverage]` | 3 | Every card in our decks has its effects written, every search whose text says "reveal" sets `reveal`, and 2,000 biased random games use every attack (copied ones too), every Ability you choose to use, every Trainer, every Stadium action and every Special Energy in our decks, listing whatever was never used |
| [`test_stress.cpp`](../../tests/test_stress.cpp) | `[stress]` | 3 | 1,000 biased and 1,000 uniform random real-deck games with invariant checks, and every state and action can be described. More than 300 of the biased games must end by Knock Out (random players burn through their decks with draw and search Trainers, so deck-outs are common) |

**315 tests in total.** Tests that need the real card data skip themselves if `data/cards/generated/`
doesn't exist yet; run `pokebot-import` first.

## Every card is tested

Milestone 3's sign-off asks for a test per card. It's covered three ways:

- **Chosen effects** (attacks, Abilities you use, Trainers, Stadium actions, Special Energy): the
  coverage test plays them in random games with the real decks, and the stress tests check every
  invariant while they do. Most also have a test of their own with a made-up card. A few can't happen
  with our decks, and the test lists them with the reason: Chien-Pao's Icicle Loop costs {W}{W}{C},
  but `basic-box` has 1 Water Energy (Chien-Pao is there for Snow Sink).
- **Triggered and always-on effects** aren't chosen, so each has its own test: the triggers in
  `test_engine_triggers.cpp`, Tools, Stadiums and passive Abilities in `test_engine_passive.cpp`,
  Area Zero in `test_engine_custom.cpp`, and Neo Upper and Ignition in `test_engine_energy.cpp`.
- **Mechanics no deck card uses** are tested with made-up cards, so they work when a card needs them:

| Mechanic | Test |
|---|---|
| Burned | "Poisoned and Burned put damage counters on at Pokemon Checkup", "Asleep and Burned are each removed on heads" |
| Paralyzed | "Asleep and Paralyzed Pokemon can't attack or retreat", "Paralyzed wears off at the Checkup after its owner's next turn" |
| More than one attack a turn (Festival Lead) | "an extra attack offers that Pokemon's attacks or ending the turn" |
| A Bench over the limit | "when Area Zero leaves play, a player over the limit discards down to 5" |

## Helpers worth knowing

- **`FakeHttp`** (`test_tcgdex.cpp`): queue responses per URL with `queue(path, body)`. An empty
  body simulates TCGdex's "200 with empty body" glitch; `"!error"` simulates a failed request.
  `calls` counts requests, which is how the cache tests prove nothing was downloaded.
- **`TempDir`** (`test_tcgdex.cpp`): a fresh temporary folder with a `sets.json`, deleted when the
  test ends.
- **`support/test_cards.hpp`**: build made-up cards and decklists: `basic_pokemon`, `evolution`,
  `basic_energy`, `attack`, and `deck({{count, number}, ...})` for set `TST`.
- **`support/real_decks.hpp`**: `load_real_decks()`, the project's real card data and decklists, or
  nothing if `pokebot-import` hasn't been run (the test then skips itself).
- **`support/test_game.hpp`**: arrange exact positions without breaking card accounting:
  `play_setup`, `take_from_deck` (deck, then hand), `put_in_hand`, `set_active`, `clear_bench`,
  `add_to_bench`, `attach_from_deck`, `end_turns`, plus `offers`, `find_action` and
  `cards_accounted_for` for checking results. Cards are only ever moved, never created, so every
  player always has exactly 60.
- **Fixtures** (in the engine tests): a struct holding the `CardDB`, `CardTable` and `GameState`
  in that order, because each points into the one before it. They're non-copyable on purpose.
- **`POKEBOT_SOURCE_DIR`**: a compile-time define with the repository path, so tests can read
  files like `data/decks/*.txt` no matter which folder they run from.

## Adding a test file

1. Create `tests/test_<name>.cpp` and include `<catch2/catch_test_macros.hpp>`.
2. Add it to `add_executable(pokebot_tests ...)` in [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt).
3. Give its tests a tag like `[name]` so the group can be run on its own.

`catch_discover_tests` registers each `TEST_CASE` with CTest automatically.
