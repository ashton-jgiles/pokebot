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
./build/debug/tools/import/pokebot-import.exe [--refresh] [--offline] --audit N [--seed S] [--using WORD,...]
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
| `--audit N` | Instead of decklists: [print N translated texts](#--audit-a-sample-of-translations), chosen at random |
| `--seed S` | With `--audit`: the seed of the choice (default 1) |
| `--using WORD,...` | With `--audit`: [only texts whose effects use one of the words](#--audit-a-sample-of-translations) |

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
older printing such as Ultra Ball (PAF 91) plays with Ultra Ball (MEG 131)'s effects. Then it
[stores translations](standard.md#translations): every card in `generated/` with neither an effects file nor `same_as`
gets what the [translator](translate.md) reads of it. It loads everything through
[`CardDB::load`](../../src/cards/db/card_db.md), exactly as the engine will. That checks
that every generated file and every effects file is valid, and counts what still needs effects. Then it
reports each decklist: how many of its cards are ready and how many of those by translation, which cards are translated,
what each one that isn't still needs, older printings
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
14 cards without effects have a translation, 9 of them with every text read
80 cards load cleanly; 0 attacks/abilities/cards still need effects

Decks:
  data/decks/alakazam.txt: 12 of 24 cards ready, 3 of them by translation
    translated: MEG-55 Kadabra
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

What still needs effects is listed under each deck that uses it. A translation that doesn't load (which would mean a
translator bug) is printed under the translations line and not stored. A Special Energy's review note
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
Complete by translation, with no effects file: ...
Pokemon ex whose Tera isn't checked yet: ...

Problems:
  ...
```

A card with no effects file in any printing is checked with what the [translator](translate.md) reads of it, so
**complete** counts translated cards; the second line says how many of them. It writes nothing in `generated/`, so the
decks' cards are untouched. With `--offline` it uses the
cached sets only. It always exits with 0 once the report is printed.

## `--check-translations`

Checks the [translator](translate.md) in two ways:

1. **Against what we wrote by hand.** For every card with an effects file, it translates every attack, Ability and card
   text, loads the card with what it read, and compares it with the hand-written card, part by part: `same`,
   `DIFFERENT` (with what it translated) or `not read` (with the first sentence it couldn't read).
2. **On all of Standard.** It translates every distinct Standard card, from the [whole sets](tcgdex.md#whole-sets)
   (downloaded or cached), and prints how many have every text read, how many texts of each kind it reads, and the 30
   sentence shapes, numbers written as `#`, that it most often can't.
3. **What would complete the most cards.** For each card with no effects file and exactly one text not read, it takes
   that text's first unread sentence, and prints the 40 commonest shapes. Each count is the most cards reading that
   sentence could complete; fewer if a later sentence of the same text can't be read either.

```
Hand-written cards, each attack, Ability and card text:
  same       30C-21 Greninja ex: attack Stealthy Slash
  ...
... hand-written texts: ... the same, 0 different, ... not read

Standard: 2097 distinct cards, ... with every text read (...%, counting cards with no text)
  ability    ... of   ... texts read (...%)
  attack     ...
The sentences most often not read, by shape:
    ...

... cards without an effects file have exactly one text not read. The sentence that stops each, by shape:
    ...
```

It writes nothing, and exits with 1 if any hand-written text comes out different.

## `--audit`: a sample of translations

Reads the Standard sets as `--standard` does, collects every translated text of the cards complete or not that no printing
has an effects file for (each attack, Ability and card text on its own), and prints `N` of them, chosen without repeats
by the engine's `Rng` seeded with `S`, so the same seed on the same data gives the same sample. Each one is its card and
part, its text as printed, and the effects it reads as.

`--using` keeps only the texts whose effects use one of a comma-separated list of words. The words of an effects list
are every op (`heal_to`), every key (`opponent_active_is`, `each`), every key with its value (`each:true`, `on:active`,
`of:in_play`), and `damage_inside_if` for an `if` whose `then` does the attack's damage ("if tails, this attack does
nothing"). The words are matched whole, so `each` doesn't match `for_each`.

```
Audit: 50 of ... translated texts of Standard cards without an effects file, seed 1

1. SVI-12 Testmon: attack Hit
   text:    Draw a card.
   effects: {"effects":[{"op":"draw"}]}
...
```

M3b's audits are run with it and checked by hand, each text against its effects. It writes nothing and exits with 0.

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
| `data/cards/generated/` | write (cards, `same_as` and `translation`) | no |
| `data/cards/effects/` | read | yes |

See [`docs/data`](../../data/cards/README.md) for the formats.
