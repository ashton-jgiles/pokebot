# `tools/import/main.cpp` — the `pokebot-import` command

**File:** [`tools/import/main.cpp`](../../../tools/import/main.cpp) · **Executable:** `pokebot-import`

Imports every card in one or more decklists: downloads them from TCGdex, converts them to our
format, and checks that the result loads.

## Usage

Run from the repository root; all paths are relative to it.

```bash
./build/debug/tools/import/pokebot-import.exe [--refresh] [--offline] <decklist.txt>...
```

| Option | Meaning |
|---|---|
| `--refresh` | Download every card again, ignoring the cache |
| `--offline` | Use only cached files; never touch the network |

Examples:

```bash
./build/debug/tools/import/pokebot-import.exe data/decks/dragapult.txt data/decks/ns-zoroark.txt
./build/debug/tools/import/pokebot-import.exe --offline data/decks/dragapult.txt
```

## What it does

For each decklist, for each card not already handled in this run (cards shared between decks
are processed once):

1. Fetch the card through [`TcgdexClient`](tcgdex.md) (from the cache when possible).
2. Compare TCGdex's name with the decklist's name and warn if they differ. That usually means a
   wrong set code or number. Basic Energy is skipped, since decklists write it as `Basic {D} Energy`.
3. Convert it with [`convert_card`](convert.md) and write `data/cards/generated/<id>.json`.
4. Collect any review notes.

Then it prints the review notes, a summary, loads everything through
[`CardDB::load`](../../src/cards/db/card_db.md), exactly as the engine will. That last step checks
that every generated file and every effects file is valid, and counts what still needs effects.
Finally it checks each decklist with the [deck validator](../../src/cards/validator/deck_validator.md) and
prints `legal`, or `ILLEGAL` followed by every problem.

## Output

```
data/decks/dragapult.txt
  ok    TWM 128  Dreepy
  ok    TWM 130  Dragapult ex
  ...
Needs review:
  XYZ-12 Newmon ex: Pokemon ex: check whether it's Tera, then add effects/XYZ-12.json with its rules
  ...

80 distinct cards, 0 network requests, 0 failures, 0 name mismatches
80 cards load cleanly; 0 attacks/abilities/cards still need hand-written effects

Deck legality:
  legal    data/decks/basic-box.txt
  legal    data/decks/dragapult.txt
  ...
```

A card that fails is shown as `FAIL` with the reason, and the import continues with the next card.

Everything still needing effects is listed by name under the count. A Special Energy's review note
is left out once its effects file sets `provides`, and a Tera note once it sets `rules`.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Every card imported and every deck is legal |
| 1 | A card failed, a deck is illegal, or loading the card database failed |
| 2 | Bad command-line arguments (usage is printed) |

## Files it reads and writes

| Path | Read / write | In git? |
|---|---|---|
| `data/decks/*.txt` | read | yes |
| `data/cards/sets.json` | read | yes |
| `data/cards/raw/` | read + write (cache) | no |
| `data/cards/generated/` | write | no |
| `data/cards/effects/` | read | yes |

See [`docs/data`](../../data/cards/README.md) for the formats.
