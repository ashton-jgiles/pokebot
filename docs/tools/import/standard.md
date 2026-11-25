# `tools/import/standard` — coverage of Standard

**Files:** [`tools/import/standard.hpp`](../../../tools/import/standard.hpp), [`tools/import/standard.cpp`](../../../tools/import/standard.cpp)
**Library:** `pokebot_import_lib` · **Namespace:** `pokebot::import`

Sorts every card of every set into distinct Standard cards and says which have all their effects. It's how M3b
measures its progress: `pokebot-import --standard` prints the result (see [the command](main.md#--standard-the-coverage-report)).

```cpp
const Coverage coverage = standard_coverage({{"TWM", tcgdex.set_cards("TWM")}}, corrections, "data/cards/effects");
```

## What counts

- **Standard-legal** means TCGdex marks the printing `legal.standard`. Delta Reign isn't marked yet, so it's left out and counted
  in `not_legal`, as are the classic reprints of 30th Celebration and old promos.
- **One card, many printings:** printings with the same name, kind, stage, HP, card text, Abilities and attacks (name, damage
  and text) are one card. HTML tags and spacing in the text don't count, so `Draw a <em>card</em>.` is `Draw a card.`
- **A printing needs a number:** its id is the set code and its number, as decklists write it (`TWM-130`, `MEE-7`). A card with
  a local id that isn't a number (30th Celebration's three Mew, `B`, `G` and `R`) is counted in `unnumbered` and left out.
- **Complete** means no problems and nothing in [`missing_effects`](../../src/cards/db/card_db.md#missing_effects). The effects
  come from the first of the card's printings that has an [effects file](../../data/cards/README.md#effectsidjson--hand-written-effects),
  so effects written for one printing count for its reprints.
- **A problem** is data that keeps the card from being played as printed:
  - the card can't be converted (an unknown stage, an unreadable number);
  - an evolution that doesn't say what it evolves from, or a Trainer with no text (`Conversion::problems`);
  - a Basic that shares its name with an evolution, which is how TCGdex's wrong stages showed up;
  - effects that don't compile.
  [`corrections.json`](../../data/cards/README.md#correctionsjson) is applied first, so a correction makes a problem go away.
- **Tera not checked:** a non-Mega Pokémon ex whose effects file doesn't set `rules`. It doesn't stop the card counting as
  complete, since TCGdex can't say whether it's Tera.

## Types and functions

| Name | What it is |
|---|---|
| `StandardCard` | One distinct card: its `CardDef` (with effects merged), its `printings`, `problems`, `missing` effects, the printing its effects came from (`effects_from`), and `tera_unchecked` |
| `Coverage` | `printings` read, `unnumbered`, `not_legal` (by set code), and the `cards` in the order first seen |
| `printing_id(set_code, local_id)` | `"TWM"`, `"130"` → `"TWM-130"`; `""` if the local id isn't a positive number |
| `card_signature(card)` | The text that decides which printings are one card |
| `standard_coverage(sets, corrections, effects_dir)` | Converts every legal, numbered printing of the sets (pairs of set code and TCGdex card list), groups them, and checks each card |

Problems are found before effects are checked; a card with a problem has no `missing` list.

## Tests

[`tests/test_standard.cpp`](../../../tests/test_standard.cpp), tag `[standard]`, with made-up cards and a temporary effects folder:
printing ids; reprints grouped whatever their tags and spacing, and printings that aren't legal or numbered left out; effects
from another printing, missing effects listed, and Tera not checked; and missing or contradictory data found, then fixed by
corrections.
