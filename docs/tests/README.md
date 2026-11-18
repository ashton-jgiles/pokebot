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
| [`test_card_db.cpp`](../../tests/test_card_db.cpp) | `[card_db]` | 5 | JSON round trip, effects files adding rules and attack effects, typo'd attack names rejected, `missing_effects` list, bad card data rejected |
| [`test_engine_state.cpp`](../../tests/test_engine_state.cpp) | `[engine]` | 9 | `FixedVector`, `Rng` determinism and shuffles, independent `GameState` copies, card ownership, `CardTable` numbering and illegal decks |
| [`test_engine_setup.cpp`](../../tests/test_engine_setup.cpp) | `[setup]` | 9 | Opening hands, determinism, turn order, Prize cards and turn 1, merged choices and the Bench limit, mulligan bonus draws, 1,000 random real-deck setups, illegal actions |
| [`test_engine_turns.cpp`](../../tests/test_engine_turns.cpp) | `[turns]` | 5 | Ending the turn, drawing, alternating turns, flag resets, deck-out |
| [`test_engine_actions.cpp`](../../tests/test_engine_actions.cpp) | `[actions]` | 10 | Benching, one Energy per turn, evolution timing, retreating (with and without a choice, free, not allowed), 200 random real-deck games with a summary of how they ended |
| [`test_engine_attacks.cpp`](../../tests/test_engine_attacks.cpp) | `[attacks]` | 11 | Cost matching, no attack on turn 1, damage, Weakness and Resistance, copied attacks using the attacker's type, Pokémon ids, Knock Outs and promotion, ex / Mega ex Prize cards, every way to win including draws |
| [`test_rules.cpp`](../../tests/test_rules.cpp) | `[rules]` | 4 | Rulebook answers for HP and Knock Outs, type, costs, Energy, Prize cards, turn limits and evolution timing |
| [`test_engine_events.cpp`](../../tests/test_engine_events.cpp) | `[events]` | 5 | Events of an attack that Knocks Out (in order), of benching, evolving, attaching and retreating; history of the last two turns; the extra-attack decision |
| [`test_effect_program.cpp`](../../tests/test_effect_program.cpp) | `[effects]` | 12 | Compiling effects: damage first, `search_deck`, `may` and `if` expansion, placed damage, Trainer programs, conditions, `special_condition`, rejected mistakes, every filter field, errors naming the attack |
| [`test_engine_effects.cpp`](../../tests/test_engine_effects.cpp) | `[effects]` | 10 | Running effects: draw, search, filters, choosing nothing, yes/no, Energy to hand, switching, discard and draw, Knock Outs waiting for the effect, copying a game mid-effect |
| [`test_engine_trainers.cpp`](../../tests/test_engine_trainers.cpp) | `[trainers]` | 12 | Items resolving and going to the discard pile, one Supporter per turn and none on turn 1, Stadiums replacing each other, one Tool per Pokémon, `"requires"` and unwritten cards, Ultra Ball, Boss's Orders, Poffin to the Bench, `if`, Judge, Night Stretcher's filter |
| [`test_engine_conditions.cpp`](../../tests/test_engine_conditions.cpp) | `[conditions]` | 11 | Asleep and Paralyzed blocking attacks and retreats, an attack applying Asleep, which conditions replace or stack, Active only, retreating and evolving clearing them, Poison and Burn counters, a Poison Knock Out, recovering on heads, Paralysis timing, confusion failing on tails |
| [`test_engine_damage.cpp`](../../tests/test_engine_damage.cpp) | `[damage]` | 11 | Damage formulas: × per card in hand, + per Benched Pokémon, flip until tails, an "if you do" bonus, discarding Energy from several Pokémon, × per damage counter; Red Card; Cipher's deck order; the compile rules for formulas |
| [`test_engine_targets.cpp`](../../tests/test_engine_targets.cpp) | `[targets]` | 10 | Weakness on the Active but not the Bench, Tera Bench protection, two different targets, the damage-counter spread order, damage by the target's counters, Flamebody Cannon, Tuck Tail and promoting, copying an N's attack (and with nothing to copy), Crushing Hammer into the owner's discard pile |
| [`test_engine_lasting.cpp`](../../tests/test_engine_lasting.cpp) | `[lasting]` | 9 | "Can't use that attack" and "can't attack" next turn, retreating ending an effect, "can't retreat" for one turn, the Item lock, a damage bonus only against an Active ex, this-turn effects ending, attack Knock Outs versus Checkup ones in history, the compile checks |
| [`test_engine_passive.cpp`](../../tests/test_engine_passive.cpp) | `[passive]` | 7 | A Tool changing only its holder, a Stadium reaching both players' N's Pokémon, Skyliner on your Basic Pokémon only, Fairy Zone adding Weakness and Shred ignoring it, Binding Mochi only while Poisoned, passive lists counting as written, the compile checks |
| [`test_engine_abilities.cpp`](../../tests/test_engine_abilities.cpp) | `[abilities]` | 10 | Once per turn and again next turn, Active only, one per name, requirements and costs, damage counters Knocking Out at once, the top 2 cards, shuffling the Active away, moving counters, a filtered switch that Poisons, a Stadium action once per turn |
| [`test_engine_triggers.cpp`](../../tests/test_engine_triggers.cpp) | `[triggers]` | 8 | A trigger asking yes/no then searching, not offered as an Ability to use, one per name, unmet requirements, discarding the Stadium to its owner, switching in and moving Energy, Risky Ruins on Basic non-{D} Pokémon (Poffin too), Poffin not counting as "played", trigger names |
| [`test_invariants.cpp`](../../tests/test_invariants.cpp) | `[invariants]` | 5 | The checker catches duplicated, missing and misplaced cards, missed Knock Outs, bad evolutions and Energy, and a missing Active |
| [`test_agents.cpp`](../../tests/test_agents.cpp) | `[agents]` | 3 | Random choices stay legal and cover every option, the biased agent attacks, seeds repeat |
| [`test_stress.cpp`](../../tests/test_stress.cpp) | `[stress]` | 3 | 1,000 biased and 1,000 uniform random real-deck games with invariant checks, and every state and action can be described. More than 300 of the biased games must end by Knock Out (random players burn through their decks with draw and search Trainers, so deck-outs are common) |

**194 tests in total.** Tests that need the real card data skip themselves if `data/cards/generated/`
doesn't exist yet; run `pokebot-import` first.

## Helpers worth knowing

- **`FakeHttp`** (`test_tcgdex.cpp`): queue responses per URL with `queue(path, body)`. An empty
  body simulates TCGdex's "200 with empty body" glitch; `"!error"` simulates a failed request.
  `calls` counts requests, which is how the cache tests prove nothing was downloaded.
- **`TempDir`** (`test_tcgdex.cpp`): a fresh temporary folder with a `sets.json`, deleted when the
  test ends.
- **`support/test_cards.hpp`**: build made-up cards and decklists: `basic_pokemon`, `evolution`,
  `basic_energy`, `attack`, and `deck({{count, number}, ...})` for set `TST`.
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
