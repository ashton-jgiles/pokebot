# `src/core/decklist` — decklist parser

**Files:** [`src/core/decklist.hpp`](../../../src/core/decklist.hpp), [`src/core/decklist.cpp`](../../../src/core/decklist.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Reads a decklist in the Pokémon TCG Live text export format and turns it into a list of
`DeckEntry` values. Both the importer and (later) the engine use it, which is why it lives in
the core library rather than in the import tool.

## Input format

```
Pokémon: 9
4 Dreepy TWM 128
3 Dragapult ex TWM 130

Trainer: 12
4 Lillie's Determination MEG 119

Energy: 3
3 Basic {D} Energy MEE 7

Total Cards: 60
```

| Line | Handling |
|---|---|
| Starts with a digit | A card line: `<count> <name> <SET> <number>` |
| `Total Cards: N` | Remembered; the card counts must add up to `N` |
| Anything else (`Pokémon: 9`, blank lines) | Skipped |

Card lines are read **from the right**. The last token is the card number and the one before it is
the set code, so everything in between is the name. Names can contain spaces, apostrophes, `é`
and `{D}` without any special handling. Windows line endings (`\r\n`) are fine.

## Types

### `DeckEntry`

| Field | Example | Meaning |
|---|---|---|
| `count` | `4` | Copies in the deck |
| `name` | `"Lillie's Determination"` | Name exactly as written in the decklist |
| `set_code` | `"MEG"` | Set code as written in the decklist |
| `number` | `119` | Card number in that set |

The decklist name isn't always the card's real name. Basic Energy is written
`Basic {D} Energy` in decklists but is called `Darkness Energy` on the card. Use the card
database (via `card_id(entry)`) for the real name.

### `Decklist`

- `entries`: the card lines, in file order.
- `total_cards()`: the sum of all `count` values.

### `DecklistError`

Derives from `std::runtime_error`. The message starts with `line N:`, and `line()` returns `N`.

## Functions

| Function | Behavior |
|---|---|
| `Decklist parse_decklist(std::istream& in)` | Parses from any stream. Throws `DecklistError`. |
| `Decklist load_decklist(const std::filesystem::path& path)` | Opens the file and parses it. Throws `std::runtime_error` if the file can't be opened, `DecklistError` if its contents are bad. |

## Errors detected

| Problem | Example |
|---|---|
| Fewer than 4 tokens on a card line | `4 Dreepy 128` |
| Count missing, zero or not a number | `0 Dreepy TWM 128` |
| Card number not a positive whole number | `4 Dreepy TWM 12a` |
| Set code not 1–6 uppercase letters/digits | `4 Drakloak twm 129` |
| `Total Cards` line that isn't a number | `Total Cards: sixty` |
| Counts don't add up to `Total Cards` | 59 cards with `Total Cards: 60` |

The parser checks the **format** only. Deck-building rules (exactly 60 cards, at most 4 copies,
ACE SPEC limit) are checked by the [deck validator](../cards/validator/deck_validator.md).

## Tests

[`tests/test_decklist.cpp`](../../../tests/test_decklist.cpp), tag `[decklist]`. Also checks that
both project decklists in `data/decks/` load with 60 cards each.
