# `src/cards/effects/program` — the effect language

**Files:** [`program.hpp`](../../../../src/cards/effects/program.hpp), [`program.cpp`](../../../../src/cards/effects/program.cpp) · **Library:** `pokebot_lib` · **Namespace:** `pokebot`

Card effects are written as data in [effects files](../../../data/cards/README.md#effectsidjson--hand-written-effects)
and **compiled** into a flat list of simple steps, a `Program`. The engine's
[interpreter](../../engine/effects/interpreter.md) runs the steps. Compiling happens once, when a
card is added to the `CardDB`, so a mistake in an effects file stops the load with the card and
attack named, instead of turning up in the middle of a game.

## Building blocks (what effects files say)

Each block is a JSON object with an `"op"`:

| `op` | Fields | Means | Compiles to |
|---|---|---|---|
| `damage` | — | The attack's damage, at this point in the list | `Damage` |
| `draw` | `count` (1) | Draw cards | `Draw` |
| `discard_hand` | — | Discard your hand | `DiscardHand` |
| `shuffle_deck` | — | Shuffle your deck | `ShuffleDeck` |
| `search_deck` | `count` (1), `filter`, `to` (`"hand"`) | Search your deck for **up to** `count` matching cards, then shuffle | `ChooseCards`, `MoveChosen`, `ShuffleDeck` |
| `move_attached_energy` | `count` (1), `filter`, `to` (`"hand"`) | Move exactly `count` Energy attached to this Pokémon (fewer if it has fewer) | `ChooseCards`, `MoveChosen` |
| `switch_self` | — | Switch this Pokémon with 1 of your Benched Pokémon | `ChooseBenched`, `SwitchWithChosen` |
| `may` | `effects` (a list) | "You may …": ask first, and skip the list on no | `AskYesNo`, then the list |

`to` can be `"hand"`, `"discard"` or `"deck"`. `count` must be 0 to 60.

**Damage comes first** unless the list places a `damage` block itself, since most attack text
describes what happens after the damage. A list that needs a different order (discard Energy, then
do damage) puts `damage` where it belongs.

### Filters

| Field | Matches |
|---|---|
| `kind` | `"pokemon"`, `"trainer"` or `"energy"` |
| `type` | A Pokémon of that type, or an Energy that provides it (`"darkness"`, …) |
| `basic` | `true`: a Basic Pokémon or a Basic Energy |
| `name` | That exact card name |

Every field given must match; `{}` (or no filter) matches any card.

Anything the compiler doesn't recognise (an unknown op, filter field or destination, a bad count,
a value of the wrong JSON type) throws `CardDataError`.

## Steps (what the engine runs)

| `Op` | Uses | Does |
|---|---|---|
| `Damage` | — | The attack's damage to the opponent's Active Pokémon |
| `Draw` | `max` | Draws `max` cards |
| `DiscardHand`, `ShuffleDeck` | — | As named |
| `ChooseCards` | `from`, `filter`, `min`, `max` | The player chooses `min` to `max` matching cards |
| `MoveChosen` | `from`, `to` | Moves the chosen cards |
| `ChooseBenched` | — | The player chooses 1 of their Benched Pokémon |
| `SwitchWithChosen` | — | "This Pokémon" switches with the chosen one |
| `AskYesNo` | `jump` | On no, carries on at step `jump` |

A `Step` holds all of those fields; each op uses the ones it needs. A program can have at most 255
steps.

## Functions

| Function | Behavior |
|---|---|
| `Program compile_attack(const json& effects)` | Compiles an attack's effects list |
| `bool matches(const CardFilter&, const CardDef&)` | Whether a card passes a filter |
| `const char* to_string(Op)` | A step's name, for printing |

The enums cards use (`CardKind`, `EnergyType`, …) live in
[`card_types.hpp`](../card/card.md#enums), so this header and `card.hpp` don't include each other.

## Tests

[`tests/test_effect_program.cpp`](../../../../tests/test_effect_program.cpp), tag `[effects]`: an
attack with no effects, how `search_deck` and `may` expand, a placed `damage` step, every kind of
mistake rejected, filters, and `CardDB::add` naming the attack in its error.
