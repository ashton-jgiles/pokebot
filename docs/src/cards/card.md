# `src/cards/card` — card definitions

**Files:** [`src/cards/card.hpp`](../../../src/cards/card.hpp), [`src/cards/card.cpp`](../../../src/cards/card.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

`CardDef` is the engine's description of one printed card: its stats, attacks, abilities and
rule-box flags. This file also defines how a `CardDef` is saved to and loaded from our card JSON
format (see [`docs/data/cards`](../../data/cards/README.md)).

A `CardDef` never changes during a game. Card *instances* in play (damage, attached Energy and
so on) will be part of the engine's game state in Milestone 2.

## Enums

| Enum | Values | JSON spelling |
|---|---|---|
| `EnergyType` | Grass, Fire, Water, Lightning, Psychic, Fighting, Darkness, Metal, Fairy, Dragon, Colorless | lowercase, e.g. `"darkness"` |
| `CardKind` | Pokemon, Trainer, Energy | `"pokemon"`, `"trainer"`, `"energy"` |
| `Stage` | None, Basic, Stage1, Stage2 | `"none"`, `"basic"`, `"stage1"`, `"stage2"` |
| `TrainerKind` | None, Item, Supporter, Stadium, Tool | lowercase |
| `EnergyKind` | None, Basic, Special | lowercase |
| `DamageMod` | None, Plus, Minus, Times | `"none"`, `"plus"`, `"minus"`, `"times"` |

`DamageMod` records the symbol printed after an attack's damage: `30+`, `120-` or `20×`. The
attack text says how the final damage is worked out.

> **Keep enums and name tables in sync.** Each enum's JSON names live in an array in
> `card.cpp` (`kEnergyTypes`, `kDamageMods`, …) and are looked up **by position**. If you add or
> reorder enum values, change the array the same way. The test
> "Times damage survives a save and load" catches the most likely mistake.

## Structs

### `Weakness` / `Resistance`

- `Weakness { type, multiplier = 2 }`: damage from `type` is multiplied (×2).
- `Resistance { type, amount = 30 }`: damage from `type` is reduced (−30).

### `Ability`

| Field | Meaning |
|---|---|
| `name` | Ability name |
| `text` | Printed text. Reference only; the engine never reads it |
| `effects` | Engine-readable effects (JSON array), hand-written in Milestone 3 |

### `Attack`

| Field | Meaning |
|---|---|
| `name` | Attack name |
| `cost` | Energy symbols required. Empty means the attack is free |
| `damage` | Printed base damage (0 if none) |
| `damage_mod` | `+`, `-` or `×` after the damage, if any |
| `text` | Printed text, reference only |
| `effects` | Engine-readable effects, hand-written in Milestone 3 |

### `CardRules`

Rule-box and deck-building flags, all `false` by default: `ex`, `mega`, `tera`, `ace_spec`, `radiant`.

### `CardDef`

| Field | Used by | Meaning |
|---|---|---|
| `id` | all | Our id: decklist set code + number, e.g. `"TWM-130"` |
| `tcgdex_id` | all | TCGdex's id, e.g. `"sv06-130"`. Reference only |
| `name` | all | Real card name |
| `kind` | all | Pokémon, Trainer or Energy |
| `rules` | all | Rule-box flags |
| `stage`, `evolves_from`, `hp`, `types`, `weakness`, `resistance`, `retreat`, `abilities`, `attacks` | Pokémon | Printed stats |
| `trainer_kind` | Trainer | Item / Supporter / Stadium / Tool |
| `energy_kind`, `provides` | Energy | Basic or Special; the types a Basic Energy provides |
| `text`, `effects` | Trainer, Energy | Printed text and engine-readable effects |

`prize_value()` returns the Prize cards taken when this Pokémon is Knocked Out: 1 normally,
2 for Pokémon ex, 3 for Mega Evolution Pokémon ex.

### `CardDataError`

Derives from `std::runtime_error`. Thrown for any missing, misspelled or wrongly typed card data.

## Functions

| Function | Behavior |
|---|---|
| `std::string_view to_string(EnergyType)` | `EnergyType::Darkness` → `"darkness"` |
| `EnergyType energy_type_from_string(std::string_view)` | `"darkness"` → `EnergyType::Darkness`. Throws `CardDataError` for unknown names |
| `void to_json(nlohmann::json&, const CardDef&)` | Writes our card JSON. Pokémon get the Pokémon fields; Trainers and Energy get `text` and `effects` |
| `void from_json(const nlohmann::json&, CardDef&)` | Reads our card JSON. Throws `CardDataError` on missing required fields, unknown enum names or wrong types (e.g. `"hp": "200"`) |

`to_json` / `from_json` follow nlohmann/json's naming convention, so these work directly:

```cpp
CardDef card = j.get<CardDef>();
nlohmann::json j = card;
```

Unknown enum strings are **errors**, never silently mapped to a default. A typo like
`"pokemonn"` fails loudly instead of loading a broken card.

## Tests

Covered by [`tests/test_card_db.cpp`](../../../tests/test_card_db.cpp) (round trips, bad data) and
[`tests/test_convert.cpp`](../../../tests/test_convert.cpp) (save and reload of damage modifiers).
