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
| [`test_effect_program.cpp`](../../tests/test_effect_program.cpp) | `[effects]` | 7 | Compiling effects: damage first, `search_deck` and `may` expansion, placed damage, rejected mistakes, filters, errors naming the attack |
| [`test_engine_effects.cpp`](../../tests/test_engine_effects.cpp) | `[effects]` | 10 | Running effects: draw, search, filters, choosing nothing, yes/no, Energy to hand, switching, discard and draw, Knock Outs waiting for the effect, copying a game mid-effect |
| [`test_invariants.cpp`](../../tests/test_invariants.cpp) | `[invariants]` | 5 | The checker catches duplicated, missing and misplaced cards, missed Knock Outs, bad evolutions and Energy, and a missing Active |
| [`test_agents.cpp`](../../tests/test_agents.cpp) | `[agents]` | 3 | Random choices stay legal and cover every option, the biased agent attacks, seeds repeat |
| [`test_stress.cpp`](../../tests/test_stress.cpp) | `[stress]` | 3 | 1,000 biased and 1,000 uniform random real-deck games with invariant checks, and every state and action can be described |

**112 tests in total.** Tests that need the real card data skip themselves if `data/cards/generated/`
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
