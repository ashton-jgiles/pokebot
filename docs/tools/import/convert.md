# `tools/import/convert` — TCGdex → our card format

**Files:** [`tools/import/convert.hpp`](../../../tools/import/convert.hpp), [`tools/import/convert.cpp`](../../../tools/import/convert.cpp)
**Library:** `pokebot_import_lib` · **Namespace:** `pokebot::import`

Turns a card object from TCGdex into our [`CardDef`](../../src/cards/card/card.md). This is the only
code that knows TCGdex's field names. The rest of the project only sees `CardDef`.

## Function

```cpp
Conversion convert_card(const nlohmann::json& tcgdex, const std::string& our_id);
```

- `our_id` is the decklist-style id, e.g. `"TWM-130"`.
- Returns a `Conversion`: the `CardDef`, `notes` (things a person should check by hand), and
  `tera_unknown`, which is true for a non-Mega Pokémon ex because TCGdex doesn't say whether it's Tera.
  Mega ex have their own rule box, so they're never Tera.
- Throws `CardDataError` (message prefixed with the id) for any value it doesn't understand.

## Field mapping

| TCGdex | `CardDef` | Notes |
|---|---|---|
| `id` | `tcgdex_id` | |
| `name` | `name` | |
| `category` | `kind` | `Pokemon` / `Trainer` / `Energy` |
| `stage` | `stage` | `Basic`, `Stage1`, `Stage2` |
| `evolveFrom` | `evolves_from` | |
| `hp`, `retreat` | same | `retreat` defaults to 0 |
| `types` | `types` | `"Darkness"` → `EnergyType::Darkness` |
| `weaknesses[0]` | `weakness` | value `"×2"` → multiplier 2 |
| `resistances[0]` | `resistance` | value `"-30"` → amount 30 |
| `abilities[]` | `abilities` | `effect` text → `text` |
| `attacks[].cost` | `cost` | missing → free attack |
| `attacks[].damage` | `damage` + `damage_mod` | `70` → 70; `"20×"` → 20 Times; `"30+"` → 30 Plus; `"120-"` → 120 Minus |
| `attacks[].effect` | `text` | |
| `suffix: "ex"` | `rules.ex` | |
| name starts with `Mega ` (and ex) | `rules.mega` | 3 Prize cards |
| `trainerType` | `trainer_kind` | `Item`, `Supporter`, `Stadium`, `Tool` |
| `rarity: "ACE SPEC Rare"` | `rules.ace_spec` | Trainers and Special Energy |
| `energyType: "Normal"`, no `effect` text | `energy_kind = Basic` | type from the name: `"Darkness Energy"` → Darkness |
| `energyType: "Special"` | `energy_kind = Special` | |
| `energyType: "Normal"` **with** `effect` text | `energy_kind = Special` | TCGdex mislabels some Special Energy (e.g. Ignition Energy, WHT 86); rules text wins |
| `effect` (Trainer / Special Energy) | `text` | |

TCGdex fields not listed (prices, images, illustrator, set details) are ignored.

## What it can't know: notes

TCGdex doesn't record rule boxes, so some things need a person to check:

| Note | When |
|---|---|
| *(via `tera_unknown`)* | Every Pokémon ex **except Mega** ex. `Conversion::tera_unknown` is set, and `pokebot-import` shows a reminder only if the card has no effects file with `"rules"` yet |
| "Special Energy: fill in what it provides" | Every Special Energy |
| "TCGdex lists this as basic Energy, but it has rules text" | A Special Energy that TCGdex labels `Normal` |
| "has more than one Weakness; only the first is kept" | Rare older cards |

The importer prints these under **Needs review**.

## Rejected on purpose

These throw `CardDataError` instead of guessing:

- Stages other than Basic / Stage1 / Stage2 (e.g. VMAX, BREAK).
- Ability types other than `Ability` (e.g. older Poké-Powers).
- Trainer, Energy or card categories it doesn't know.
- Unreadable numbers (damage `"lots"`, weakness `"×two"`).
- A Basic Energy whose name doesn't end in ` Energy`.
- Any missing required field or wrong JSON type (reported as "unexpected TCGdex data").

When a new deck hits one of these, the error says exactly what to support next.

## Tests

[`tests/test_convert.cpp`](../../../tests/test_convert.cpp), tag `[convert]`. It uses small
made-up TCGdex cards: an ex with `×` damage and a free attack, an ACE SPEC Trainer, a Basic
Energy, data it must reject, and a save-and-reload check for damage modifiers.
