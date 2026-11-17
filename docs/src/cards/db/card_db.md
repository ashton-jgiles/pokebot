# `src/cards/db/card_db` — card database

**Files:** [`src/cards/db/card_db.hpp`](../../../../src/cards/db/card_db.hpp), [`src/cards/db/card_db.cpp`](../../../../src/cards/db/card_db.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Loads every card the project knows about and looks cards up by id. When loading, it merges each
generated card with its hand-written effects file, so the engine sees one complete `CardDef`
per card.

```
data/cards/generated/TWM-130.json  ─┐
                                     ├─ merge_effects ─► CardDef "TWM-130"
data/cards/effects/TWM-130.json    ─┘   (if it exists)
```

## Functions

### `std::string card_id(const DeckEntry& entry)`

The id a decklist line refers to: `{set_code: "TWM", number: 130}` → `"TWM-130"`.

### `nlohmann::json merge_effects(nlohmann::json generated, const nlohmann::json& overrides)`

Applies an effects file on top of a generated card, both as JSON:

| Key in the effects file | What happens |
|---|---|
| `"attacks"` | An object keyed by attack name. Each value is merged into the attack with that name, replacing keys it mentions (e.g. `effects`) and keeping the rest (e.g. `text`) |
| `"abilities"` | Same, for abilities |
| Any other key | Replaces the generated value entirely, e.g. `"rules": ["ex", "tera"]` |

Naming an attack or ability that doesn't exist throws `CardDataError`, so a typo in an effects
file is caught instead of silently ignored.

## `class CardDB`

| Member | Behavior |
|---|---|
| `static CardDB load(generated_dir, effects_dir)` | Reads every `*.json` in `generated_dir`, merges `effects_dir/<same name>` when it exists, and parses the result into a `CardDef` |
| `void add(CardDef card)` | Compiles each attack's effects into its `program`, an Item's or Supporter's effects into the card's `program`, `requirements` into `conditions`, and every `passive` list into `modifiers`, then adds or replaces the card. A bad effect throws `CardDataError` naming the card and attack. `load` uses it too |
| `const CardDef* find(std::string_view id) const` | The card, or `nullptr` |
| `const CardDef& at(std::string_view id) const` | The card; throws `CardDataError` if unknown |
| `std::size_t size() const` | Number of cards |
| `std::vector<std::string> missing_effects() const` | Everything with printed text but an empty `effects` list |

`load` throws `CardDataError` when:
- a file isn't valid JSON or isn't valid card data. The message starts with the file name;
- an effects file has **no matching generated card**, which almost always means a typo in its file name.

If `generated_dir` doesn't exist, `load` fails with the standard library's filesystem error.
Run `pokebot-import` first.

### `missing_effects()`

A card, Ability or attack counts as written when it has `effects` or a `passive` list.

Returns lines like:

```
TWM-130 Dragapult ex: attack Phantom Dive
POR-62 Meowth ex: ability Last-Ditch Catch
MEG-131 Ultra Ball
```

It's the Milestone 3 to-do list, generated from the data. An attack with no printed text
(plain damage, like Dragapult ex's Jet Headbutt) needs no effects and isn't listed.

Cards are stored in a `std::map` with `std::less<>`, so lookups work directly with a
`std::string_view` without building a temporary `std::string`.

## Tests

[`tests/test_card_db.cpp`](../../../../tests/test_card_db.cpp), tag `[card_db]`.
