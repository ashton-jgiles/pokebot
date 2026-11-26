# `tools/import/main.cpp` — the `pokebot-import` command

**File:** [`tools/import/main.cpp`](../../../tools/import/main.cpp) · **Executable:** `pokebot-import`

Imports every card in one or more decklists: downloads them from TCGdex, converts them to our
format, and checks that the result loads.

## Usage

Run from the repository root; all paths are relative to it.

```bash
./build/debug/tools/import/pokebot-import.exe [--refresh] [--offline] <decklist.txt or directory>...
./build/debug/tools/import/pokebot-import.exe [--refresh] [--offline] --standard [--list-missing]
./build/debug/tools/import/pokebot-import.exe [--refresh] [--offline] --check-translations
```

A directory stands for every `.txt` decklist in it and its subdirectories, in sorted order (so `data/decks` includes
`data/decks/incoming`), so
`data/decks data/archetypes` imports and checks every deck and every [archetype list](../../data/archetypes/README.md).

| Option | Meaning |
|---|---|
| `--refresh` | Download every card again, ignoring the cache |
| `--offline` | Use only cached files; never touch the network |
| `--standard` | Instead of decklists: [the coverage report](#--standard-the-coverage-report) |
| `--list-missing` | With `--standard`: list every effect still missing |
| `--check-translations` | Instead of decklists: [check the translator](#--check-translations) |

Examples:

```bash
./build/debug/tools/import/pokebot-import.exe data/decks/dragapult.txt data/decks/ns-zoroark.txt
./build/debug/tools/import/pokebot-import.exe --offline data/decks/dragapult.txt
./build/debug/tools/import/pokebot-import.exe --offline data/decks data/archetypes
```

## What it does

For each decklist, for each card not already handled in this run (cards shared between decks
are processed once):

1. Fetch the card through [`TcgdexClient`](tcgdex.md) (from the cache when possible).
2. Compare TCGdex's name with the decklist's name and warn if they differ. That usually means a
   wrong set code or number. Energy symbols count as their names, so "Telepathic {P} Energy" matches
   "Telepathic Psychic Energy", and basic Energy is skipped, since decklists write it as `Basic {D} Energy`.
   A set code that isn't in `sets.json`, such as `PAF` for an older printing, is
   [found through TCGdex's series](tcgdex.md#set-codes).
3. Apply [`corrections.json`](../../data/cards/README.md#correctionsjson), convert it with [`convert_card`](convert.md) and write `data/cards/generated/<id>.json`.
4. Collect any review notes.

Then it prints the review notes and a summary, and [links reprints](standard.md#reprints): every card in
`generated/` without its own effects file gets `same_as` naming a card with the same rules that has one, so an
older printing such as Ultra Ball (PAF 91) plays with Ultra Ball (MEG 131)'s effects. It loads everything through
[`CardDB::load`](../../src/cards/db/card_db.md), exactly as the engine will. That checks
that every generated file and every effects file is valid, and counts what still needs effects. Then it
reports each decklist: how many of its cards are ready, what each one that isn't still needs, older printings
and the card whose effects they use, printings TCGdex doesn't mark Standard-legal, and, for a deck in
`data/decks/`, a missing archetype folder. Finally it checks each decklist with the [deck validator](../../src/cards/validator/deck_validator.md) and
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
2 printings use the effects of a reprint with the same rules
80 cards load cleanly; 0 attacks/abilities/cards still need effects

Decks:
  data/decks/alakazam.txt: 12 of 24 cards ready
    needs effects: MEG-56 Alakazam: attack Mind Ruler
    ...
    older printing: SVI-191 Rare Candy, the same card as MEG-125
  ...

Deck legality:
  legal    data/decks/basic-box.txt
  legal    data/decks/dragapult.txt
  ...
```

A card that fails is shown as `FAIL` with the reason, and the import continues with the next card.

What still needs effects is listed under each deck that uses it. A Special Energy's review note
is left out once its effects file sets `provides`, and a Tera note once it sets `rules`.

## `--standard`: the coverage report

Downloads every card of every set in `sets.json`, a set at a time with
[`set_cards`](tcgdex.md#whole-sets), and prints what [`standard_coverage`](standard.md) finds:

```
  30C  30th  191 cards
  ...
21 sets, 3594 printings, 25 network requests
Left out, not marked Standard-legal by TCGdex: 30C 30 DLR 141 ...
Left out, no card number: 3

2109 distinct Standard cards
            complete   missing  problems   total
Pokemon          ...
Trainer          ...
Energy           ...
All              ...
Complete: ...% of distinct cards
Pokemon ex whose Tera isn't checked yet: ...

Problems:
  ...
```

It writes nothing in `generated/`, so the decks' cards are untouched. With `--offline` it uses the
cached sets only. It always exits with 0 once the report is printed.

## `--check-translations`

Checks the [translator](translate.md) in two ways:

1. **Against what we wrote by hand.** For every attack with text on a card with an effects file, it translates the
   text and compares the compiled program with the hand-written one, printing `same`, `DIFFERENT` (with both effects
   lists) or `not read` (with the first sentence it couldn't read).
2. **On all of Standard.** It translates every distinct Standard attack text, from the
   [whole sets](tcgdex.md#whole-sets) (downloaded or cached), and prints how many it reads and the 25 sentence shapes,
   numbers written as `#`, that it most often can't.

```
Hand-written attacks:
  same       30C-21 Greninja ex: attack Stealthy Slash
  ...
40 hand-written attacks with text: 40 the same, 0 different, 0 not read

Standard attacks: ... of ... distinct texts read (...%)
The sentences most often not read, by shape:
    ...
```

It writes nothing, and exits with 1 if any hand-written attack comes out different.

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
| `data/cards/corrections.json` | read | yes |
| `data/cards/raw/bulk/` | read + write with `--standard` (cache) | no |
| `data/cards/raw/series/`, `data/cards/raw/set-codes.json` | read + write when a set code isn't in `sets.json` (cache) | no |
| `data/archetypes/` | checked for a folder for each deck | yes |
| `data/cards/raw/` | read + write (cache) | no |
| `data/cards/generated/` | write | no |
| `data/cards/effects/` | read | yes |

See [`docs/data`](../../data/cards/README.md) for the formats.
